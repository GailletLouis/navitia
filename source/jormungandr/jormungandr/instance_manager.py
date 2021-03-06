# coding=utf-8

#  Copyright (c) 2001-2014, Canal TP and/or its affiliates. All rights reserved.
#
# This file is part of Navitia,
#     the software to build cool stuff with public transport.
#
# Hope you'll enjoy and contribute to this project,
#     powered by Canal TP (www.canaltp.fr).
# Help us simplify mobility and open public transport:
#     a non ending quest to the responsive locomotion way of traveling!
#
# LICENCE: This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#
# Stay tuned using
# twitter @navitia
# IRC #navitia on freenode
# https://groups.google.com/d/forum/navitia
# www.navitia.io
from __future__ import absolute_import, print_function, unicode_literals, division
from flask import json

from shapely import geometry
from zmq import green as zmq
from navitiacommon import type_pb2, request_pb2
import glob
import logging
from jormungandr.protobuf_to_dict import protobuf_to_dict
from jormungandr.exceptions import ApiNotFound, RegionNotFound,\
    DeadSocketException, InvalidArguments
from jormungandr import authentication, cache, app
from jormungandr.instance import Instance
import gevent
import os

def instances_comparator(instance1, instance2):
    """
    compare the instances for journey computation

    we want first the non free instances then the free ones following by priority
    """
    #Here we choose the instance with greater priority.
    if instance1.priority != instance2.priority:
        return instance2.priority - instance1.priority

    if instance1.is_free != instance2.is_free:
        return instance1.is_free - instance2.is_free

    # TODO choose the smallest region ?
    # take the origin/destination coords into account and choose the region with the center nearest to those coords ?
    return 1


def choose_best_instance(instances):
    """
    get the best instance in term of the instances_comparator
    """
    best = None
    for i in instances:
        if not best or instances_comparator(i, best) > 0:
            best = i
    return best

class InstanceManager(object):

    """
    Handle the different Kraken instances

    a kraken instance's id is associated to a zmq socket and possibly some custom configuration
    """

    def __init__(self, instances_dir=None, instance_filename_pattern='*.json', start_ping=False):
        # loads all json files found in 'instances_dir' that matches 'instance_filename_pattern'
        self.configuration_files = glob.glob(instances_dir + '/' + instance_filename_pattern) if instances_dir else []
        self.start_ping = start_ping
        self.instances = {}
        self.context = zmq.Context()

    def __repr__(self):
        return '<InstanceManager>'

    def register_instance(self, config):
        logging.getLogger(__name__).debug("instance configuration: %s", config)
        name = config['key']
        instance = Instance(self.context, name, config['zmq_socket'],
                            config.get('street_network'),
                            config.get('ridesharing'),
                            config.get('realtime_proxies', []),
                            config.get('zmq_socket_type', 'persistent'),
                            config.get('default_autocomplete', 'kraken'))
        self.instances[instance.name] = instance

    def initialisation(self):
        """ Charge la configuration à partir d'un fichier ini indiquant
            les chemins des fichiers contenant :
            - géométries de la région sur laquelle s'applique le moteur
            - la socket pour chaque identifiant navitia
        """

        self.instances.clear()
        for key, value in os.environ.items():
            if key.startswith('JORMUNGANDR_INSTANCE_'):
                logging.getLogger(__name__).info("Initialisation, reading: %s", key)
                config_data = json.loads(value)
                self.register_instance(config_data)

        for file_name in self.configuration_files:
            logging.getLogger(__name__).info("Initialisation, reading file: %s", file_name)
            with open(file_name) as f:
                config_data = json.load(f)
                self.register_instance(config_data)

        #we fetch the krakens metadata first
        # not on the ping thread to always have the data available (for the tests for example)
        self.init_kraken_instances()

        if self.start_ping:
            gevent.spawn(self.thread_ping)

    def _clear_cache(self):
        logging.getLogger(__name__).info('clear cache')
        try:
            cache.delete_memoized(self._all_keys_of_id)
        except RuntimeError:
            #if there is an error with cache, flask want to access to the app, this will fail at startup
            #with a "working outside of application context"
            logger = logging.getLogger(__name__)
            logger.exception('there seem to be some kind of problems with the cache')

    def dispatch(self, arguments, api, instance_name=None):
        if instance_name not in self.instances:
            raise RegionNotFound(instance_name)

        instance = self.instances[instance_name]

        scenario = instance.scenario(arguments.get('_override_scenario'))
        if not hasattr(scenario, api) or not callable(getattr(scenario, api)):
            raise ApiNotFound(api)

        publication_date = instance.publication_date
        api_func = getattr(scenario, api)
        resp = api_func(arguments, instance)
        if instance.publication_date != publication_date:
            self._clear_cache()
        return resp

    def init_kraken_instances(self):
        """
        Call all kraken instances (as found in the instances dir) and store it's metadata
        """
        futures = []
        purge_cache_needed = False
        for instance in self.instances.values():
            if not instance.is_initialized:
                futures.append(gevent.spawn(instance.init))

        gevent.wait(futures)
        for future in futures:
            #we check if an instance needs the cache to be purged
            if future.get():
                self._clear_cache()
                break;

    def thread_ping(self, timer=10):
        """
        fetch krakens metadata
        """
        while [i for i in self.instances.values() if not i.is_initialized]:
            self.init_kraken_instances()
            gevent.sleep(timer)
        logging.getLogger(__name__).debug('end of ping thread')

    def stop(self):
        if not self.thread_event.is_set():
            self.thread_event.set()

    def _filter_authorized_instances(self, instances, api):
        if not instances:
            return None
        user = authentication.get_user(token=authentication.get_token())
        valid_instances = [i for i in instances
                           if authentication.has_access(i.name, abort=False, user=user, api=api)]
        if not valid_instances:
            authentication.abort_request(user)
        return valid_instances

    @cache.memoize(app.config['CACHE_CONFIGURATION'].get('TIMEOUT_PTOBJECTS', None))
    def _all_keys_of_id(self, object_id):
        if object_id.count(";") == 1 or object_id[:6] == "coord:":
            if object_id.count(";") == 1:
                lon, lat = object_id.split(";")
            else:
                lon, lat = object_id[6:].split(":")
            try:
                flon = float(lon)
                flat = float(lat)
            except:
                raise InvalidArguments(object_id)
            return self._all_keys_of_coord(flon, flat)
        instances = []
        futures = {}
        for name, instance in self.instances.items():
            futures[name] = gevent.spawn(instance.has_id, object_id)
        for name, future in futures.items():
            if future.get():
                instances.append(name)

        if not instances:
            raise RegionNotFound(object_id=object_id)
        return instances

    def _all_keys_of_coord(self, lon, lat):
        p = geometry.Point(lon, lat)
        instances = [i.name for i in self.instances.values() if i.has_point(p)]
        logging.getLogger(__name__).debug("all_keys_of_coord(self, {}, {}) returns {}".format(lon, lat, instances))
        if not instances:
            raise RegionNotFound(lon=lon, lat=lat)
        return instances

    def get_region(self, region_str=None, lon=None, lat=None, object_id=None, api='ALL'):
        return self.get_regions(region_str, lon, lat, object_id, api, only_one=True)

    def get_regions(self, region_str=None, lon=None, lat=None, object_id=None, api='ALL', only_one=False):
        valid_instances = self.get_instances(region_str, lon, lat, object_id, api)
        if not valid_instances:
            raise RegionNotFound(region=region_str, lon=lon, lat=lat, object_id=object_id)
        if only_one:
            return choose_best_instance(valid_instances).name
        else:
            return [i.name for i in valid_instances]

    def get_instances(self, name=None, lon=None, lat=None, object_id=None, api='ALL'):
        available_instances = []
        if name:
            if name in self.instances:
                available_instances = [self.instances[name]]
        elif lon and lat:
            available_instances = [self.instances[k] for k in self._all_keys_of_coord(lon, lat)]
        elif object_id:
            available_instances = [self.instances[k] for k in self._all_keys_of_id(object_id)]
        else:
            available_instances = list(self.instances.values())

        valid_instances = self._filter_authorized_instances(available_instances, api)
        if available_instances and not valid_instances:
            #user doesn't have access to any of the instances
            authentication.abort_request(user=authentication.get_user())
        else:
            return valid_instances

    def regions(self, region=None, lon=None, lat=None):
        response = {'regions': []}
        regions = []
        if region or lon or lat:
            regions.append(self.get_region(region_str=region, lon=lon, lat=lat))
        else:
            regions = self.get_regions()
        for key_region in regions:
            req = request_pb2.Request()
            req.requested_api = type_pb2.METADATAS
            try:
                resp = self.instances[key_region].send_and_receive(req, timeout=1000)
                resp_dict = protobuf_to_dict(resp.metadatas)
            except DeadSocketException:
                resp_dict = {
                    "status": "dead",
                    "error": {
                        "code": "dead_socket",
                        "value": "The region {} is dead".format(key_region)
                    }
                }
            if resp_dict.get('status') == 'no_data' and not region and not lon and not lat:
                continue
            resp_dict['region_id'] = key_region
            response['regions'].append(resp_dict)
        return response
