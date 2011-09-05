#include "pool.h"
#include <iostream>
#include "configuration.h"
#include <log4cplus/logger.h>
#include <log4cplus/configurator.h>

Pool::Pool(){
	Configuration * conf = Configuration::get();
    std::string initFileName = conf->get_string("path") + conf->get_string("application") + ".ini";

	log4cplus::PropertyConfigurator::doConfigure(LOG4CPLUS_TEXT(initFileName));
    log4cplus::Logger logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
    LOG4CPLUS_DEBUG(logger, "chargement de la configuration");

    conf->load_ini(initFileName);

	this->nb_threads = conf->get_as<int>("GENERAL","NbThread", 4);

    navitia_list.push_back(new Navitia("http://localhost:81/1", 8));
    std::make_heap(navitia_list.begin(), navitia_list.end(), Sorter());
}


void Pool::add_navitia(Navitia* navitia){
    log4cplus::Logger logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
    LOG4CPLUS_DEBUG(logger, "ajout du navitia " + navitia->url);
    mutex.lock();
    navitia_list.push_back(navitia);
    std::push_heap(navitia_list.begin(), navitia_list.end(), Sorter());
    mutex.unlock();
}

void Pool::remove_navitia(const Navitia& navitia){
    log4cplus::Logger logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
    LOG4CPLUS_DEBUG(logger, "suppression du navitia" + navitia.url);
    mutex.lock();
    std::deque<Navitia*>::iterator it = std::find_if(navitia_list.begin(), navitia_list.end(), Comparer(navitia));
    if(it == navitia_list.end()){
        mutex.unlock();
        LOG4CPLUS_DEBUG(logger, "navitia : " + navitia.url + " introuvable");
        return;
    }
    const Navitia* nav = *it;
    navitia_list.erase(it);
    std::sort_heap(navitia_list.begin(), navitia_list.end(), Sorter());
    mutex.unlock();
    while(nav->current_thread > 0){
        LOG4CPLUS_DEBUG(logger, "attente avant suppression de: " + navitia.url);
        usleep(2);
    }
    delete nav;
    //utilisé un thread pour détruire le navitia quand celui ci ne serat plus utilisé?
}
