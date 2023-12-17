#ifndef _MAIN
#define _MAIN

#include <string>

const int revenue_fixer = 1e5;
const int params_fixer = 1e5;
const int distance_fixer = 10;
const long long serial_unix_offset = 2208988800 + 2*24*60*60 + 3*60*60; // seconds between Jan-1-1900(Serial Date) and Jan-1-1970(UNIX Time)

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
    int distance;
    int revenue;
};

#endif // _MAIN
