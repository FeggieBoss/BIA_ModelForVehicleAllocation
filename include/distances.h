#ifndef DEFINE_DISTANCES_H
#define DEFINE_DISTANCES_H

#include "main.h"
#include "xlsx_cell.h"

#include <OpenXLSX.hpp>

#include <string>
#include <map>
#include <optional>

class Distances {
public:
    std::map<std::pair<unsigned int,unsigned int>, double> dists; 

    Distances(){};
    Distances(const std::string &path_to_xlsx);

    std::optional<double> GetDistance(unsigned int from, unsigned int to);

    #ifdef DEBUG_MODE
    void DebugPrint();
    #endif
};

#endif // DEFINE_DISTANCES_H