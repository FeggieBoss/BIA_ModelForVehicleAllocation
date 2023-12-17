#ifndef _PARSE
#define _PARSE

#include <OpenXLSX.hpp>

#include "main.h"

void parse_params(
    int &speed,  
    int &free_km_cost, 
    int &free_hour_cost,
    int &duty_km_cost, 
    int &duty_hour_cost,
    int &wait_cost
);
void parse_trucks(std::vector<truck> &trucks);
void parse_orders(std::vector<order> &orders);
void parse_distances(std::map<std::pair<int,int>, int> &dists);
void parse(
    int &speed,  
    int &free_km_cost, 
    int &free_hour_cost,
    int &duty_km_cost, 
    int &duty_hour_cost,
    int &wait_cost,
    int &n, 
    std::vector<truck> &trucks, 
    int &m_real,
    int &m_fake,
    int &m_stop,
    std::vector<order> &orders,
    std::map<std::pair<int,int>, int> &dists,
    int &lst_city
);

#endif // _PARSE
