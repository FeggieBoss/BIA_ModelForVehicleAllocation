#ifndef _MAIN
#define _MAIN

#include <string>
#include <vector>
#include <map>
#include <climits>

const long long serial_unix_offset = 2208988800 + 2*24*60*60 + 3*60*60; // seconds between Jan-1-1900(Serial Date) and Jan-1-1970(UNIX Time)

struct params_t {
    double speed = 0;
    double free_km_cost = 0;
    double free_hour_cost = 0;
    double wait_cost = 0;
    double duty_km_cost = 0;
    double duty_hour_cost = 0;
};
struct truck {
    int truck_id;
    std::string type;
    int init_time;
    int init_city;
};

struct order {
    int order_id;
    int obligation;
    int start_time;
    int finish_time;
    int from_city;
    int to_city;
    std::string type;
    double distance;
    double revenue;
};

struct data_t {
    int n = 0;
    int m_real = 0;
    int lst_city = 0;
    int min_time = INT_MAX;
    params_t params;
    int inf_d = 1e6;
    std::map<std::pair<int,int>, double> dists; 
    std::vector<truck> trucks = {{0,"0",0,0}};
    std::vector<order> orders = {{0,0,0,0,0,0,"0",0,0}};
};

#endif // _MAIN
