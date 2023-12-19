#ifndef _PARSE
#define _PARSE

#include <OpenXLSX.hpp>
#include <ctime>

#include "main.h"

void parse_params(params_t &params);
void parse_trucks(std::vector<truck> &trucks);
void parse_orders(std::vector<order> &orders);
void parse_distances(std::map<std::pair<int,int>, double> &dists);
void parse(data_t &data);

#endif // _PARSE
