#ifndef _MODEL
#define _MODEL

#include "main.h"
#include "Highs.h"

#include <iostream>
#include <iomanip>

std::vector<std::vector<int>> solve(HighsModel &model);
HighsModel create_model(data_t &data);
bool checker(std::vector<std::vector<int>> &ans, std::vector<truck> &trucks, std::vector<order> &orders, std::map<std::pair<int,int>, double> &dists);

#endif // _MODEL