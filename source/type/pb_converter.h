/* Copyright © 2001-2014, Canal TP and/or its affiliates. All rights reserved.
  
This file is part of Navitia,
    the software to build cool stuff with public transport.
 
Hope you'll enjoy and contribute to this project,
    powered by Canal TP (www.canaltp.fr).
Help us simplify mobility and open public transport:
    a non ending quest to the responsive locomotion way of traveling!
  
LICENCE: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
   
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.
   
You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
  
Stay tuned using
twitter @navitia 
IRC #navitia on freenode
https://groups.google.com/d/forum/navitia
www.navitia.io
*/

#pragma once
#include "data.h"
#include "type/type.pb.h"
#include "type/response.pb.h"
#include "type/pt_data.h"

namespace pt = boost::posix_time;

// we don't use a boolean to have a more type checks
enum class DumpMessage {
    Yes,
    No
};

#define null_time_period boost::posix_time::time_period(boost::posix_time::not_a_date_time, boost::posix_time::seconds(0))

//*********************************************

using house_number_coord = std::pair<int, navitia::type::GeographicalCoord>;
using way_name = std::pair<const navitia::georef::Way*, const std::string>;
using way_pair = std::pair<const navitia::georef::Way*, house_number_coord>;
using way_pair_name = std::pair<way_name, house_number_coord>;

namespace ProtoCreator {
struct PbCreator {
    const nt::Data& data;
    const pt::ptime now;
    const pt::time_period action_period;
    const bool show_codes;
    PbCreator(const nt::Data& data, const pt::ptime  now, const pt::time_period action_period, const bool show_codes):
        data(data), now(now), action_period(action_period),show_codes(show_codes) {}
    template<typename N, typename P>
    void fill(int depth, const DumpMessage dump_message, const N* item, P * proto) {
        Filler(depth, dump_message, *this).filler_pb_object(item, proto);
    }

private:
    struct Filler {
        int depth;
        const DumpMessage dump_message;
        PbCreator & pb_creator;
        Filler(int depth, const DumpMessage dump_message, PbCreator & pb_creator):
            depth(depth), dump_message(dump_message), pb_creator(pb_creator){};

        Filler copy_depth_decr();
        template<typename NAV, typename PB>
        void fill(const NAV* nav_object, PB* pb_object) {
            copy_depth_decr().filler_pb_object(nav_object, pb_object);
        }

        void filler_pb_object(const navitia::type::Contributor*, pbnavitia::Contributor*);
        void filler_pb_object(const navitia::type::Frame*, pbnavitia::Frame*);
        void filler_pb_object(const navitia::type::StopArea*, pbnavitia::StopArea*);
        void filler_pb_object(const navitia::type::StopPoint*, pbnavitia::StopPoint*);
        void filler_pb_object(const navitia::type::Company*, pbnavitia::Company*);
        void filler_pb_object(const navitia::type::Network*, pbnavitia::Network*);
        void filler_pb_object(const navitia::type::PhysicalMode*, pbnavitia::PhysicalMode*);
        void filler_pb_object(const navitia::type::CommercialMode*, pbnavitia::CommercialMode*);
        void filler_pb_object(const navitia::type::Line*, pbnavitia::Line*);
        void filler_pb_object(const navitia::type::Route*, pbnavitia::Route*);
        void filler_pb_object(const navitia::type::LineGroup*, pbnavitia::LineGroup*);
        void filler_pb_object(const navitia::type::Calendar*, pbnavitia::Calendar*);
        void filler_pb_object(const navitia::type::ValidityPattern*, pbnavitia::ValidityPattern*);
        void filler_pb_object(const navitia::type::VehicleJourney*, pbnavitia::VehicleJourney*);
        void filler_pb_object(const navitia::georef::Admin*, pbnavitia::AdministrativeRegion*);
        void filler_pb_object(const navitia::type::ExceptionDate*, pbnavitia::CalendarException*);
        void filler_pb_object(const navitia::type::MultiLineString*, pbnavitia::MultiLineString*);
        void filler_pb_object(const navitia::type::GeographicalCoord*, pbnavitia::Address*);
        void filler_pb_object(const way_pair*, pbnavitia::Address*);
        void filler_pb_object(const way_pair_name*, pbnavitia::Address*);
    };
};

template<typename N, typename P>
void fill_pb_object(const N* item, const navitia::type::Data& data, P * proto, int depth = 0,
        const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
        const boost::posix_time::time_period& action_period = null_time_period,
        const bool show_codes = false, const DumpMessage dump_message = DumpMessage::Yes) {
    PbCreator creator(data, now, action_period, show_codes);
    creator.fill(depth, dump_message, item, proto);
}

}

//*********************************************
//forward declare
namespace navitia {
    namespace routing {
        struct PathItem;
        struct JourneyPattern;
        struct JourneyPatternPoint;    
        using JppIdx = Idx<JourneyPatternPoint>;
        using JpIdx = Idx<JourneyPattern>;
    }
    namespace georef {
        struct PathItem;
        struct Way;
        struct POI;
        struct Path;
        struct POIType;
    }
    namespace fare {
        struct results;
        struct Ticket;
    }
    namespace timetables {
        struct Thermometer;
    }
}




namespace navitia {

struct EnhancedResponse {
    pbnavitia::Response response;
    size_t nb_sections = 0;
    std::map<std::pair<pbnavitia::Journey*, size_t>, std::string> routing_section_map;
    pbnavitia::Ticket* unknown_ticket = nullptr; //we want only one unknown ticket

    const std::string& register_section(pbnavitia::Journey* j, const routing::PathItem& /*routing_item*/, size_t section_idx) {
        routing_section_map[{j, section_idx}] = "section_" + boost::lexical_cast<std::string>(nb_sections++);
        return routing_section_map[{j, section_idx}];
    }

    std::string register_section() {
        // For some section (transfer, waiting, streetnetwork, corwfly) we don't need info
        // about the item
        return "section_" + boost::lexical_cast<std::string>(nb_sections++);
    }

    std::string get_section_id(pbnavitia::Journey* j, size_t section_idx) {
        auto it  = routing_section_map.find({j, section_idx});
        if (it == routing_section_map.end()) {
            LOG4CPLUS_WARN(log4cplus::Logger::getInstance("logger"), "Impossible to find section id for section idx " << section_idx);
            return "";
        }
        return it->second;
    }
};

#define FILL_PB_CONSTRUCTOR(type_name, collection_name)\
    void fill_pb_object(const navitia::type::type_name* item, const navitia::type::Data& data, pbnavitia::type_name *, int max_depth = 0,\
            const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,\
            const boost::posix_time::time_period& action_period = null_time_period, \
            const bool show_codes = false, const DumpMessage = DumpMessage::Yes);
    ITERATE_NAVITIA_PT_TYPES(FILL_PB_CONSTRUCTOR)
#undef FILL_PB_CONSTRUCTOR
void fill_pb_object(const std::pair<const routing::JpIdx, const routing::JourneyPattern&>& item,
                    const navitia::type::Data& data,
                    pbnavitia::JourneyPattern *,
                    int max_depth = 0,
                    const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
                    const boost::posix_time::time_period& action_period = null_time_period,
                    const bool show_codes=false);
void fill_pb_object(const std::pair<const routing::JppIdx, const routing::JourneyPatternPoint&>& item,
                    const navitia::type::Data& data,
                    pbnavitia::JourneyPatternPoint *,
                    int max_depth = 0,
                    const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
                    const boost::posix_time::time_period& action_period = null_time_period,
                    const bool show_codes=false);
    

void fill_pb_object(const navitia::georef::Way* way, const type::Data &data, pbnavitia::Address* address, int house_number,
        const type::GeographicalCoord& coord, int max_depth = 0,
        const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
        const boost::posix_time::time_period& action_period = null_time_period);

void fill_pb_object(const navitia::type::GeographicalCoord& coord, const type::Data &data, pbnavitia::Address* address,
        int max_depth = 0, const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
        const boost::posix_time::time_period& action_period = null_time_period);

void fill_pb_object(const navitia::type::StopTime& st, const type::Data &data,
                    pbnavitia::StopTime *stop_time, int max_depth = 0,
                    const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
                    const boost::posix_time::time_period& action_period = null_time_period,
                    const bool show_codes = false,
                    const DumpMessage = DumpMessage::Yes);

void fill_pb_object(const navitia::type::StopTime* st, const type::Data &data, pbnavitia::StopDateTime * stop_date_time, int max_depth = 0,
        const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
        const boost::posix_time::time_period& action_period = null_time_period);

void fill_pb_object(const navitia::type::MetaVehicleJourney* nav_mvj,
                    const nt::Data&,
                    pbnavitia::Trip* pb_trip,
                    int max_depth = 0,
                    const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
                    const boost::posix_time::time_period& action_period = null_time_period,
                    const bool show_codes = false,
                    const DumpMessage = DumpMessage::Yes);

void fill_co2_emission(pbnavitia::Section *pb_section, const nt::Data& data, const type::VehicleJourney* vehicle_journey);
void fill_co2_emission_by_mode(pbnavitia::Section *pb_section, const nt::Data& data, const std::string& mode_uri);

void fill_pb_placemark(const navitia::georef::Way* way, const type::Data &data, pbnavitia::PtObject* place, int house_number, const type::GeographicalCoord& coord, int max_depth = 0,
        const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
        const boost::posix_time::time_period& action_period = null_time_period,
        const bool show_codes = false);



void fill_pb_placemark(const type::EntryPoint& origin, const type::Data &data, pbnavitia::PtObject* place, int max_depth = 0,
        const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
        const boost::posix_time::time_period& action_period = null_time_period,
        const bool show_codes = false);

inline pbnavitia::NavitiaType get_embedded_type(const type::Calendar*) { return pbnavitia::CALENDAR; }
inline pbnavitia::NavitiaType get_embedded_type(const type::VehicleJourney*) { return pbnavitia::VEHICLE_JOURNEY; }
inline pbnavitia::NavitiaType get_embedded_type(const type::Line*) { return pbnavitia::LINE; }
inline pbnavitia::NavitiaType get_embedded_type(const type::Route*) { return pbnavitia::ROUTE; }
inline pbnavitia::NavitiaType get_embedded_type(const type::Company*) { return pbnavitia::COMPANY; }
inline pbnavitia::NavitiaType get_embedded_type(const type::Network*) { return pbnavitia::NETWORK; }
inline pbnavitia::NavitiaType get_embedded_type(const georef::POI*) { return pbnavitia::POI; }
inline pbnavitia::NavitiaType get_embedded_type(const georef::Admin*) { return pbnavitia::ADMINISTRATIVE_REGION; }
inline pbnavitia::NavitiaType get_embedded_type(const type::StopArea*) { return pbnavitia::STOP_AREA; }
inline pbnavitia::NavitiaType get_embedded_type(const type::StopPoint*) { return pbnavitia::STOP_POINT; }
inline pbnavitia::NavitiaType get_embedded_type(const type::CommercialMode*) { return pbnavitia::COMMERCIAL_MODE; }
inline pbnavitia::NavitiaType get_embedded_type(const type::MetaVehicleJourney*) { return pbnavitia::TRIP; }

inline pbnavitia::Calendar* get_sub_object(const type::Calendar*, pbnavitia::PtObject* pt_object) { return pt_object->mutable_calendar(); }
inline pbnavitia::VehicleJourney* get_sub_object(const type::VehicleJourney*, pbnavitia::PtObject* pt_object) { return pt_object->mutable_vehicle_journey(); }
inline pbnavitia::Line* get_sub_object(const type::Line*, pbnavitia::PtObject* pt_object) { return pt_object->mutable_line(); }
inline pbnavitia::Route* get_sub_object(const type::Route*, pbnavitia::PtObject* pt_object) { return pt_object->mutable_route(); }
inline pbnavitia::Company* get_sub_object(const type::Company*, pbnavitia::PtObject* pt_object) { return pt_object->mutable_company(); }
inline pbnavitia::Network* get_sub_object(const type::Network*, pbnavitia::PtObject* pt_object) { return pt_object->mutable_network(); }
inline pbnavitia::Poi* get_sub_object(const georef::POI*, pbnavitia::PtObject* pt_object) { return pt_object->mutable_poi(); }
inline pbnavitia::AdministrativeRegion* get_sub_object(const georef::Admin*, pbnavitia::PtObject* pt_object) { return pt_object->mutable_administrative_region(); }
inline pbnavitia::StopArea* get_sub_object(const type::StopArea*, pbnavitia::PtObject* pt_object) { return pt_object->mutable_stop_area(); }
inline pbnavitia::StopPoint* get_sub_object(const type::StopPoint*, pbnavitia::PtObject* pt_object) { return pt_object->mutable_stop_point(); }
inline pbnavitia::CommercialMode* get_sub_object(const type::CommercialMode*, pbnavitia::PtObject* pt_object) { return pt_object->mutable_commercial_mode(); }
inline pbnavitia::Trip* get_sub_object(const type::MetaVehicleJourney*, pbnavitia::PtObject* pt_object) { return pt_object->mutable_trip(); }

void fill_crowfly_section(const type::EntryPoint& origin, const type::EntryPoint& destination,
                          const time_duration& crow_fly_duration, type::Mode_e mode,
                          boost::posix_time::ptime origin_time, const type::Data& data,
                          EnhancedResponse& response,  pbnavitia::Journey* pb_journey,
                          const pt::ptime& now, const pt::time_period& action_period);

void fill_fare_section(EnhancedResponse& pb_response, pbnavitia::Journey* pb_journey, const fare::results& fare);

void fill_street_sections(EnhancedResponse& response, const type::EntryPoint &ori_dest, const georef::Path & path, const type::Data &data, pbnavitia::Journey* pb_journey,
        const boost::posix_time::ptime departure, int max_depth = 1,
        const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
        const boost::posix_time::time_period& action_period = null_time_period);

template <typename T>
void fill_message(const type::disruption::Impact& impact, const type::Data &data, T pb_object, int max_depth = 0,
        const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
        const boost::posix_time::time_period& action_period = null_time_period, const bool show_codes = false);

template <typename HasMessage, typename PbObj>
void fill_messages(const HasMessage*, const type::Data& data, PbObj, int max_depth = 0,
                   const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
                   const boost::posix_time::time_period& action_period = null_time_period,
                   const bool show_codes = false, const DumpMessage = DumpMessage::Yes);

void add_path_item(pbnavitia::StreetNetwork* sn, const navitia::georef::PathItem& item, const type::EntryPoint &ori_dest,
                   const navitia::type::Data& data);

void fill_pb_object(const georef::POI*, const type::Data &data, pbnavitia::Poi* poi, int max_depth = 0,
                    const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
                    const boost::posix_time::time_period& action_period = null_time_period,
                    const bool show_codes = false, const DumpMessage = DumpMessage::Yes);

void fill_pb_object(const georef::POIType*, const type::Data &data, pbnavitia::PoiType* poi_type, int max_depth = 0,
                    const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
                    const boost::posix_time::time_period& action_period = null_time_period);

void fill_pb_object(const georef::Admin* adm, const type::Data& data, pbnavitia::AdministrativeRegion* admin,
                    int max_depth = 0,
                    const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
                    const boost::posix_time::time_period& action_period = null_time_period,
                    const bool show_codes = false, const DumpMessage = DumpMessage::Yes);

void fill_pb_object(const navitia::type::StopTime* st, const type::Data& data, pbnavitia::ScheduleStopTime* row,
                    int max_depth = 0,
                    const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
                    const boost::posix_time::time_period& action_period = null_time_period,
                    const DateTime& date_time = DateTime(),
                    boost::optional<const std::string> calendar_id = boost::optional<const std::string>());

void fill_pb_object(const type::StopPointConnection* c, const type::Data& data,
                    pbnavitia::Connection* connection, int max_depth,
                    const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
                    const boost::posix_time::time_period& action_period = null_time_period);

void fill_pb_object(const type::Route* r, const type::Data& data,
                    pbnavitia::PtDisplayInfo* pt_display_info, int max_depth,
                    const boost::posix_time::ptime& now,
                    const boost::posix_time::time_period& action_period = null_time_period);


void fill_pb_object(const nt::VehicleJourney* vj,
                    const nt::Data& data,
                    pbnavitia::VehicleJourney * vehicle_journey,
                    int max_depth,
                    const pt::ptime& now,
                    const pt::time_period& action_period,
                    const bool show_codes, const DumpMessage);

void fill_pb_object(const type::VehicleJourney* vj,
                    const type::Data& data,
                    pbnavitia::PtDisplayInfo* pt_display_info,
                    const type::StopTime* origin,
                    const type::StopTime* destination,
                    int max_depth,
                    const boost::posix_time::ptime& now,
                    const boost::posix_time::time_period& action_period);

void fill_pb_object(const type::VehicleJourney* vj,
                    const type::Data&,
                    pbnavitia::hasEquipments* has_equipments,
                    const type::StopPoint* origin,
                    const type::StopPoint* destination,
                    int,
                    const pt::ptime&,
                    const pt::time_period&);

void fill_pb_object(const type::VehicleJourney* vj,
                    const type::Data& data,
                    pbnavitia::hasEquipments* has_equipments,
                    int max_depth,
                    const pt::ptime& now,
                    const pt::time_period& action_period);

void fill_additional_informations(google::protobuf::RepeatedField<int>* infos,
                                  const bool has_datetime_estimated,
                                  const bool has_odt,
                                  const bool is_zonal);

void fill_pb_error(const pbnavitia::Error::error_id id, const std::string& comment,
                    pbnavitia::Error* error, int max_depth = 0 ,
                    const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
                    const boost::posix_time::time_period& action_period  = null_time_period);

void fill_pb_object(const navitia::type::ExceptionDate& exception_date, const type::Data& data,
                    pbnavitia::CalendarException* calendar_exception, int max_depth,
                    const boost::posix_time::ptime& now, const boost::posix_time::time_period& action_period);

void fill_pb_object(const std::string comment, const type::Data& data,
                    pbnavitia::Note* note, int max_depth,
                    const boost::posix_time::ptime& now, const boost::posix_time::time_period& action_period);

void fill_pb_object(const navitia::type::StopTime* st, const type::Data& data,
                    pbnavitia::Properties* properties, int max_depth,
                    const boost::posix_time::ptime& now, const boost::posix_time::time_period& action_period);

void fill_pb_object(const type::disruption::Impact*, const type::Data& data,
                    pbnavitia::Response*, int max_depth,
                    const boost::posix_time::ptime& now,
                    const boost::posix_time::time_period& action_period,
                    bool show_codes);

void fill_pb_object(const navitia::type::MultiLineString& shape, pbnavitia::MultiLineString* geojson);


void fill_pb_placemark(const navitia::georef::Admin* value, const type::Data &data, pbnavitia::PtObject* pt_object,
        int max_depth = 0,
        const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
        const boost::posix_time::time_period& action_period = null_time_period,
        const bool show_codes=false);


/**
 * get_label() is a function that returns:
 * * the label (or a function get_label()) if the object has it
 * * else return the name of the object
 *
 * Note: the trick is that int is a better match for 0 than
 * long and template resolution takes the best match
 */
namespace detail {
template<typename T>
auto get_label_if_exists(const T* v, int) -> decltype(v->label) {
    return v->label;
}

template<typename T>
auto get_label_if_exists(const T* v, int) -> decltype(v->get_label()) {
    return v->get_label();
}

template<typename T>
auto get_label_if_exists(const T* v, long) -> decltype(v->name) {
    return v->name;
}
}

template<typename T>
std::string get_label(const T* v) {
    return detail::get_label_if_exists(v, 0);
}

template<typename T>
void fill_pb_placemark(const T* value, const type::Data &data, pbnavitia::PtObject* pt_object, int max_depth = 0,
        const boost::posix_time::ptime& now = boost::posix_time::not_a_date_time,
        const boost::posix_time::time_period& action_period = null_time_period,
        const bool show_codes = false, const DumpMessage dump_message = DumpMessage::Yes) {
    if(value == nullptr)
        return;
    int depth = (max_depth <= 3) ? max_depth : 3;
    fill_pb_object(value, data, get_sub_object(value, pt_object), depth,
                   now, action_period, show_codes, dump_message);
    pt_object->set_name(get_label(value));
    pt_object->set_uri(value->uri);
    pt_object->set_embedded_type(get_embedded_type(value));
}

pbnavitia::StreetNetworkMode convert(const navitia::type::Mode_e& mode);

}//namespace navitia
