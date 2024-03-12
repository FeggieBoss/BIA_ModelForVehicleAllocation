#include "xlsx_cell.h"

PARAM_NAME GetParamName(const std::string &str_param_name) {
    if(str_param_name == "VELOCITY") {
        return PARAM_NAME::VELOCITY;
    } else if(str_param_name == "TRIP_KM_PRICE") {
        return PARAM_NAME::TRIP_KM_PRICE;
    } else if(str_param_name == "TRIP_HOUR_PRICE") {
        return PARAM_NAME::TRIP_HOUR_PRICE;
    } else if(str_param_name == "IDLE_RUN_KM_PRICE") {
        return PARAM_NAME::IDLE_RUN_KM_PRICE;
    } else if(str_param_name == "IDLE_RUN_HOUR_PRICE") {
        return PARAM_NAME::IDLE_RUN_HOUR_PRICE;
    } else if(str_param_name == "REST_HOUR_PRICE") {
        return PARAM_NAME::REST_HOUR_PRICE;
    } else {
        std::cerr << "unexpected PARAM_NAME("<<str_param_name<<")" << std::endl;
        exit(1);
    }

    return PARAM_NAME::VELOCITY;
}

int GetMaskLoadType(const std::string &str_load_type) {
    if(str_load_type == "Задняя") {
        return static_cast<int>(LOAD_TYPE::REAR);
    } else if(str_load_type == "Полная") {
        return static_cast<int>(LOAD_TYPE::FULL);
    } else if(str_load_type == "Боковая, задняя") {
        return (static_cast<int>(LOAD_TYPE::SIDE) | static_cast<int>(LOAD_TYPE::REAR));
    } else if(str_load_type == "Верхняя, задняя") {
        return (static_cast<int>(LOAD_TYPE::TOP) | static_cast<int>(LOAD_TYPE::REAR));
    } else {
        std::cerr << "unexpected LOAD_TYPE("<<str_load_type<<")" << std::endl;
        exit(1);
    }

    return static_cast<int>(LOAD_TYPE::REAR);
}

int GetMaskTrailerType(const std::string &str_trailer_type) {
    if(str_trailer_type == "Тент") {
        return static_cast<int>(TRAILER_TYPE::AWNING);
    } else if(str_trailer_type == "Фургон") {
        return static_cast<int>(TRAILER_TYPE::VAN);
    } else if(str_trailer_type == "Термос") {
        return static_cast<int>(TRAILER_TYPE::THERMOS);
    } else if(str_trailer_type == "Рефриджератор") {
        return static_cast<int>(TRAILER_TYPE::REFRIDGERATOR);
    } else if(str_trailer_type == "Изотермический") {
        return static_cast<int>(TRAILER_TYPE::ISOTHERMAL);
    } else {
        std::cerr << "unexpected LOAD_TYPE("<<str_trailer_type<<")" << std::endl;
        exit(1);
    }

    return static_cast<int>(TRAILER_TYPE::AWNING);
}

int GetFullMaskLoadType() {
    return (
        static_cast<int>(LOAD_TYPE::REAR) |
        static_cast<int>(LOAD_TYPE::SIDE) |
        static_cast<int>(LOAD_TYPE::TOP)  | 
        static_cast<int>(LOAD_TYPE::FULL)
    );
}

int GetFullMaskTrailerType() {
    return (
        static_cast<int>(TRAILER_TYPE::AWNING)        |
        static_cast<int>(TRAILER_TYPE::VAN)           |
        static_cast<int>(TRAILER_TYPE::REFRIDGERATOR) | 
        static_cast<int>(TRAILER_TYPE::THERMOS)       |
        static_cast<int>(TRAILER_TYPE::ISOTHERMAL)
    );
}

bool IsExecutableBy(
    int truck_mask_load_type, 
    int truck_mask_trailer_type, 
    int order_mask_load_type, 
    int order_mask_trailer_type) 
{
    bool good_load_type = (truck_mask_load_type & order_mask_load_type) == truck_mask_load_type;    
    bool good_trailer_type = (truck_mask_trailer_type & order_mask_trailer_type) == truck_mask_trailer_type;
    return good_load_type & good_trailer_type;
}


XLSXcell::XLSXcell(XLRowDataIterator cell): cell(cell) {}

XLRowDataIterator XLSXcell::get_raw_iterator() {
    return cell;
}

void XLSXcell::next() {
    ++cell;
}

XLValueType XLSXcell::get_type() {
    return (cell->value()).type();
}

template<typename T>
T XLSXcell::get_value() {
    return (cell->value()).get<T>();
}

template<typename T>
std::optional<T> XLSXcell::get_num_value() {
    XLCellValueProxy &v = cell->value();

    T x;
    switch (v.type())
    {
    case XLValueType::Float: {
        x = v.get<float>();
        break;
    }
    
    case XLValueType::Integer: {
        x = v.get<int>();
        break;
    }
    
    default:
        #ifdef DEBUG_MODE
        std::cerr << "not expected type(" << v.typeAsString() << ") in set_num_value" << std::endl;
        std::cerr << v.get<std::string>()<<std::endl;
        #endif
        return std::nullopt;
    }
    return {x};
}

std::optional<unsigned int> XLSXcell::get_time_value() {
    XLCellValueProxy &v = cell->value();

    int x;
    switch (v.type())
    {
    case XLValueType::Float: {
        tm t = v.get<XLDateTime>().tm();
        x = (mktime(&t)+59)/60;
        break;
    }
    
    case XLValueType::Integer: {
        long long y = (1LL * (v.get<int>()) * 24 * 60 * 60 - serial_unix_offset);
        tm t = *std::gmtime(reinterpret_cast<time_t*>(&y));
        x = (mktime(&t)+59)/60;
        //std::cout<<ctime(reinterpret_cast<time_t*>(&y))<<std::endl;
        break;
    }

    default:
        #ifdef DEBUG_MODE
        std::cerr << "not expected type (" << v.typeAsString() << ") in set_time_value" << std::endl;
        std::cerr << v.get<std::string>()<<std::endl;
        #endif
        return std::nullopt;
    }
    return {x};
}

template std::optional<double> XLSXcell::get_num_value();
template std::optional<unsigned int> XLSXcell::get_num_value();
template std::string XLSXcell::get_value();