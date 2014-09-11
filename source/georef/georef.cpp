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

#include "georef.h"

#include "utils/logger.h"
#include "utils/functions.h"
#include "utils/csv.h"
#include "utils/configuration.h"

#include <unordered_map>
#include <boost/foreach.hpp>
#include <array>
#include <boost/math/constants/constants.hpp>

using navitia::type::idx_t;

namespace navitia{ namespace georef{

/** Ajout d'une adresse dans la liste des adresses d'une rue
  * les adresses avec un numéro pair sont dans la liste "house_number_right"
  * les adresses avec un numéro impair sont dans la liste "house_number_left"
  * Après l'ajout, la liste est trié dans l'ordre croissant des numéros
*/

void Way::add_house_number(const HouseNumber& house_number){
    if (house_number.number % 2 == 0){
            this->house_number_right.push_back(house_number);
            std::sort(this->house_number_right.begin(),this->house_number_right.end());
    } else{
        this->house_number_left.push_back(house_number);
        std::sort(this->house_number_left.begin(),this->house_number_left.end());
    }
}

/** Recherche des coordonnées les plus proches à un un numéro
    * les coordonnées par extrapolation
*/
nt::GeographicalCoord Way::extrapol_geographical_coord(int number){
    HouseNumber hn_upper, hn_lower;
    nt::GeographicalCoord to_return;

    if (number % 2 == 0){ // pair
        for(auto it=this->house_number_right.begin(); it != this->house_number_right.end(); ++it){
            if ((*it).number  < number){
                hn_lower = (*it);
            }else {
                hn_upper = (*it);
                break;
            }
        }
    }else{
        for(auto it=this->house_number_left.begin(); it != this->house_number_left.end(); ++it){
            if ((*it).number  < number){
                hn_lower = (*it);
            }else {
                hn_upper = (*it);
                break;
            }
        }
    }

    // Extrapolation des coordonnées:
    int diff_house_number = hn_upper.number - hn_lower.number;
    int diff_number = number - hn_lower.number;

    double x_step = (hn_upper.coord.lon() - hn_lower.coord.lon()) /diff_house_number;
    to_return.set_lon(hn_lower.coord.lon() + x_step*diff_number);

    double y_step = (hn_upper.coord.lat() - hn_lower.coord.lat()) /diff_house_number;
    to_return.set_lat(hn_lower.coord.lat() + y_step*diff_number);

    return to_return;
}

/**
    * Si le numéro est plus grand que les numéros, on renvoie les coordonées du plus grand de la rue
    * Si le numéro est plus petit que les numéros, on renvoie les coordonées du plus petit de la rue
    * Si le numéro existe, on renvoie ses coordonnées
    * Sinon, les coordonnées par extrapolation
*/

nt::GeographicalCoord Way::get_geographical_coord(const std::vector< HouseNumber>& house_number_list, const int number){
    if (!house_number_list.empty()){

        /// Dans le cas où le numéro recherché est plus grand que tous les numéros de liste
        if (house_number_list.back().number <= number){
            return house_number_list.back().coord;
        }

        /// Dans le cas où le numéro recherché est plus petit que tous les numéros de liste
        if (house_number_list.front().number >= number){
            return house_number_list.front().coord;
        }

        /// Dans le cas où le numéro recherché est dans la liste = à un numéro dans la liste
        for(auto it=house_number_list.begin(); it != house_number_list.end(); ++it){
            if ((*it).number  == number){
                return (*it).coord;
             }
        }

        /// Dans le cas où le numéro recherché est dans la liste et <> à tous les numéros
        return extrapol_geographical_coord(number);
    }
    nt::GeographicalCoord to_return;
    return to_return;
}

/** Recherche des coordonnées les plus proches à un numéro
    * Si la rue n'a pas de numéro, on renvoie son barycentre
*/
nt::GeographicalCoord Way::nearest_coord(const int number, const Graph& graph){
    /// Attention la liste :
    /// "house_number_right" doit contenir les numéros pairs
    /// "house_number_left" doit contenir les numéros impairs
    /// et les deux listes doivent être trier par numéro croissant

    if (( this->house_number_right.empty() && this->house_number_left.empty() )
            || (this->house_number_right.empty() && number % 2 == 0)
            || (this->house_number_left.empty() && number % 2 != 0)
            || number <= 0)
        return projected_centroid(graph);

    if (number % 2 == 0) // Pair
        return get_geographical_coord(this->house_number_right, number);
    else // Impair
        return get_geographical_coord(this->house_number_left, number);
}

// returns the centroid projected on the way
nt::GeographicalCoord Way::projected_centroid(const Graph& graph){
    std::vector<nt::GeographicalCoord> line;
    nt::GeographicalCoord centroid;

    std::pair<vertex_t, vertex_t> previous(type::invalid_idx, type::invalid_idx);
    for(auto edge : this->edges){
        if(edge.first != previous.second || edge.second != previous.first ){
            line.push_back(graph[edge.first].coord);
            line.push_back(graph[edge.second].coord);
        }
        previous = edge;
    }
    try{
        boost::geometry::centroid(line, centroid);
    }catch(...){
        LOG4CPLUS_WARN(log4cplus::Logger::getInstance("log"),
                       "Can't find the centroid of the way::  " << this->name);
    }

    if (line.empty()) { return centroid; }

    // project the centroid on the way
    nt::GeographicalCoord projected_centroid = line.front();
    float min_dist = centroid.distance_to(projected_centroid);
    nt::GeographicalCoord last = line.front();
    auto cur = line.begin();
    for (++cur; cur != line.end(); last = *cur, ++cur) {
        auto projection = centroid.project(last, *cur);
        if (projection.second < min_dist) {
            min_dist = projection.second;
            projected_centroid = projection.first;
        }
    }

    return projected_centroid;
}

/** Recherche du némuro le plus proche à des coordonnées
    * On récupère le numéro se trouvant à une distance la plus petite par rapport aux coordonnées passées en paramètre
*/
int Way::nearest_number(const nt::GeographicalCoord& coord){

    int to_return = -1;
    double distance, distance_temp;
    distance = std::numeric_limits<double>::max();
    for(auto house_number : this->house_number_left){
        distance_temp = coord.distance_to(house_number.coord);
        if (distance  > distance_temp){
            to_return = house_number.number;
            distance = distance_temp;
        }
    }
    for(auto house_number : this->house_number_right){
        distance_temp = coord.distance_to(house_number.coord);
        if (distance  > distance_temp){
            to_return = house_number.number;
            distance = distance_temp;
        }
    }
    return to_return;
}


type::Mode_e GeoRef::get_mode(vertex_t vertex) const {
    return static_cast<type::Mode_e>(vertex / nb_vertex_by_mode);
}

PathItem::TransportCaracteristic GeoRef::get_caracteristic(edge_t edge) const {
    auto source_mode = get_mode(boost::source(edge, graph));
    auto target_mode = get_mode(boost::target(edge, graph));

    if (source_mode == target_mode) {
        switch (source_mode) {
        case type::Mode_e::Walking:
            return PathItem::TransportCaracteristic::Walk;
        case type::Mode_e::Bike:
            return PathItem::TransportCaracteristic::Bike;
        case type::Mode_e::Car:
            return PathItem::TransportCaracteristic::Car;
        default:
            throw navitia::exception("unhandled path item caracteristic");
        }
    }
    if (source_mode == type::Mode_e::Walking && target_mode == type::Mode_e::Bike) {
        return PathItem::TransportCaracteristic::BssTake;
    }
    if (source_mode == type::Mode_e::Bike && target_mode == type::Mode_e::Walking) {
        return PathItem::TransportCaracteristic::BssPutBack;
    }
    if (source_mode == type::Mode_e::Walking && target_mode == type::Mode_e::Car) {
        return PathItem::TransportCaracteristic::CarLeaveParking;
    }
    if (source_mode == type::Mode_e::Car && target_mode == type::Mode_e::Walking) {
        return PathItem::TransportCaracteristic::CarPark;
    }

    throw navitia::exception("unhandled path item caracteristic");
}

void GeoRef::add_way(const Way& w){
    Way* to_add = new Way;
    to_add->name = w.name;
    to_add->idx = w.idx;
    to_add->uri = w.uri;
    ways.push_back(to_add);
}

ProjectionData::ProjectionData(const type::GeographicalCoord & coord, const GeoRef & sn, const proximitylist::ProximityList<vertex_t> &prox) {
    edge_t edge;
    found = true;
    try {
        edge = sn.nearest_edge(coord, prox);
    } catch(proximitylist::NotFound) {
        found = false;
        vertices[Direction::Source] = std::numeric_limits<vertex_t>::max();
        vertices[Direction::Target] = std::numeric_limits<vertex_t>::max();
    }

    if(found) {
        init(coord, sn, edge);
    }
}

ProjectionData::ProjectionData(const type::GeographicalCoord & coord, const GeoRef & sn, type::idx_t offset, const proximitylist::ProximityList<vertex_t> &prox){
    edge_t edge;
    found = true;
    try {
        edge = sn.nearest_edge(coord, prox, offset);
    } catch(proximitylist::NotFound) {
        found = false;
        vertices[Direction::Source] = std::numeric_limits<vertex_t>::max();
        vertices[Direction::Target] = std::numeric_limits<vertex_t>::max();
    }

    if(found) {
        init(coord, sn, edge);
    }
}

void ProjectionData::init(const type::GeographicalCoord & coord, const GeoRef & sn, edge_t nearest_edge) {
    // On cherche les coordonnées des extrémités de ce segment
    vertices[Direction::Source] = boost::source(nearest_edge, sn.graph);
    vertices[Direction::Target] = boost::target(nearest_edge, sn.graph);
    const type::GeographicalCoord& vertex1_coord = sn.graph[vertices[Direction::Source]].coord;
    const type::GeographicalCoord& vertex2_coord = sn.graph[vertices[Direction::Target]].coord;
    // On projette le nœud sur le segment
    this->projected = coord.project(vertex1_coord, vertex2_coord).first;
    // On calcule la distance « initiale » déjà parcourue avant d'atteindre ces extrémité d'où on effectue le calcul d'itinéraire
    distances[Direction::Source] = projected.distance_to(vertex1_coord);
    distances[Direction::Target] = projected.distance_to(vertex2_coord);
}

/**
 * there are 3 graphs:
 *  - one for the walk
 *  - one for the bike
 *  - one for the car
 *
 *  since some transportation modes mixes the differents graphs (ie for bike sharing you use the walking and biking graph)
 *  there are some edges between the 3 graphs
 *
 *  the Vls has thus not it's own graph and all projections are done on the walking graph (hence its offset is the walking graph offset)
 */
void GeoRef::init() {
    offsets[nt::Mode_e::Walking] = 0;
    offsets[nt::Mode_e::Bss] = 0;

    //each graph has the same number of vertex
    nb_vertex_by_mode = boost::num_vertices(graph);

    //we dupplicate the graph for the bike and the car
    for (nt::Mode_e mode : {nt::Mode_e::Bike, nt::Mode_e::Car}) {
        offsets[mode] = boost::num_vertices(graph);
        for (vertex_t v = 0; v < nb_vertex_by_mode; ++v){
            boost::add_vertex(graph[v], graph);
        }
    }
}

void GeoRef::build_proximity_list(){
    pl.clear();

    //do not build the proximitylist with the edge of other transportation mode than walking (and walking HAS to be the first graph)
    for(vertex_t v = 0; v < nb_vertex_by_mode; ++v){
        pl.add(graph[v].coord, v);
    }

    pl.build();

    poi_proximity_list.clear();

    for(const POI *poi : pois) {
        poi_proximity_list.add(poi->coord, poi->idx);
    }
    poi_proximity_list.build();
}

void GeoRef::build_autocomplete_list(){
    int pos = 0;
    fl_way.clear();
    for(Way* way : ways){
        if (!way->name.empty()) {
            std::string key="";
            for(Admin* admin : way->admin_list){
                //Level Admin 8  : City
                if (admin->level == 8) {
                    key+= " " + admin->name;
                }
                if ((!admin->post_code.empty()) && (admin->level == 8)) {
                    key += " "+ admin->post_code;
                }
            }
            fl_way.add_string(way->way_type +" "+ way->name + " " + key, pos, this->synonyms);
        }
        pos++;
    }
    fl_way.build();

    fl_poi.clear();
    //Autocomplete poi list
    for(const POI* poi : pois){
        if ((!poi->name.empty()) && (poi->visible)) {
            std::string key="";
            for(Admin* admin : poi->admin_list) {
                //Level Admin 8  : City
                if (admin->level == 8) {
                    key += " " + admin->name;
                }
            }
            fl_poi.add_string(poi->name + " " + key, poi->idx , this->synonyms);
        }
    }
    fl_poi.build();

    fl_admin.clear();
    for(Admin* admin : admins){
        std::string key="";

        if (!admin->post_code.empty())
        {
            key = admin->post_code;
        }
        fl_admin.add_string(admin->name + " " + key, admin->idx , this->synonyms);
    }
    fl_admin.build();
}


/** Chargement de la liste poitype_map : mappage entre codes externes et idx des POITypes*/
void GeoRef::build_poitypes_map(){
   this->poitype_map.clear();
   for(const POIType* ptype : poitypes){
       this->poitype_map[ptype->uri] = ptype->idx;
   }
}

/** Chargement de la liste poi_map : mappage entre codes externes et idx des POIs*/
void GeoRef::build_pois_map(){
    this->poi_map.clear();
   for(const POI* poi : pois){
       this->poi_map[poi->uri] = poi->idx;
   }
}

/** Normalisation des codes externes des rues*/
void GeoRef::normalize_extcode_way(){
    this->way_map.clear();
    for(Way* way : ways){
        way->uri = "address:"+ way->uri;
        this->way_map[way->uri] = way->idx;
    }
}


void GeoRef::build_admin_map(){
    this->admin_map.clear();
    for(Admin* admin : admins){
        this->admin_map[admin->uri] = admin->idx;
    }
}

/**
    * Recherche les voies avec le nom, ce dernier peut contenir : [Numéro de rue] + [Type de la voie ] + [Nom de la voie] + [Nom de la commune]
        * Exemple : 108 rue victor hugo reims
    * Si le numéro est rensigné, on renvoie les coordonnées les plus proches
    * Sinon le barycentre de la rue
*/
std::vector<nf::Autocomplete<nt::idx_t>::fl_quality> GeoRef::find_ways(const std::string & str, const int nbmax, const int search_type, std::function<bool(nt::idx_t)> keep_element) const{
    std::vector<nf::Autocomplete<nt::idx_t>::fl_quality> to_return;
    boost::tokenizer<> tokens(str);

    int search_number = str_to_int(*tokens.begin());
    std::string search_str;

    //Si un numero existe au début de la chaine alors il faut l'exclure.
    if (search_number != -1){
        search_str = "";
        int i = 0;
        for(auto token : tokens){
            if (i != 0){
                search_str = search_str + " " + token;
            }
            ++i;
           }
    }else{
        search_str = str;
    }
    if (search_type == 0){
        to_return = fl_way.find_complete(search_str, this->synonyms, nbmax, keep_element);
    }else{
        to_return = fl_way.find_partial_with_pattern(search_str, this->synonyms, word_weight, nbmax, keep_element);
    }

    /// récupération des coordonnées du numéro recherché pour chaque rue
    for(auto &result_item  : to_return){
       Way * way = this->ways[result_item.idx];
       result_item.coord = way->nearest_coord(search_number, this->graph);
       result_item.house_number = search_number;
    }

    return to_return;
}

void GeoRef::project_stop_points(const std::vector<type::StopPoint*> &stop_points) {
   enum class error {
       matched = 0,
       matched_walking,
       matched_bike,
       matched_car,
       not_initialized,
       not_valid,
       other,
       size
   };
   navitia::flat_enum_map<error, int> messages {{{}}};

   this->projected_stop_points.clear();
   this->projected_stop_points.reserve(stop_points.size());

   for(const type::StopPoint* stop_point : stop_points) {
       std::pair<GeoRef::ProjectionByMode, bool> pair = project_stop_point(stop_point);

       this->projected_stop_points.push_back(pair.first);
       if (pair.second) {
           messages[error::matched] += 1;
       } else {
           //verify if coordinate is not valid:
           if (! stop_point->coord.is_initialized()) {
               messages[error::not_initialized] += 1;
           } else if (! stop_point->coord.is_valid()) {
               messages[error::not_valid] += 1;
           } else {
               messages[error::other] += 1;
           }
       }
       if (pair.first[nt::Mode_e::Walking].found) {
           messages[error::matched_walking] += 1;
       }
       if (pair.first[nt::Mode_e::Bike].found) {
           messages[error::matched_bike] += 1;
       }
       if (pair.first[nt::Mode_e::Car].found) {
           messages[error::matched_car] += 1;
       }
   }

   auto log = log4cplus::Logger::getInstance("kraken::type::Data::project_stop_point");
   LOG4CPLUS_DEBUG(log, "Number of stop point projected on the georef network : "
                   << messages[error::matched] << " (on " << stop_points.size() << ")");


   LOG4CPLUS_DEBUG(log, "Number of stop point projected on the walking georef network : "
                   << messages[error::matched_walking] << " (on " << stop_points.size() << ")");
   LOG4CPLUS_DEBUG(log, "Number of stop point projected on the biking georef network : "
                   << messages[error::matched_bike] << " (on " << stop_points.size() << ")");
   LOG4CPLUS_DEBUG(log, "Number of stop point projected on the car georef network : "
                   << messages[error::matched_car] << " (on " << stop_points.size() << ")");

   if (messages[error::not_initialized]) {
       LOG4CPLUS_DEBUG(log, "Number of stop point rejected (X=0 or Y=0)"
                       << messages[error::not_initialized]);
   }
   if (messages[error::not_valid]) {
       LOG4CPLUS_DEBUG(log, "Number of stop point rejected (not valid)"
                       << messages[error::not_valid]);
   }
   if (messages[error::other]) {
       LOG4CPLUS_DEBUG(log, "Number of stop point rejected (other issues)"
                       << messages[error::other]);
   }
}

void GeoRef::build_admins_stop_points(std::vector<type::StopPoint*> & stop_points){
    auto log = log4cplus::Logger::getInstance("kraken::type::GeoRef::fill_admins_stop_points");
    int cpt_no_projected = 0;
    for(type::StopPoint* stop_point : stop_points) {
        ProjectionData projection = this->projected_stop_points[stop_point->idx][type::Mode_e::Walking];
        if(projection.found){
            const edge_t edge = boost::edge(projection[ProjectionData::Direction::Source],
                                         projection[ProjectionData::Direction::Target],
                                         this->graph).first;
            const georef::Way *way = this->ways[this->graph[edge].way_idx];
            stop_point->admin_list.insert(stop_point->admin_list.end(),
                                          way->admin_list.begin(),
                                          way->admin_list.end());
        }else{
            cpt_no_projected++;
        }
    }
    LOG4CPLUS_DEBUG(log, cpt_no_projected<<"/"<<stop_points.size() << " stop_points are not associated with any admins");
}

void GeoRef::build_admins_pois(){
    auto log = log4cplus::Logger::getInstance("kraken::type::GeoRef::fill_admins_pois");
    int cpt_no_projected = 0;
    int cpt_no_initialized = 0;
    for(POI* poi : this->pois){
        if(poi->coord.is_initialized()){
            try{
                edge_t edge = this->nearest_edge(poi->coord);
                georef::Way *way = this->ways[this->graph[edge].way_idx];
                poi->admin_list.insert(poi->admin_list.end(),
                                       way->admin_list.begin(), way->admin_list.end());
            }catch(proximitylist::NotFound){
                cpt_no_projected++;
            }
        }else{
            cpt_no_initialized++;
        }
    }
    LOG4CPLUS_DEBUG(log, cpt_no_projected<<"/"<<this->pois.size() << " pois are not associated with any admins");
    LOG4CPLUS_DEBUG(log, cpt_no_initialized<<"/"<<this->pois.size() << " pois with coordinates not initialized");
}

std::pair<GeoRef::ProjectionByMode, bool> GeoRef::project_stop_point(const type::StopPoint* stop_point) const {
    bool one_proj_found = false;
    ProjectionByMode projections;

    // for a given mode, in which layer the stop are projected
    const flat_enum_map<nt::Mode_e, nt::Mode_e> mode_to_layer {{{
        nt::Mode_e::Walking, // Walking -> Walking
        nt::Mode_e::Bike, // Bike -> Bike
        nt::Mode_e::Walking, // Car -> Walking
        nt::Mode_e::Walking // Bss -> Walking
    }}};

    for (auto const &mode_layer: mode_to_layer) {
        nt::Mode_e mode = mode_layer.first;
        nt::idx_t offset = offsets[mode_layer.second];

        ProjectionData proj(stop_point->coord, *this, offset, this->pl);
        projections[mode] = proj;
        if(proj.found)
            one_proj_found = true;
    }

    return {projections, one_proj_found};
}

vertex_t GeoRef::nearest_vertex(const type::GeographicalCoord & coordinates, const proximitylist::ProximityList<vertex_t> &prox) const {
    return prox.find_nearest(coordinates);
}

edge_t GeoRef::nearest_edge(const type::GeographicalCoord & coordinates) const {
    return this->nearest_edge(coordinates, this->pl);
}

/// Get the nearest_edge with at least one vertex in the graph corresponding to the offset (walking, bike, ...)
edge_t GeoRef::nearest_edge(const type::GeographicalCoord & coordinates, const proximitylist::ProximityList<vertex_t>& prox, type::idx_t offset) const {
    boost::optional<edge_t> res;
    float min_dist = 0.;
    for (const auto pair_coord : prox.find_within(coordinates)) {
        //we increment the index to get the vertex in the other graph
        const auto u = pair_coord.first + offset;

        BOOST_FOREACH (edge_t e, boost::out_edges(u, graph)) {
            const auto v = target(e, graph);
            float cur_dist = coordinates.project(graph[u].coord, graph[v].coord).second;
            if (!res || cur_dist < min_dist) {
                min_dist = cur_dist;
                res = e;
            }
        }
    }
    if (res) { return *res; }
    throw proximitylist::NotFound();
}

//get the minimum distance and the vertex to start from between 2 edges
static std::tuple<float, vertex_t, vertex_t>
get_min_distance(const GeoRef& geo_ref, const type::GeographicalCoord &coord, edge_t walking_e, edge_t biking_e) {
    vertex_t source_a_idx = source(walking_e, geo_ref.graph);
    Vertex source_a = geo_ref.graph[source_a_idx];

    vertex_t target_a_idx = target(walking_e, geo_ref.graph);
    Vertex target_a = geo_ref.graph[target_a_idx];

    vertex_t source_b_idx = source(biking_e, geo_ref.graph);
    Vertex source_b = geo_ref.graph[source_b_idx];

    vertex_t target_b_idx = target(biking_e, geo_ref.graph);
    Vertex target_b = geo_ref.graph[target_b_idx];

    const vertex_t min_a_idx =
        coord.distance_to(source_a.coord) < coord.distance_to(target_a.coord) ? source_a_idx : target_a_idx;
    const vertex_t min_b_idx =
        coord.distance_to(source_b.coord) < coord.distance_to(target_b.coord) ? source_b_idx : target_b_idx;

    return std::make_tuple(
        geo_ref.graph[min_a_idx].coord.distance_to(geo_ref.graph[min_b_idx].coord),
        min_a_idx,
        min_b_idx);
}

bool GeoRef::add_bss_edges(const type::GeographicalCoord& coord) {
    using navitia::type::Mode_e;

    edge_t nearest_biking_edge, nearest_walking_edge;
    try {
        //we need to find the nearest edge in the walking graph and the nearest edge in the biking graph
        nearest_biking_edge = nearest_edge(coord, Mode_e::Bike);
        nearest_walking_edge = nearest_edge(coord, Mode_e::Walking);
    } catch(proximitylist::NotFound) {
        return false;
    }

    //we add a new edge linking those 2 edges, with the walking distance between the 2 edges + the time to take of hang the bike back
    auto min_dist = get_min_distance(*this, coord, nearest_walking_edge, nearest_biking_edge);
    vertex_t walking_v = std::get<1>(min_dist);
    vertex_t biking_v = std::get<2>(min_dist);
    time_duration dur_between_edges = seconds(std::get<0>(min_dist) / default_speed[Mode_e::Walking]);

    navitia::georef::Edge edge;
    edge.way_idx = graph[nearest_walking_edge].way_idx; //arbitrarily we assume the way is the walking way

    // time needed to take the bike + time to walk between the edges
    edge.duration = dur_between_edges + default_time_bss_pickup;
    add_edge(walking_v, biking_v, edge, graph);

    // time needed to hang the bike back + time to walk between the edges
    edge.duration = dur_between_edges + default_time_bss_putback;
    add_edge(biking_v, walking_v, edge, graph);

    return true;
}

bool GeoRef::add_parking_edges(const type::GeographicalCoord& coord) {
    using navitia::type::Mode_e;

    edge_t nearest_car_edge, nearest_walking_edge;
    try {
        //we need to find the nearest edge in the walking and car graph
        nearest_car_edge = nearest_edge(coord, Mode_e::Car);
        nearest_walking_edge = nearest_edge(coord, Mode_e::Walking);
    } catch(navitia::proximitylist::NotFound) {
        return false;
    }

    //we add a new edge linking those 2 edges, with the walking
    //distance between the 2 edges + the time to park (resp. leave)
    auto min_dist = get_min_distance(*this, coord, nearest_walking_edge, nearest_car_edge);
    vertex_t walking_v = std::get<1>(min_dist);
    vertex_t car_v = std::get<2>(min_dist);
    time_duration dur_between_edges = seconds(std::get<0>(min_dist) / default_speed[Mode_e::Walking]);

    Edge edge;

    //arbitrarily we assume the way is the walking way
    edge.way_idx = graph[nearest_walking_edge].way_idx;

    // time to walk between the edges + time needed to leave the parking
    edge.duration = dur_between_edges + default_time_parking_leave;
    add_edge(walking_v, car_v, edge, graph);

    // time needed to park the car + time to walk between the edges
    edge.duration = dur_between_edges + default_time_parking_park;
    add_edge(car_v, walking_v, edge, graph);

    return true;
}

GeoRef::~GeoRef() {
    for(POIType* poi_type : poitypes) {
        delete poi_type;
    }
    for(POI* poi: pois) {
        delete poi;
    }
    for(Way* way: ways) {
        delete way;
    }
    for(Admin* admin: admins) {
        delete admin;
    }

}


std::vector<type::idx_t> POI::get(type::Type_e type, const GeoRef &) const {
    switch(type) {
    case type::Type_e::POIType : return {poitype_idx}; break;
    default : return {};
    }
}

std::vector<type::idx_t> POIType::get(type::Type_e type, const GeoRef & data) const {
    std::vector<type::idx_t> result;
    switch(type) {
    case type::Type_e::POI:
        for(const POI* elem : data.pois) {
            if(elem->poitype_idx == idx) {
                result.push_back(elem->idx);
            }
        }
        break;
    default : break;
    }
    return result;
}

}}
