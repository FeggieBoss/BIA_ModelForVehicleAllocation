#include "trucks.h"

#ifdef DEBUG_MODE
using std::cout;
using std::endl;
void Truck::DebugPrint() const {
    cout<<std::fixed<<std::setprecision(5)
        <<"##TRUCK_DEBUG:"<<endl
        <<"truck_id is "<<truck_id<<endl
        <<"MASK_LOAD_TYPE is "<<mask_load_type<<endl
        <<"MASK_TRAILER_TYPE is "<<mask_trailer_type<<endl
        <<"init_time is "<<init_time<<endl
        <<"init_city is "<<init_city<<endl
        <<"TRUCK_DEBUG##"<<endl<<endl;
}
#endif

#ifdef DEBUG_MODE
using std::cout;
using std::endl;
void Trucks::DebugPrint() const {
    for(auto &truck : trucks_) {
        truck.DebugPrint();
    }
}
#endif

Truck::Truck(
    unsigned int truck_id, 
    const std::string &str_load_type, 
    const std::string &str_trailer_type,
    unsigned int init_time,
    unsigned int init_city
) : truck_id(truck_id),
    init_time(init_time),
    init_city(init_city) 
{
    mask_load_type = GetMaskLoadType(str_load_type);
    mask_trailer_type = GetMaskTrailerType(str_trailer_type);
}


Trucks::Trucks(const std::string &path_to_xlsx) {
    XLDocument doc;
    doc.open(path_to_xlsx);
    auto worksheet = doc.workbook().worksheet("Sheet1");

    bool is_need_skip_header = 1;
    for (auto& row : worksheet.rows()) {
        if(is_need_skip_header) {
            is_need_skip_header = false;
            continue;
        }
        
        XLSXcell cell(row.cells().begin());
        if(cell.get_type()==XLValueType::Empty) {
            break;
        }

        auto truck_id = cell.get_num_value<unsigned int>();
        cell.next();

        std::string load_type = cell.get_value<std::string>();
        cell.next();

        std::string trailer_type = cell.get_value<std::string>();
        cell.next();

        auto init_time = cell.get_time_value();
        cell.next();

        auto init_city = cell.get_num_value<unsigned int>();
        cell.next();

        if(truck_id && init_time && init_city) {
            trucks_.push_back(Truck{
                truck_id.value(),
                load_type,
                trailer_type,
                init_time.value(),
                init_city.value(),
            });
        }
    }
}


Trucks::Trucks(const std::vector<Truck>& trucks): trucks_(trucks) {}

Trucks::Trucks(const Trucks& other): trucks_(other.trucks_) {}

Truck Trucks::GetTruck(size_t ind) const {
    if (ind >= trucks_.size()) {
        std::cerr << "GetTruck(" << ind << "): out of bounds id > size(" << trucks_.size() << ")" << std::endl;
        exit(1);
    }
    return trucks_[ind];
}

const Truck& Trucks::GetTruckConst(size_t ind) const {
    if (ind >= trucks_.size()) {
        std::cerr << "GetTruck(" << ind << "): out of bounds id > size(" << trucks_.size() << ")" << std::endl;
        exit(1);
    }
    return trucks_[ind];
}


Truck& Trucks::GetTruckRef(size_t ind) {
    if (ind >= trucks_.size()) {
        std::cerr << "GetTruck(" << ind << "): out of bounds id > size(" << trucks_.size() << ")" << std::endl;
        exit(1);
    }
    return trucks_[ind];
}

size_t Trucks::Size() const {
    return trucks_.size();
}