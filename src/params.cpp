#include "params.h"

#ifdef DEBUG_MODE
using std::cout;
using std::endl;
void Params::DebugPrint() {
    cout<<std::fixed<<std::setprecision(5)
        <<"##PARAMS_DEBUG:"<<endl
        <<"speed is "<<speed<<endl
        <<"duty_km_cost is "<<duty_km_cost<<endl
        <<"duty_hour_cost is "<<duty_hour_cost<<endl
        <<"free_km_cost is "<<free_km_cost<<endl
        <<"free_hour_cost is "<<free_hour_cost<<endl
        <<"wait_cost is "<<wait_cost<<endl
        <<"PARAMS_DEBUG##"<<endl<<endl;
}
#endif

Params::Params(const std::string &path_to_xlsx) {
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

        cell.next(); // skipping description cell

        std::string str_param_name = cell.get_value<std::string>(); // reading name
        cell.next(); // moving to value
        
        switch (GetParamName(str_param_name)) { // reading value
            case PARAM_NAME::VELOCITY: {
                speed = cell.get_num_value<double>().value();
            }
            case PARAM_NAME::TRIP_KM_PRICE: {
                duty_km_cost = cell.get_num_value<double>().value();
            }
            case PARAM_NAME::TRIP_HOUR_PRICE: {
                duty_hour_cost = cell.get_num_value<double>().value();
            }
            case PARAM_NAME::IDLE_RUN_KM_PRICE: {
                free_km_cost = cell.get_num_value<double>().value();
            }
            case PARAM_NAME::IDLE_RUN_HOUR_PRICE: {
                free_hour_cost = cell.get_num_value<double>().value();
            }
            case PARAM_NAME::REST_HOUR_PRICE: {
                wait_cost = cell.get_num_value<double>().value();
            }
            default: {
                break;
            }
        }
    }
}

Params::Params(
        double speed, 
        double free_km_cost, 
        double free_hour_cost,
        double wait_cost,
        double duty_km_cost,
        double duty_hour_cost
) : speed(speed),
    free_km_cost(free_km_cost),
    free_hour_cost(free_hour_cost),
    wait_cost(wait_cost),
    duty_km_cost(duty_km_cost),
    duty_hour_cost(duty_hour_cost) {}