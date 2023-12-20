#include "model.h"

using std::cout;
using std::endl;

static int n = 0;
static int m_real = 0;
static int m_fake = 0;
static int m_stop = 0;
static int m = 0;
static int lst_city = 0;
static int min_time = INT_MAX;
static params_t params;
static int inf_d = 0;


int get_1d_index(int i, int j, int k) {
    return k+(j+(i-1)*m - 1)*m - 1;
} 

void get_3d_index(int x, int &i, int &j, int &k) {
    k = (x%m)+1;   
    j = (x/m)%m+1;
    i = (x/m)/m+1;
}

double get_dists(std::map<std::pair<int,int>, double> &dists, int from, int to) {
    if(from == to) {
        return 0;
    }

    auto it = dists.find({from,to});
    if(it!=dists.end()) {
        return it->second;
    } else {
        return inf_d;
    }
}

bool is_real(int i) {
    return (1 <= i && i <= m_real);
}
bool is_fake(int i) {
    return (m_real+1 <= i && i <= m_real+m_fake);
}

bool is_stop(int i) {
    return (m_real+m_fake+1 <= i && i <= m_real+m_fake+m_stop);
}

bool is_zero(int i, std::unordered_map<int, int> &non_zeros) {
    auto it = non_zeros.find(i);
    if(it==non_zeros.end()) {
        return true;
    }
    return false;
}

std::vector<int> solve(HighsModel &model) {
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
    
    std::vector<int> ans;
    for (int col=0; col < lp.num_col_; col++) {
        if (info.primal_solution_status && solution.col_value[col]) {
            ans.push_back(col);
        }
    }

    return ans;
}

model_t create_model(data_t &data) {
    n = data.n;
    m_real = data.m_real;
    m_fake = data.n;
    m_stop = 1;
    m = m_real + m_fake + m_stop;
    lst_city = data.lst_city;
    min_time = data.min_time;
    params = data.params;
    inf_d = data.inf_d;

    for(int i=1;i<=n;++i) {
        data.orders.push_back(order{});
        data.orders.back().obligation = 1;
        data.orders.back().start_time = data.orders.back().finish_time = data.trucks[i].init_time;
        data.orders.back().to_city = data.trucks[i].init_city;
        data.trucks[i].init_city = data.orders.back().from_city = lst_city+i; // relocating
        data.orders.back().type = data.trucks[i].type;
    }
    data.orders.push_back(order{});
    data.orders.back().from_city = lst_city+n+1;
    data.orders.back().to_city = lst_city+n+1;
    
    cout<<endl;
    cout<<std::fixed<<std::setprecision(5)<<params.speed<<" "<<params.duty_km_cost<<" "<<params.duty_hour_cost<<" "<<params.free_km_cost<<" "<<params.free_hour_cost<<" "<<params.wait_cost<<endl;
    
    cout<<endl;
    int ost_print=20;
    for(auto&el : data.trucks) {
        cout<<el.truck_id<<" "<<el.type<<" "<<el.init_time<<" "<<el.init_city<<endl; 

        --ost_print;
        if(!ost_print) break;
    }
    
    cout<<endl;
    ost_print=10;
    for(auto&el : data.orders) {
        cout<<el.order_id<<" "<<el.obligation<<" "<<el.start_time<<" "<<el.finish_time<<" "<<el.from_city<<" "<<el.to_city<<" "<<el.type<<" "<<el.distance<<" "<<el.revenue<<endl;
    
        --ost_print;
        if(!ost_print) break;
    }

    
    // c^Tx + d subject to L <= Ax <= U; l <= x <= u
    int num_row = 0;
    HighsModel model;
    model.lp_.sense_ = ObjSense::kMaximize;
    model.lp_.offset_ = 0;

    std::unordered_map<int, int> non_zeros;
    std::vector<variable_t> variables;

    // l,u
    // model.lp_.col_lower_, model.lp_.col_upper_
    #ifdef NO_LONG_EDGES
    int no_long_edges = 0;
    #endif
    #ifdef COMPLETE_GRAPH
    int complete_graph=0;
    #endif
    for(int i=1;i<=n;++i) {
        for(int j=1;j<=m;++j) {
            for(int k=1;k<=m;++k) {
                int l = 0;
                int u = 1; 
                if(j==k) { 
                    u = 0;
                } else if(data.trucks[i].type != data.orders[j].type) {
                    u = 0; 
                } else if(is_fake(j) && (m_real+i) != j) { // fake && fake wasnt made for him
                    u = 0;
                } else {
                    double t1,t2;                    
                    if (!is_stop(k)) { 
                        t1 = data.orders[j].finish_time + 1.0 * get_dists(data.dists, data.orders[j].to_city, data.orders[k].from_city)/params.speed*60;
                        t2 = data.orders[k].start_time;
                        u &= (t1<=t2); // possible to arrive to k after j
                    }

                    #ifdef COMPLETE_GRAPH
                    if (u && !is_fake(j)) {
                        t1 = data.orders[m_real+i].finish_time + 1.0 * get_dists(data.dists, data.orders[m_real+i].to_city, data.orders[j].from_city)/params.speed*60;
                        t2 = data.orders[j].start_time;
                        u &= (t1<=t2); // possible to arrive to j straight from initial place (true for any order - not necessary start)
                        complete_graph += (t1>t2);
                    }
                    #endif
                }

                #ifdef NO_LONG_EDGES
                if(u!=0) {
                    double d = get_dists(data.dists, data.orders[j].to_city,data.orders[k].from_city);
                    if((is_fake(j)&&is_real(k))||(is_real(j)&&is_stop(k))||(is_fake(j)&&is_stop(k))) { // fake->real / real->stop / fake->stop
                        d = 0;
                    }

                    if(d > NO_LONG_EDGES) {
                        ++no_long_edges;
                        u = 0;
                    }
                }
                #endif
                
                if(u!=0) {
                    non_zeros.emplace(get_1d_index(i,j,k), model.lp_.col_lower_.size());
                    variables.push_back({(int)model.lp_.col_lower_.size(), i, j, k});

                    model.lp_.col_lower_.push_back(l);
                    model.lp_.col_upper_.push_back(u);
                }
            }
        }
    }
    #ifdef NO_LONG_EDGES
    cout << "[flag]no long edges: " << no_long_edges << " (out of " << n*m*m << ")" << endl;
    #endif
    #ifdef COMPLETE_GRAPH
    cout << "[flag]complete graph: " << complete_graph << " (out of " << n*m*m << ")" << endl;
    #endif
    cout << "non zero variables: " << non_zeros.size() << " (out of " << n*m*m << ")" << endl;

    cout << "variables: " << non_zeros.size() << endl;
    model.lp_.num_col_ = non_zeros.size();

    // c 
    // model.lp_.col_cost_
    for(int i=1;i<=n;++i) {
        for(int j=1;j<=m;++j) {
            for(int k=1;k<=m;++k) {
                if(is_zero(get_1d_index(i,j,k), non_zeros)) continue;

                double c = 0;

                // revenue sum
                c += data.orders[j].revenue; 

                // duty hours
                c -= 1.0 * (data.orders[j].finish_time - data.orders[j].start_time)*params.duty_hour_cost / 60;

                // duty km
                c -= data.orders[j].distance*params.duty_km_cost;

                if(k<=m_real+m_fake) {
                    // free hours
                    c -= (1.0 * get_dists(data.dists, data.orders[j].to_city,data.orders[k].from_city)/params.speed)*params.free_hour_cost;

                    // free km
                    c -= get_dists(data.dists, data.orders[j].to_city,data.orders[k].from_city)*params.free_km_cost;

                    // waiting
                    c -= 1.0 * ((data.orders[k].start_time - data.orders[j].finish_time) - ((double)get_dists(data.dists, data.orders[j].to_city,data.orders[k].from_city)/params.speed * 60))*params.wait_cost / 60;
                }
                model.lp_.col_cost_.push_back(c);
            }
        }
    }

    model.lp_.a_matrix_.format_ = MatrixFormat::kRowwise;
    
    // A
    // L, U
    // model.lp_.a_matrix_.start_, model.lp_.a_matrix_.index_, model.lp_.a_matrix_.value_
    // model.lp_.row_lower_, model.lp_.row_upper_
    int a_matrix_non_zeros = 0;
    for(int i=1;i<=n;++i) {
        for(int j=1;j<=m_real;++j) { // reals
            std::vector<std::pair<int,int>> non_zeros_cur;
            for(int k=1;k<=m;++k) {
                int idx1 = get_1d_index(i,j,k);
                int idx2 = get_1d_index(i,k,j);
                if(k==j) continue;
                if(!is_zero(idx1,non_zeros))
                    non_zeros_cur.emplace_back(non_zeros[idx1], 1); // A[i][k][j] += 1;
                if(!is_zero(idx2,non_zeros))
                    non_zeros_cur.emplace_back(non_zeros[idx2],-1); // A[i][k][j] -= 1;
            }
            if(non_zeros_cur.empty()) continue;

            ++num_row;

            model.lp_.row_lower_.push_back(0);
            model.lp_.row_upper_.push_back(0);

            a_matrix_non_zeros += non_zeros_cur.size();
            model.lp_.a_matrix_.start_.push_back(a_matrix_non_zeros);
            
            sort(non_zeros_cur.begin(),non_zeros_cur.end());
            for(auto &el : non_zeros_cur) {
                model.lp_.a_matrix_.index_.push_back(el.first);
                model.lp_.a_matrix_.value_.push_back(el.second);
            }
        }
    }
    for(int j=1;j<=m_real+m_fake;++j) { // fake,reals          
        int non_zeros_cur = 0;
        for(int i=1;i<=n;++i) {
            for(int k=1;k<=m;++k) {
                int idx = get_1d_index(i,j,k);
                if(is_zero(idx,non_zeros)) continue;

                // A[cur][i][j][k] += 1;
                model.lp_.a_matrix_.index_.push_back(non_zeros[idx]);
                model.lp_.a_matrix_.value_.push_back(1);

                ++non_zeros_cur;
            }
        }
        if(!non_zeros_cur) {
            if(data.orders[j].obligation) {
                std::cerr<<"There is no single truck with type: "<<data.orders[j].type<<" to make obligation order "<<data.orders[j].order_id<<endl;
                //exit(1);
            }
            continue;
        }

        ++num_row;

        model.lp_.row_lower_.push_back(data.orders[j].obligation);
        model.lp_.row_upper_.push_back(1);

        a_matrix_non_zeros += non_zeros_cur;
        model.lp_.a_matrix_.start_.push_back(a_matrix_non_zeros);
    }           

    model.lp_.num_row_ = num_row;
    cout<<"num row: "<<num_row<<endl;
    cout<<"non zeros: "<<a_matrix_non_zeros<<endl;
    
    return {model, variables};
}

bool checker(std::vector<std::vector<int>> &ans, std::vector<truck> &trucks, std::vector<order> &orders, std::map<std::pair<int,int>, double> &dists) {
    std::map<int, int> oblig_orders;

    double revenue = 0;
    for(int i=0;i<ans.size();++i) {
        truck t = trucks[i+1];

        sort(ans[i].begin(), ans[i].end(), [&orders](auto &a, auto &b) { 
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
            if(ord.obligation && ord.order_id) oblig_orders[ord.order_id] = t.truck_id;

            if(ord.order_id > 0) {       
                double d = get_dists(dists, prev.to_city, ord.from_city);
                if(d>=inf_d-1) {
                    std::cerr<<"error "<< ord.order_id<<": no road between "<<prev.to_city<<" and "<<ord.start_time<<endl;
                    exit(1);
                }
                revenue -= 1.0*d/params.speed * params.free_hour_cost;
                revenue -= 1.0*d*params.free_km_cost;


                cur_time += 1.0*d/params.speed * 60;
                if(cur_time>ord.start_time) {
                    std::cerr<<"error "<< ord.order_id<<": arrived too late ("<<cur_time<<") but order starts at "<<ord.start_time <<endl;
                    exit(1);
                }
                revenue -= 1.0*(ord.start_time - cur_time) * params.wait_cost / 60;

                revenue -= ord.distance * params.duty_km_cost;
                revenue -= 1.0*(ord.finish_time - ord.start_time) * params.duty_hour_cost / 60;
                
                revenue += ord.revenue;
            }

            cout<<"arrive city "<<ord.from_city<<" at "<<cur_time<<" wait till "<<ord.start_time<<" and move to "<<ord.to_city<<endl;

            cur_time = ord.finish_time;
            prev = ord;

            cout<<revenue<<endl;
                
        }
    }

    for(auto &ord : orders) {
        if(ord.obligation && ord.order_id) {
            auto it = oblig_orders.find(ord.order_id);
            if(it==oblig_orders.end()) {
                cout<<"error: order "<<ord.order_id<<" wasnt scheduled but its obligation one"<<endl;
            }
            else cout<<"obligation order "<<ord.order_id<<" sucessfully done by "<<it->second<<endl;
        }
    }

    cout<<std::fixed<<std::setprecision(5)<<"total revenue: "<< revenue << endl;

    return true;
}