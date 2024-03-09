#ifndef _MODEL
#define _MODEL

#define NO_LONG_EDGES 1000
//#define COMPLETE_GRAPH

#include "main.h"

#include <iostream>
#include <unordered_map>
#include <iomanip>

std::vector<int> solve(HighsModel &model);
model_t create_model(data_t &data);
bool checker(std::vector<std::vector<int>> &ans, std::vector<truck> &trucks, std::vector<order> &orders, std::map<std::pair<int,int>, double> &dists);

#endif // _MODEL