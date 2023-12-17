#include "Highs.h"

#include "parse.h"
#include "main.h"

#include <cassert>
#include <vector>
#include <ctime>
#include <iomanip>
#include <map>

using std::cout;
using std::endl;

int n = 2;
int m_real = 4;
int m_fake = n;
int m_stop = 1;
int m = m_real + m_fake + m_stop;
int lst_city = 0;

int speed = 1;
int free_km_cost = 1;
int free_hour_cost = 0;
int wait_cost = 1;
int duty_km_cost = 0;
int duty_hour_cost = 0;
int inf = 1e6;
int inf_d = 1000;


int get_3d_index(int i, int j, int k) {
    return i+(j+(k-1)*m - 1)*n - 1;
}

std::map<std::pair<int,int>, int> dists; 
std::vector<std::vector<int>> d; // = {
//     {0,  0,  0,  0,  0,  0,  0,  0,   0,  0 , 0},
//     {0,  0, 10, 30, 30, 30, 30, 60, inf, inf, 0},
//     {0, 10,  0, 45, 30, 30, 30, 60, inf, inf, 0},
//     {0, 30, 45,  0, 10, 20, 30, 30, inf, inf, 0},
//     {0, 30, 30, 10,  0, 10, 20, 30, inf, inf, 0},
//     {0, 30, 30, 20, 10,  0, 10, 30, inf, inf, 0},
//     {0, 30, 30, 30, 20, 10,  0, 30, inf, inf, 0},
//     {0, 60, 60, 30, 30, 30, 30,  0, inf, inf, 0},
//     {0,  0,  0,  0,  0,  0,  0,  0,  0,  inf, 0},
//     {0,  0,  0,  0,  0,  0,  0,  0, inf,  0 , 0},
//     {0,inf,inf,inf,inf,inf,inf,inf, inf, inf, 0},
// };

int get_dist(int from, int to) {
    if(from == to) {
        return 0;
    }

    if(from > lst_city && from < lst_city + n) {
        if(to > lst_city && to < lst_city + n) {
            return 0;
        }
    }

    auto it = dists.find({from,to});
    if(it!=dists.end()) {
        return it->second;
    } else {
        return inf_d;
    }
}


std::vector<truck> trucks = {{0, "0", 0, 0}};
// std::vector<truck> trucks = {
//     {0, 0, 0, "0"}, // 0 -> 1 indexes
//     {1, 8, 0, "0"},
//     {2, 9, 90, "0"}
// };

std::vector<order> orders = {{0,0,0,0,0,0,"0",0,0}};
// std::vector<order> orders = {
//     {0,0,0,0,0,"0",0,0,0}, // 0 -> 1 indexes
//     {0,3,4,35,50,"0",40,10,0},
//     {0,4,5,55,70,"0",10,10,0},
//     {0,6,7,120,180,"0",60,30,0},
//     {0,5,6,120,130,"0",20,10,1},
//     {0,8,1,0,0,"0",0,0,1},      // fake1
//     {0,9,2,90,90,"0",0,0,1},    // fake2
//     {0,1,10,inf,inf,"0",0,0,1}  // stop
// };


std::vector<std::vector<int>> solve(HighsModel &model) {
    Highs highs;
    HighsStatus return_status;
    
    return_status = highs.passModel(model);
    assert(return_status==HighsStatus::kOk);

    const HighsLp& lp = highs.getLp(); 

    return_status = highs.run();
    assert(return_status==HighsStatus::kOk);
    
    const HighsModelStatus& model_status = highs.getModelStatus();
    assert(model_status==HighsModelStatus::kOptimal);
    //cout << "Model status: " << highs.modelStatusToString(model_status) << endl;
    
    const HighsInfo& info = highs.getInfo();
    //cout << "Simplex iteration count: " << info.simplex_iteration_count << endl;
    //cout << "Objective function value: " << info.objective_function_value << endl;
    //cout << "Primal  solution status: " << highs.solutionStatusToString(info.primal_solution_status) << endl;
    //cout << "Dual    solution status: " << highs.solutionStatusToString(info.dual_solution_status) << endl;
    //cout << "Basis: " << highs.basisValidityToString(info.basis_validity) << endl;
    const bool has_values = info.primal_solution_status;
    const bool has_duals = info.dual_solution_status;
    const bool has_basis = info.basis_validity;
    
    const HighsSolution& solution = highs.getSolution();
    const HighsBasis& basis = highs.getBasis();
    
    // for (int col=0; col < lp.num_col_; col++) {
    //     //cout << "Column " << col;
    //     if (has_values) //cout << "; value = " << solution.col_value[col];
    //     if (has_duals) //cout << "; dual = " << solution.col_dual[col];
    //     if (has_basis) //cout << "; status: " << highs.basisStatusToString(basis.col_status[col]);
    //     //cout << endl;
    // }
    // for (int row=0; row < lp.num_row_; row++) {
    //     //cout << "Row    " << row;
    //     if (has_values) //cout << "; value = " << solution.row_value[row];
    //     if (has_duals) //cout << "; dual = " << solution.row_dual[row];
    //     if (has_basis) //cout << "; status: " << highs.basisStatusToString(basis.row_status[row]);
    //     //cout << endl;
    // }

    
    model.lp_.integrality_.resize(lp.num_col_);
    for (int col=0; col < lp.num_col_; col++)
        model.lp_.integrality_[col] = HighsVarType::kInteger;

    highs.passModel(model);
    
    return_status = highs.run();
    assert(return_status==HighsStatus::kOk);
    
    std::vector<std::vector<int>> ans(n);
    for (int col=0; col < lp.num_col_; col++) {
        if (info.primal_solution_status && solution.col_value[col]) {
            cout << "Column " << col;
            cout << "; to_(i="<<(col%n)+1<<")_(j="<<(col/n)%m+1<<")_(k="<<(col/n)/m+1<<")"; 
            if (info.primal_solution_status) cout << "; value = " << solution.col_value[col];
            cout << endl;
            
            if((col/n)%m+1 < m - m_stop + 1)
                ans[col%n].push_back((col/n)%m+1);
        }
    }

    return ans;
}

HighsModel create_model() {
    int any_city = trucks[1].init_city;
    for(int i=1;i<=n;++i) {
        orders.push_back(order{});
        orders.back().obligation = 1;
        orders.back().start_time = orders.back().finish_time = trucks[i].init_time;
        orders.back().to_city = trucks[i].init_city;
        trucks[i].init_city = orders.back().from_city = lst_city+i; // relocating
        orders.back().type = trucks[i].type;
    }
    orders.push_back(order{});
    orders.back().start_time = orders.back().finish_time = inf;
    orders.back().from_city = any_city;
    orders.back().to_city = lst_city+n+1;
    
    cout<<endl;
    cout<<std::fixed<<std::setprecision(5)<<speed<<" "<<duty_km_cost<<" "<<duty_hour_cost<<" "<<free_km_cost<<" "<<free_hour_cost<<" "<<wait_cost<<endl;
    
    cout<<endl;
    for(auto&el : trucks) {
        cout<<el.truck_id<<" "<<el.type<<" "<<el.init_time<<" "<<el.init_city<<endl; 
    }
    
    cout<<endl;
    for(auto&el : orders) {
        cout<<el.order_id<<" "<<el.obligation<<" "<<el.start_time<<" "<<el.finish_time<<" "<<el.from_city<<" "<<el.to_city<<" "<<el.type<<" "<<el.distance<<" "<<el.revenue<<endl;
    }
cout<<"core dumped mfca"<<endl;

    cout<<(n+1)*(m+1)*(m+1)<<endl;
    
    // c^Tx + d subject to L <= Ax <= U; l <= x <= u
    int c[n+1][m+1][m+1];
    int l[n+1][m+1][m+1];
    int u[n+1][m+1][m+1];
    const int nmm = n*m_real+m_real+m_fake+n*m*m;

cout<<"core dumped mfca"<<endl;
    // c 
    for(int i=1;i<=n;++i) {
        for(int j=1;j<=m;++j) {
            for(int k=1;k<=m;++k) {
                c[i][j][k] = 0;

                // revenue sum
                c[i][j][k] += orders[j].revenue; 

                // duty hours
                c[i][j][k] -= 1.0 * (orders[j].finish_time - orders[j].start_time)*duty_hour_cost / 60;

                // duty km
                c[i][j][k] -= orders[j].distance*duty_km_cost;

                if(k>m_real+m_fake) {
                    continue;
                }
                // free hours
                c[i][j][k] -= (1.0 * get_dist(orders[j].to_city,orders[k].from_city)/speed)*free_hour_cost;

                // free km
                c[i][j][k] -= get_dist(orders[j].to_city,orders[k].from_city)*free_km_cost;

                // waiting
                c[i][j][k] -= 1.0 * (orders[k].start_time - (orders[j].finish_time + (1.0 * get_dist(orders[j].to_city,orders[k].from_city)/speed * 60)))*wait_cost / 60;
            }
        }
    }

cout<<"core dumped mfca"<<endl;
    // l,u
    for(int i=1;i<=n;++i) {
        for(int j=1;j<=m;++j) {
            for(int k=1;k<=m;++k) {
                l[i][j][k] = 0;
                u[i][j][k] = (trucks[i].type == orders[j].type) && (j != k);
                if(j>=m_real+1 && j<=m_real+m_fake) { // fake
                    u[i][j][k] &= (m_real+i) == j; // fake was made for him
                }
            }
        }
    }

    HighsModel model;
    model.lp_.num_col_ = n*m*m;
    model.lp_.num_row_ = nmm;

    model.lp_.sense_ = ObjSense::kMaximize;

    model.lp_.offset_ = 0;

cout<<"core dumped mfca"<<endl;
    // model.lp_.col_cost_
    model.lp_.col_cost_.resize(n*m*m);
    for(int i=1;i<=n;++i) {
        for(int j=1;j<=m;++j) {
            for(int k=1;k<=m;++k) {
                model.lp_.col_cost_[get_3d_index(i,j,k)] = c[i][j][k];
            }
        }
    }

cout<<"core dumped mfca"<<endl;
    // model.lp_.col_lower_, model.lp_.col_upper_
    model.lp_.col_lower_.resize(n*m*m);
    model.lp_.col_upper_.resize(n*m*m);
    for(int i=1;i<=n;++i) {
        for(int j=1;j<=m;++j) {
            for(int k=1;k<=m;++k) {
                model.lp_.col_lower_[get_3d_index(i,j,k)] = l[i][j][k];
                model.lp_.col_upper_[get_3d_index(i,j,k)] = u[i][j][k];
            }
        }
    }
    
    // model.lp_.row_lower_, model.lp_.row_upper_
    // model.lp_.a_matrix_.start_
    // model.lp_.a_matrix_.index_, model.lp_.a_matrix_.value_
    
    model.lp_.a_matrix_.format_ = MatrixFormat::kRowwise;
cout<<"core dumped mfca"<<endl;
    // A, L, U
    int non_zeros = 0;
    for(int i=1;i<=n;++i) { // n*m_real
        for(int j=1;j<=m_real;++j) { // reals
            model.lp_.row_lower_.push_back(0);
            model.lp_.row_upper_.push_back(0);

            non_zeros += 2*(m-1);
            model.lp_.a_matrix_.start_.push_back(non_zeros);
            for(int k=1;k<=m;++k) {
                if(k==j) continue;

                // A[i][k][j] += 1;
                model.lp_.a_matrix_.index_.push_back(get_3d_index(i,k,j));
                model.lp_.a_matrix_.value_.push_back(1);
                
                // A[i][j][k] -= 1;
                model.lp_.a_matrix_.index_.push_back(get_3d_index(i,j,k));
                model.lp_.a_matrix_.value_.push_back(-1);
            }
        }
    }
    for(int j=1;j<=m_real+m_fake;++j) { // fake,reals   m_real+m_fake
        model.lp_.row_lower_.push_back(orders[j].obligation);
        model.lp_.row_upper_.push_back(1);
        
        non_zeros += n*m;
        model.lp_.a_matrix_.start_.push_back(non_zeros);
        for(int i=1;i<=n;++i) {
            for(int k=1;k<=m;++k) {
                // A[cur][i][j][k] += 1;
                model.lp_.a_matrix_.index_.push_back(get_3d_index(i,j,k));
                model.lp_.a_matrix_.value_.push_back(1);
            }
        }
    }           
    for(int i=1;i<=n;++i) { // n*m*m
        for(int j=1;j<=m;++j) {
            for(int k=1;k<=m;++k) {
                
                model.lp_.row_lower_.push_back(0);
                model.lp_.row_upper_.push_back(orders[k].start_time + 1); // ...+1 <= ...+1 

                non_zeros += 1;
                model.lp_.a_matrix_.start_.push_back(non_zeros);

                // A[cur][i][j][k] += orders[j].finish_time + get_dist(orders[j].to_city, orders[k].from_city)/speed;
                model.lp_.a_matrix_.index_.push_back(get_3d_index(i,j,k));
                model.lp_.a_matrix_.value_.push_back(orders[j].finish_time + get_dist(orders[j].to_city, orders[k].from_city)/speed*60 + 1); // +1 so non zero
            }
        }
    }    


    return model;
}

bool checker(std::vector<std::vector<int>> &ans) {
    double revenue = 0;
    for(int i=0;i<ans.size();++i) {
        truck t = trucks[i+1];

        sort(ans[i].begin(), ans[i].end(), [](auto &a, auto &b) { 
            if(orders[a].start_time == orders[b].start_time) {
                return orders[a].order_id < orders[b].order_id;
            }
            return orders[a].start_time < orders[b].start_time;
        });
        cout<<t.truck_id<<": {";
        for(auto &el : ans[i]) {
            if(orders[el].order_id > 0) {
                cout<<orders[el].order_id<<", ";
            }
        }
        cout<<"}"<<endl;

        order prev;
        int cur_time = t.init_time;
        for(auto &el : ans[i]) {
            order ord = orders[el];

            if(ord.order_id > 0) {       
                int d = get_dist(prev.to_city, ord.from_city);
                if(d==inf) {
                    std::cerr<<"error "<< ord.order_id<<": no road between "<<prev.to_city<<" and "<<ord.start_time<<endl;
                    exit(1);
                }
                revenue -= 1.0*d/speed * free_hour_cost;
                revenue -= 1.0*d*free_km_cost;

                cur_time += 1.0*d/speed * 60;
                if(cur_time>ord.start_time) {
                    std::cerr<<"error "<< ord.order_id<<": arrived too late ("<<cur_time<<") but order starts at "<<ord.start_time <<endl;
                    exit(1);
                }
                revenue -= 1.0*(ord.start_time - cur_time) * wait_cost / 60;

                revenue -= ord.distance * duty_km_cost;
                revenue -= 1.0*(ord.finish_time - ord.start_time) * duty_hour_cost / 60;
                
                revenue += ord.revenue;
            }

            cout<<"arrive city "<<ord.from_city<<" at "<<cur_time<<" wait till "<<ord.start_time<<" and move to "<<ord.to_city<<endl;

            cur_time = ord.finish_time;
            prev = ord;

            cout<<revenue<<endl;
                
        }
    }

    cout<<std::fixed<<std::setprecision(5)<<"total revenue: "<< revenue << endl;
    return true;
}

int main() {
    parse(
        speed, 
        free_km_cost, 
        free_hour_cost,
        duty_km_cost, 
        duty_hour_cost,
        wait_cost,
        n, 
        trucks, 
        m_real,
        m_fake,
        m_stop,
        orders,
        dists,
        lst_city
    );
    m = m_real + m_fake + m_stop;

    auto model = create_model();

    auto ans = solve(model);

    assert(checker(ans)==true);

    return 0;
}