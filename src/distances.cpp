#include "distances.h"

#ifdef DEBUG_MODE
using std::cout;
using std::endl;
void Distances::DebugPrint() {
    cout<<"##DISTANCE_DEBUG:"<<endl;
    for(auto &[from_to, d] : dists) {
        auto [from, to] = from_to;
        cout<<std::fixed<<std::setprecision(5)
            <<"from_city is "<<from<<endl
            <<"to_city is "<<to<<endl
            <<"distance is "<<d<<endl
            <<"##"<<endl;
    }
    cout<<"DISTANCE_DEBUG##"<<endl<<endl;
}
#endif


Distances::Distances(const std::string &path_to_xlsx) {
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

        auto from = cell.get_num_value<unsigned int>();
        cell.next();

        auto to = cell.get_num_value<unsigned int>();
        cell.next();

        if(cell.get_raw_iterator()==row.cells().end()) continue;

        auto d = cell.get_num_value<double>();
        cell.next();

        if(from && to && d) {
            dists[{from.value(),to.value()}] = d.value() / 1000; 
        }     
    }
}

std::optional<double> Distances::GetDistance(unsigned int from, unsigned int to) {
    if (from == to) {
        return 0.;
    }

    auto it = dists.find({from,to});
    if (it != dists.end()) {
        return {it->second};
    }
    return std::nullopt;
}