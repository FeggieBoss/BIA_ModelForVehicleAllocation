#ifndef DEFINE_XLSX_CELL_H
#define DEFINE_XLSX_CELL_H

#include "main.h"

#include <OpenXLSX.hpp>

#include <optional>
#include <cstring>

using namespace OpenXLSX;

//const long long serial_unix_offset = 2208988800 + 2*24*60*60 + 3*60*60; // seconds between Jan-1-1900(Serial Date) and Jan-1-1970(UNIX Time)

enum class PARAM_NAME {
    VELOCITY,
    TRIP_KM_PRICE,
    TRIP_HOUR_PRICE,
    IDLE_RUN_KM_PRICE,
    IDLE_RUN_HOUR_PRICE,
    REST_HOUR_PRICE,
};

constexpr size_t LOAD_TYPE_COUNT = 4;
enum class LOAD_TYPE {
    REAR  = (1<<0),
    SIDE  = (1<<1),
    TOP   = (1<<2),
    FULL  = (1<<3),
};

enum class TRAILER_TYPE {
    AWNING        = (1<<0),
    VAN           = (1<<1),
    REFRIDGERATOR = (1<<2),
    THERMOS       = (1<<3),
    ISOTHERMAL    = (1<<4),
};

PARAM_NAME GetParamName(const std::string &str_param_name);
int GetMaskLoadType(const std::string &str_load_type);
int GetMaskTrailerType(const std::string &str_trailer_type);
int GetFullMaskLoadType();
int GetFullMaskTrailerType();
bool IsExecutableBy(
    int truck_mask_load_type, 
    int truck_mask_trailer_type, 
    int order_mask_load_type, 
    int order_mask_trailer_type);

class XLSXcell {
private:
    XLRowDataIterator cell;
public:
    XLSXcell() = default;
    XLSXcell(XLRowDataIterator cell);

    XLRowDataIterator get_raw_iterator();
    
    void next();
    
    XLValueType get_type();

    std::optional<unsigned int> get_time_value();
    
    template<typename T>
    T get_value();

    template<typename T>
    std::optional<T> get_num_value();
};

#endif // DEFINE_XLSX_CELL_H