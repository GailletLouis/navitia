import copy
from validate import *
from singleton import singleton
from instance_manager import DeadSocketException, RegionNotFound, ApiNotFound, InvalidArguments, NavitiaManager
from werkzeug.wrappers import Response
from renderers import render, render_from_protobuf

class Arguments:
    scheduleArguments = {
        "filter" : Argument("Filter to have the times you want", filter, True,
                            False, order=0),
        "from_datetime" : Argument("The date from which you want the times",
                              datetime_validator, True, False, order=10),
        "duration" : Argument("Maximum duration between the datetime and the last  retrieved stop time",
                                  int, False, False, defaultValue=86400, order=20 ),        
        "wheelchair" : Argument("true if you want the times to have accessibility", boolean, False, False, defaultValue=False, order=50),
        "depth" : Argument("Maximal depth of the returned objects", int, False,
                           False, defaultValue = 1, order=100)
        }
    stopsScheduleArguments = {
        "from_datetime" : Argument("The date from which you want the times",
                              datetime_validator, True, False, order=10),
        "duration" : Argument("Maximum duration between the datetime and the last  retrieved stop time",
                                  int, False, False, defaultValue=86400, order=20 ),        
        "wheelchair" : Argument("true if you want the times to have accessibility", boolean, False, False, defaultValue=False, order=50),
        "depth" : Argument("Maximal depth of the returned objects", int, False,
                           False, defaultValue = 1, order=100),
        "departure_filter" : Argument("The filter of your departure point", filter,
                                                      True, False,order=0),
        "arrival_filter" : Argument("The filter of your arrival point", filter,
                                                      True, False,order=1)
    }

    nextTimesArguments = {
        "filter" : Argument("Filter to have the times you want", filter, False,
                            False, order=0, defaultValue = ""),
        "from_datetime" : Argument("The date from which you want the times",
                              datetime_validator, True, False, order=10),
        "duration" : Argument("Maximum duration between the datetime and the last  retrieved stop time",
                                  int, False, False, defaultValue=86400, order=20 ),        
        "wheelchair" : Argument("true if you want the times to have accessibility", boolean, False, False, defaultValue=False, order=50),
        "depth" : Argument("Maximal depth of the returned objects", int, False,
                           False, defaultValue = 1, order=100),
        "nb_stoptimes": Argument("The maximum number of stop_times, defaults to 20", int , False, False, 20, order=30)
        }

    ptrefArguments = {
        "filter" : Argument("Conditions to filter the returned objects", filter,
                            False, False, "", order=0),
        "depth" : Argument("Maximum depth on objects", int, False, False, 1,
                           order = 50),
        "count" : Argument("Number of elements per page", int, False, False,
                           50),
        "startPage" : Argument("The page number of the ptref result", int,
                               False, False, 0)
        }
    journeyArguments = {
        "origin" : Argument("Departure Point", entrypoint(), True, False, order = 0),
        "destination" : Argument("Destination Point" , entrypoint(), True, False, order = 1),
        "datetime" : Argument("The time from which you want to arrive (or arrive before depending on the value of clockwise)", datetime_validator, True, False, order = 2),
        "clockwise" : Argument("true if you want to have a journey that starts after datetime, false if you a journey that arrives before datetime", boolean, False, False, True, order = 3),
        "max_duration" : Argument("Maximum duratioon of the isochrone", int,
                                  False, False, order=4, defaultValue = 36000),
        "forbidden_uris[]" : Argument("Forbidden uris",  str, False, True, ""),
        "walking_speed" : Argument("Walking speed in m/s", float, False, False, 1.38),
        "walking_distance" : Argument("Maximum walking distance in meters", int,
                                      False, False, 1000),
        "wheelchair" : Argument("Does the journey has to be accessible?",
                                boolean, False, False, False)
        }

    isochroneArguments ={
        "origin" : Argument("Departure Point", entrypoint(), True, False, order = 0),
        "datetime" : Argument("The time from which you want to arrive (or arrive before depending on the value of clockwise)", datetime_validator, True, False, order = 2),
        "clockwise" : Argument("true if you want to have a journey that starts after datetime, false if you a journey that arrives before datetime", boolean, False, False, True, order = 3),
        "max_duration" : Argument("Maximum duratioon of the isochrone", int,
                                  False, False, order=4, defaultValue = 3600),
        "forbidden_uris[]" : Argument("Forbidden uris",  str, False, True, ""),
        "walking_speed" : Argument("Walking speed in m/s", float, False, False, 1.38),
        "walking_distance" : Argument("Maximum walking distance in meters", int,
                                      False, False, 1000),
        "wheelchair" : Argument("Does the journey has to be accessible?",
                                boolean, False, False, False)        }

class Apis:
    apis = {
        "autocomplete" : {
            "arguments" :{
                "name" : Argument("The data to search", unicode, True, False, order = 1),
                "object_type[]" : Argument("The type of datas you want in return", str, False, True, 
                                           ["stop_area", "stop_point", "address", "poi", "admin"], 2,["stop_area", "stop_point", "address", "poi", "admin"]),
                "depth" : Argument("Maximum depth on objects", int, False, False, 1),
                "count" : Argument("Number of elements per page", int, False, False,
                                    50),
                "startPage" : Argument("The page number of the ptref result", int,
                                        False, False, 0),
		"nbmax" : Argument("Maximum number of objects in the response", int, False, False, 10)
            },
            "description" : "Retrieves the objects which contains in their name the \"name\"",
            "order":2},
        "next_departures" : {
            "arguments" : Arguments.nextTimesArguments, 
            "description" : "Retrieves the departures after datetime at the stop points filtered with filter",
            "order":3},
        "next_arrivals" : {
            "arguments" : Arguments.nextTimesArguments,
            "description" : "Retrieves the departures after datetime at the stop points filtered with filter",
            "order":3},
        "route_schedules" : {
            "arguments" : Arguments.scheduleArguments,
            "description" : "Retrieves the schedule of route at the day datetime",
            "order":4},
        "stops_schedules" : {
            "arguments" : Arguments.stopsScheduleArguments,
            "description" : "Retrieves the schedule for 2 stops points",
            "order":4},
        "departure_boards" : {
            "arguments":Arguments.scheduleArguments,
            "description" : "Give all the departures of filter at datetime",
            "order":4},
        "stop_areas" : {
            "arguments" : Arguments.ptrefArguments,
            "description" : "Retrieves all the stop areas filtered with filter",
            "order":5},
        "stop_points" : {
            "arguments" : Arguments.ptrefArguments,
            "description" : "Retrieves all the stop points filtered with filter",
            "order":5},
        "lines" : {
            "arguments" : Arguments.ptrefArguments,
            "description" : "Retrieves all the stop_areas filtered with filter",
            "order":5},
        "routes" : {
            "arguments" : Arguments.ptrefArguments,
            "description" : "Retrieves all the routes filtered with filter",
            "order":5},
        "networks" : {
            "arguments" : Arguments.ptrefArguments,
            "description" : "Retrieves all the networks filtered with filter",
            "order":5},
        "physical_modes" : {
            "arguments" : Arguments.ptrefArguments,
            "description" : "Retrieves all the physical modes filtered with filter",
            "order":5},
        "commercial_modes" : {
            "arguments" : Arguments.ptrefArguments,
            "description" : "Retrieves all the commercial modes filtered with filter",
            "order":5},
        "connections" : {
            "arguments" : Arguments.ptrefArguments,
            "description" : "Retrieves all the connections points filtered with filter",
            "order":5},
        "journey_pattern_points" : {
            "arguments" : Arguments.ptrefArguments,
            "description" : "Retrieves all the journey pattern points filtered with filter",
            "order":5,
            "hidden": True},
        "journey_patterns" : {
            "arguments" : Arguments.ptrefArguments,
            "description" : "Retrieves all the journey pattern filtered with filter",
            "order":5, 
            "hidden": True},
        "companies" : {
            "arguments" : Arguments.ptrefArguments,
            "description" : "Retrieves all the companies filtered with filter",
            "order":5},
        "vehicle_journeys" : {
            "arguments" : Arguments.ptrefArguments,
            "description" :"Retrieves all the vehicle journeys filtered with filter" ,
            "order" : 5},
        "journeys" : {
            "arguments" : Arguments.journeyArguments,
            "description" : "Computes and retrieves a journey",
            "order":1, 
            "universal" : True},
        "isochrone" : {
            "arguments" : Arguments.isochroneArguments,
            "description" : "Computes and retrieves an isochrone",
            "order":1, 
            "universal" : True},
        "proximity_list" : {
            "arguments" : {
                "uri" : Argument("uri arround which you want to look for objects. Not all objects make sense (e.g. a mode).", entrypoint(), True, False, order=0),
                "distance" : Argument("Distance range of the query", int, False, False, 1000, order=3),
                "object_type[]" : Argument("Type of the objects to return", str, False, True, ["stop_area", "stop_point"], order=4),
                "depth" : Argument("Maximum depth on objects", int, False, False, 1),
                "count" : Argument("Number of elements per page", int, False, False,
                                50),
                "startPage" : Argument("The page number of the ptref result", int,
                                    False, False, 0)
            },
            "description" : "Retrieves all the objects around a point within the given distance",
            "order" : 1.1, 
            "universal" : True},
        "metadatas" : {
            "arguments" : {}, 
            "description" : "Retrieves the metadatas of a region"},
        "status" : {
            "arguments" : {}, 
            "description" : "Retrieves the status of a region", 
            "hidden" : True},
        "load" : {
            "arguments" : {},  
            "hidden" : True}
        }

    def __init__(self):
        self.apis_all = copy.copy(self.apis)
        self.apis_all["regions"] = {"arguments" : {}, "description" : "Retrieves the list of available regions", "regions" : False,
                          "order":0}


    def on_api(self, request, version, region, api, format):
        if version != "v0":
            return Response("Unknown version: " + version, status=404)
        if api in self.apis:
                try:
                    return render_from_protobuf(NavitiaManager().dispatch(request, version, region, api, format), request.args.get("callback"))
                except DeadSocketException, e:
                    return Response(e, status=503)
                except RegionNotFound, e:
                    return Response(e, status=404)
                except ApiNotFound, e:
                    return Response(e, status=404)
        else:
            return Response("Unknown api: " + api, status=404)


def validation_decorator(func):
    api = func.__name__
    def wrapped(self, request_args, version, region):
        self.v = validate_arguments(request_args, self.apis[api]["arguments"])
        if not self.v.valid:
            raise InvalidArguments(unicode(self.v.details))
        else:
            return func(self, request_args, version, region)
    return wrapped

