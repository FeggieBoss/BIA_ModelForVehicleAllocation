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
static int inf = 1e6;
static int inf_d = 4e5+5e4;


int get_3d_index(int i, int j, int k) {
    return i+(j+(k-1)*m - 1)*n - 1;
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

HighsModel create_model(data_t &data) {
    n = data.n;
    m_real = data.m_real;
    m_fake = data.n;
    m_stop = 1;
    m = m_real + m_fake + m_stop;
    lst_city = data.lst_city;
    min_time = data.min_time;
    params = data.params;
    inf = data.inf;
    inf_d = data.inf_d;


    int any_city = data.trucks[1].init_city;
    for(int i=1;i<=n;++i) {
        data.orders.push_back(order{});
        data.orders.back().obligation = 1;
        data.orders.back().start_time = data.orders.back().finish_time = data.trucks[i].init_time;
        data.orders.back().to_city = data.trucks[i].init_city;
        data.trucks[i].init_city = data.orders.back().from_city = lst_city+i; // relocating
        data.orders.back().type = data.trucks[i].type;
    }
    data.orders.push_back(order{});
    data.orders.back().start_time = data.orders.back().finish_time = inf;
    data.orders.back().from_city = any_city;
    data.orders.back().to_city = lst_city+n+1;
    
    cout<<endl;
    cout<<std::fixed<<std::setprecision(5)<<params.speed<<" "<<params.duty_km_cost<<" "<<params.duty_hour_cost<<" "<<params.free_km_cost<<" "<<params.free_hour_cost<<" "<<params.wait_cost<<endl;
    
    cout<<endl;
    int ost_print=10;
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

    cout<<(n+1)*(m+1)*(m+1)<<endl;
    
    // c^Tx + d subject to L <= Ax <= U; l <= x <= u
    const int nmm = n*m_real+m_real+m_fake+n*m*m;
    HighsModel model;
    model.lp_.num_col_ = n*m*m;
    model.lp_.num_row_ = nmm;

    model.lp_.sense_ = ObjSense::kMaximize;

    model.lp_.offset_ = 0;

    // c 
    // model.lp_.col_cost_
    model.lp_.col_cost_.resize(n*m*m);
    for(int i=1;i<=n;++i) {
        for(int j=1;j<=m;++j) {
            for(int k=1;k<=m;++k) {
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
                model.lp_.col_cost_[get_3d_index(i,j,k)] = c;
            }
        }
    }

    // l,u
    // model.lp_.col_lower_, model.lp_.col_upper_
    model.lp_.col_lower_.resize(n*m*m);
    model.lp_.col_upper_.resize(n*m*m);
    for(int i=1;i<=n;++i) {
        for(int j=1;j<=m;++j) {
            for(int k=1;k<=m;++k) {
                int l = 0;
                int u = (data.trucks[i].type == data.orders[j].type) && (j != k);
                if(j>=m_real+1 && j<=m_real+m_fake) { // fake
                    u &= (m_real+i) == j; // fake was made for him
                }
                if(k>=m_real+1 && k<=m_real+m_fake) { // fake
                    u = 0; // fake is always start
                }

                model.lp_.col_lower_[get_3d_index(i,j,k)] = l;
                model.lp_.col_upper_[get_3d_index(i,j,k)] = u;
            }
        }
    }

    model.lp_.a_matrix_.format_ = MatrixFormat::kRowwise;
    
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
        model.lp_.row_lower_.push_back(data.orders[j].obligation);
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
                model.lp_.row_upper_.push_back(data.orders[k].start_time + 1); // ...+1 <= ...+1 

                non_zeros += 1;
                model.lp_.a_matrix_.start_.push_back(non_zeros);

                // A[cur][i][j][k] += data.orders[j].finish_time + get_dists(data.dists, data.orders[j].to_city, data.orders[k].from_city)/params.speed;
                model.lp_.a_matrix_.index_.push_back(get_3d_index(i,j,k));
                model.lp_.a_matrix_.value_.push_back(data.orders[j].finish_time + get_dists(data.dists, data.orders[j].to_city, data.orders[k].from_city)/params.speed*60 + 1); // +1 so non zero
            }
        }
    }    

    cout<<non_zeros<<endl;

    return model;
}

bool checker(std::vector<std::vector<int>> &ans, std::vector<truck> &trucks, std::vector<order> &orders, std::map<std::pair<int,int>, double> &dists) {
    std::map<int, bool> oblig_orders;

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
            if(ord.obligation && ord.order_id) oblig_orders[ord.order_id] = 1;

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
            else cout<<"obligation order "<<ord.order_id<<" sucessfully done"<<endl;
        }
    }

    cout<<std::fixed<<std::setprecision(5)<<"total revenue: "<< revenue << endl;

    return true;
}