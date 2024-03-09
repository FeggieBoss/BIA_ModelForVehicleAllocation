#include "orders.h"

#ifdef DEBUG_MODE
using std::cout;
using std::endl;

void Order::DebugPrint() {
    cout<<std::fixed<<std::setprecision(5)
        <<"##ORDER_DEBUG:"<<endl
        <<"order_id is "<<order_id<<endl
        <<"obligation is "<<obligation<<endl
        <<"start_time is "<<start_time<<endl
        <<"finish_time is "<<finish_time<<endl
        <<"from_city is "<<from_city<<endl
        <<"to_city is "<<to_city<<endl
        <<"mask_load_type is "<<mask_load_type<<endl
        <<"mask_trailer_type is "<<mask_trailer_type<<endl
        <<"distance is "<<distance<<endl
        <<"revenue is "<<revenue<<endl
        <<"ORDER_DEBUG##"<<endl<<endl;
}
#endif

#ifdef DEBUG_MODE
using std::cout;
using std::endl;
void Orders::DebugPrint() {
    for(auto &order : orders) {
        order.DebugPrint();
    }
}
#endif

Order::Order(
    unsigned int order_id,
    bool obligation,
    unsigned int start_time,
    unsigned int finish_time,
    unsigned int from_city,
    unsigned int to_city,
    const std::string &str_load_type, 
    const std::string &str_trailer_type,
    double distance,
    double revenue
) : order_id(order_id),
    obligation(obligation),
    start_time(start_time),
    finish_time(finish_time),
    from_city(from_city),
    to_city(to_city),
    distance(distance),
    revenue(revenue)
{
    mask_load_type = get_mask_load_type(str_load_type);
    mask_trailer_type = get_mask_trailer_type(str_trailer_type);
}


Orders::Orders(const std::string &path_to_xlsx) {
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

        auto order_id = cell.get_num_value<unsigned int>();
        cell.next();

        bool obligation = cell.get_value<std::string>() == "да";
        cell.next();

        auto start_time = cell.get_time_value();
        cell.next();

        auto finish_time = cell.get_time_value();
        cell.next();

        auto from_city = cell.get_num_value<unsigned int>();
        cell.next();

        auto to_city = cell.get_num_value<unsigned int>();
        cell.next();

        std::string load_type = cell.get_value<std::string>();
        cell.next();

        std::string trailer_type = cell.get_value<std::string>();
        cell.next();

        auto distance = cell.get_num_value<double>();
        cell.next();

        auto revenue = cell.get_num_value<double>();
        cell.next();

        if(order_id && start_time && finish_time && from_city && to_city && distance && revenue) {
            orders.push_back(Order{
                order_id.value(),
                obligation,
                start_time.value(),
                finish_time.value(),
                from_city.value(),
                to_city.value(),
                load_type,
                trailer_type,
                distance.value(),
                revenue.value(),
            });
        }
    }
}

Order Orders::GetOrder(size_t ind) {
    if (ind >= orders.size()) {
        std::cerr << "GetOrder(" << ind << "): out of bounds id > size(" << orders.size() << ")" << std::endl;
        exit(1);
    }
    return orders[ind];
}

size_t Orders::Size() {
    return orders.size();
}