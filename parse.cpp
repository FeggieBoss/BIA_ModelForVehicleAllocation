
#include "parse.h"

using namespace OpenXLSX;
using std::cout;
using std::endl;

static int min_time = INT32_MAX;

template<typename T>
void set_num_value(XLCellValueProxy &v, T *ptr, bool &f) {
    T x;
    switch (v.type())
    {
    case XLValueType::Float:
        x = v.get<float>();
        break;
    
    case XLValueType::Integer:
        x = v.get<int>();
        break;
    
    default:
        //std::cerr << "not expected type " << v.typeAsString() << " in set_num_value" << std::endl;
        //std::cerr << v.get<std::string>()<<std::endl;
        f = true;
        return;
    }
    *ptr = x;
}


void set_time_value(XLCellValueProxy &v, int *ptr, bool &f) {
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
        //std::cerr << "not expected type " << v.typeAsString() << " in set_time_value" << std::endl;
        //std::cerr << v.get<std::string>()<<std::endl;
        f = true;
        return;
    }
    *ptr = x;
}

void parse_params(params_t &params) {
    XLDocument doc;
    doc.open("./../case_1/params.xlsx");
    auto wks = doc.workbook().worksheet("Sheet1");
    bool header = 1;
    for (auto& row : wks.rows()) {
        if(header) {
            header=0;
            continue;
        }
        
        auto it = row.cells().begin(); 
        if((*it).value().type()==XLValueType::Empty) {
            break;
        }
        ++it;
        std::string name = ((*it).value()).get<std::string>(); ++it;

        bool fl = false;
        if(name=="VELOCITY") {
            set_num_value((*it).value(), &params.speed, fl);
        } else if(name=="TRIP_KM_PRICE") {
            set_num_value((*it).value(), &params.duty_km_cost, fl);
        } else if(name=="TRIP_HOUR_PRICE") {
            set_num_value((*it).value(), &params.duty_hour_cost, fl);
        } else if(name=="IDLE_RUN_KM_PRICE") {
            set_num_value((*it).value(), &params.free_km_cost, fl);
        } else if(name=="IDLE_RUN_HOUR_PRICE") {
            set_num_value((*it).value(), &params.free_hour_cost, fl);
        } else if(name=="REST_HOUR_PRICE") {
            set_num_value((*it).value(), &params.wait_cost, fl);
        }

        if(fl) {
            exit(1);
        }
    }
}

void parse_trucks(std::vector<truck> &trucks) {
    XLDocument doc;
    doc.open("./../case_1/trucks.xlsx");
    auto wks = doc.workbook().worksheet("Sheet1");
    bool header = 1;
    for (auto& row : wks.rows()) {
        if(header) {
            header=0;
            continue;
        }

        auto it = row.cells().begin();
        if((*it).value().type()==XLValueType::Empty) {
            break;
        }

        bool fl = false;

        trucks.push_back(truck{});
        
        set_num_value((*it).value(), &trucks.back().truck_id, fl); ++it;

        trucks.back().type = ((*it).value()).get<std::string>(); ++it;
        trucks.back().type += ", ";
        trucks.back().type += ((*it).value()).get<std::string>(); ++it;

        set_time_value((*it).value(), &trucks.back().init_time, fl); ++it;

        set_num_value((*it).value(), &trucks.back().init_city, fl);

        if(fl) {
            trucks.pop_back();
            continue;
        }
    }
}

void parse_orders(std::vector<order> &orders) {
    XLDocument doc;
    doc.open("./../case_1/orders.xlsx");
    auto wks = doc.workbook().worksheet("Sheet1");
    bool header = 1;
    for (auto& row : wks.rows()) {
        if(header) {
            header=0;
            continue;
        }

        auto it = row.cells().begin();
        if((*it).value().type()==XLValueType::Empty) {
            break;
        }
        
        orders.push_back(order{});

        bool fl = false;

        set_num_value((*it).value(), &orders.back().order_id, fl);
        ++it;

        orders.back().obligation = ((*it).value()).get<std::string>()=="да"; ++it;

        set_time_value((*it).value(), &orders.back().start_time, fl); ++it;
        set_time_value((*it).value(), &orders.back().finish_time, fl); ++it;

        set_num_value((*it).value(), &orders.back().from_city, fl);
        ++it;

        set_num_value((*it).value(), &orders.back().to_city, fl);
        ++it;

        orders.back().type = ((*it).value()).get<std::string>(); ++it;
        orders.back().type += ", ";
        orders.back().type += ((*it).value()).get<std::string>(); ++it;


        set_num_value((*it).value(), &orders.back().distance, fl);
        ++it;

        set_num_value((*it).value(), &orders.back().revenue, fl);

        if(fl || orders.back().start_time >= orders.back().finish_time) {
            orders.pop_back();
            continue;
        }
    }
}

void parse_distances(std::map<std::pair<int,int>, double> &dists) {
    XLDocument doc;
    doc.open("./../case_1/distances.xlsx");
    auto wks = doc.workbook().worksheet("Sheet1");
    bool header = 1;
    for (auto& row : wks.rows()) {
        if(header) {
            header=0;
            continue;
        }

        auto it = row.cells().begin();
        if((*it).value().type()==XLValueType::Empty) {
            break;
        }

        int from, to;
        double d;

        bool fl = false;

        set_num_value((*it).value(), &from, fl); ++it;
        set_num_value((*it).value(), &to, fl); ++it;
        if(it==row.cells().end()) continue;
        set_num_value((*it).value(), &d, fl);

        if(fl) continue;

        dists[{from,to}] = d / 1000;        
    }
}


void parse(data_t &data) {
    parse_params(data.params);
    data.params.speed *= 2;
    //cout<<std::fixed<<std::setprecision(5)<<speed<<" "<<duty_km_cost<<" "<<duty_hour_cost<<" "<<free_km_cost<<" "<<free_hour_cost<<" "<<wait_cost<<endl;
    
    parse_trucks(data.trucks);
    for(auto&el : data.trucks) {
        if(el.truck_id==0) continue;
        data.min_time = std::min(data.min_time, el.init_time);
        //std::cout<<el.truck_id<<" "<<el.type<<" "<<el.init_time<<" "<<el.init_city<<std::endl; 
    }
    data.trucks.resize(71);
    data.n = data.trucks.size() - 1;

    parse_orders(data.orders);
    for(auto&el : data.orders) {
        if(el.order_id==0) continue;
        data.min_time = std::min(data.min_time, el.start_time);
        //std::cout<<el.order_id<<" "<<el.obligation<<" "<<el.start_time<<" "<<el.finish_time<<" "<<el.from_city<<" "<<el.to_city<<" "<<el.type<<" "<<el.distance<<" "<<el.revenue<<std::endl;
    }
    data.orders.resize(184);
    data.m_real = data.orders.size() - 1;

    parse_distances(data.dists);
    for(auto&el : data.dists) {
        data.lst_city = std::max(data.lst_city, el.first.first); 
        data.lst_city = std::max(data.lst_city, el.first.second); 
        //cout<<"("<<el.first.first<<","<<el.first.second<<") "<<el.second<<endl;
    }

    for(auto&el : data.trucks) {
        if(el.truck_id==0) continue;
        el.init_time -= data.min_time;
    }
    for(auto&el : data.orders) {
        if(el.order_id==0) continue;
        el.start_time -= data.min_time;
        el.finish_time -= data.min_time;
    }
}