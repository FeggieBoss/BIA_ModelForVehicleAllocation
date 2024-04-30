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
    void DebugPrint() const;
    #endif
};

class Trucks {
private:
    std::vector<Truck> trucks_;
public:
    Trucks(){};
    Trucks(const std::string &path_to_xlsx);
    Trucks(const std::vector<Truck>& trucks);
    Trucks(const Trucks& other);

    std::vector<Truck>::iterator begin() { // NOLINT
        return trucks_.begin();
    }

    std::vector<Truck>::iterator end() { // NOLINT
        return trucks_.end();
    }

    Truck GetTruck(size_t ind) const;
    const Truck& GetTruckConst(size_t ind) const;
    Truck& GetTruckRef(size_t ind);

    size_t Size() const;

    #ifdef DEBUG_MODE
    void DebugPrint() const;
    #endif
};

#endif // DEFINE_TRUCKS_H