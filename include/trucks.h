#ifndef DEFINE_TRUCKS_H
#define DEFINE_TRUCKS_H

#include "main.h"
#include "xlsx_cell.h"

#include <OpenXLSX.hpp>

#include <string>
#include <vector>

class Truck {
public:
    unsigned int truck_id = 0;
    int mask_load_type = 0;
    int mask_trailer_type = 0;
    unsigned int init_time = 0;
    unsigned int init_city = 0;
public:
    Truck(){};
    Truck(
        unsigned int truck_id, 
        const std::string &str_load_type, 
        const std::string &str_trailer_type,
        unsigned int init_time,
        unsigned int init_city
    );
    #ifdef DEBUG_MODE
    void DebugPrint();
    #endif
};

class Trucks {
private:
    std::vector<Truck> trucks;
public:
    Trucks(){};
    Trucks(const std::string &path_to_xlsx);

    std::vector<Truck>::iterator begin() { // NOLINT
        return trucks.begin();
    }

    std::vector<Truck>::iterator end() { // NOLINT
        return trucks.end();
    }

    Truck GetTruck(size_t ind);
    size_t Size();

    #ifdef DEBUG_MODE
    void DebugPrint();
    #endif
};

#endif // DEFINE_TRUCKS_H