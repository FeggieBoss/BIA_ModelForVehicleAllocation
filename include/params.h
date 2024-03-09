#ifndef DEFINE_PARAMS_H
#define DEFINE_PARAMS_H

#include "main.h"
#include "xlsx_cell.h"

#include <string>

class Params {
public:
    double speed = 0;
    double free_km_cost = 0;
    double free_hour_cost = 0;
    double wait_cost = 0;
    double duty_km_cost = 0;
    double duty_hour_cost = 0;

    Params(){};
    Params(const std::string &path_to_xlsx);
    Params(
        double speed, 
        double free_km_cost, 
        double free_hour_cost,
        double wait_cost,
        double duty_km_cost,
        double duty_hour_cost
    );
    #ifdef DEBUG_MODE
    void DebugPrint();
    #endif
};

#endif // DEFINE_PARAMS_H