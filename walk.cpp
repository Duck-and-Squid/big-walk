#include <algorithm>
#include <cmath>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include <omp.h>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

inline double get_edge(const std::vector<double>& matrix, size_t n, size_t from,
    size_t to)
{
    return matrix[from * n + to];
}

double compute_tour_cost(const std::vector<int>& tour,
    const std::vector<double>& matrix, size_t n,
    bool closed)
{
    double cost = 0.0;
    for (size_t i = 0; i < n - 1; ++i) {
        cost += get_edge(matrix, n, tour[i], tour[i + 1]);
    }
    if (closed)
        cost += get_edge(matrix, n, tour.back(), tour.front());
    return cost;
}

void optimize_tsp_2opt(std::vector<int>& tour,
    const std::vector<double>& matrix, size_t n, bool closed,
    double& best_cost)
{
    bool improved = true;
    while (improved) {
        improved = false;
        for (size_t i = 1; i < n - 1; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                if (j - i == 1)
                    continue;
                std::reverse(tour.begin() + i, tour.begin() + j);
                double new_cost = compute_tour_cost(tour, matrix, n, closed);
                if (new_cost < best_cost) {
                    best_cost = new_cost;
                    improved = true;
                } else {
                    std::reverse(tour.begin() + i, tour.begin() + j);
                }
            }
        }
    }
}

void solve_tsp_aco(const std::vector<double>& matrix, size_t n, bool closed,
    int generations, int ants, double evaporation, double beta,
    std::vector<int>& global_best_tour,
    double& global_best_cost)
{
    global_best_cost = std::numeric_limits<double>::infinity();

// Phase 1: Multi-start NN to seed global best and initial pheromones
#pragma omp parallel
    {
        std::vector<int> local_best_tour;
        double local_best_cost = std::numeric_limits<double>::infinity();

#pragma omp for nowait
        for (int start_node = 0; start_node < (int)n; ++start_node) {
            std::vector<int> tour;
            std::vector<bool> visited(n, false);
            tour.reserve(n);
            tour.push_back(start_node);
            visited[start_node] = true;
            int current = start_node;

            for (size_t step = 1; step < n; ++step) {
                double min_dist = std::numeric_limits<double>::infinity();
                int next_node = -1;
                for (size_t cand = 0; cand < n; ++cand) {
                    if (!visited[cand]) {
                        double d = get_edge(matrix, n, current, cand);
                        if (d < min_dist) {
                            min_dist = d;
                            next_node = cand;
                        }
                    }
                }
                tour.push_back(next_node);
                visited[next_node] = true;
                current = next_node;
            }
            double cost = compute_tour_cost(tour, matrix, n, closed);
            optimize_tsp_2opt(tour, matrix, n, closed, cost);
            if (cost < local_best_cost) {
                local_best_cost = cost;
                local_best_tour = tour;
            }
        }
#pragma omp critical
        {
            if (local_best_cost < global_best_cost) {
                global_best_cost = local_best_cost;
                global_best_tour = local_best_tour;
            }
        }
    }

    if (generations <= 0)
        return;

    // Phase 2: ACO Setup
    double tau0 = 1.0 / (n * global_best_cost);
    std::vector<double> pheromone(n * n, tau0);
    std::vector<double> heuristic(n * n);
    for (size_t i = 0; i < n * n; ++i)
        heuristic[i] = std::pow(1.0 / (matrix[i] + 1e-6), beta);
    std::vector<double> weights(n * n);

    // Phase 2: ACO Iterations
    for (int gen = 0; gen < generations; ++gen) {
        for (size_t i = 0; i < n * n; ++i)
            weights[i] = pheromone[i] * heuristic[i];

        std::vector<int> gen_best_tour;
        double gen_best_cost = std::numeric_limits<double>::infinity();

#pragma omp parallel
        {
            int t_id = omp_get_thread_num();
            std::mt19937 rng(1337 ^ t_id ^ gen);
            std::uniform_real_distribution<double> dist(0.0, 1.0);

            std::vector<int> local_best_tour;
            double local_best_cost = std::numeric_limits<double>::infinity();
            std::vector<double> probs(n);

#pragma omp for nowait
            for (int a = 0; a < ants; ++a) {
                std::vector<int> tour;
                tour.reserve(n);
                std::vector<bool> visited(n, false);

                int current = rng() % n;
                tour.push_back(current);
                visited[current] = true;

                for (size_t step = 1; step < n; ++step) {
                    double sum = 0.0;
                    for (size_t i = 0; i < n; ++i) {
                        if (!visited[i]) {
                            probs[i] = weights[current * n + i];
                            sum += probs[i];
                        } else {
                            probs[i] = 0.0;
                        }
                    }

                    double threshold = dist(rng) * sum;
                    double cumulative = 0.0;
                    int next_node = -1;
                    for (size_t i = 0; i < n; ++i) {
                        if (!visited[i]) {
                            cumulative += probs[i];
                            if (cumulative >= threshold) {
                                next_node = i;
                                break;
                            }
                        }
                    }
                    if (next_node == -1) {
                        for (size_t i = 0; i < n; ++i)
                            if (!visited[i]) {
                                next_node = i;
                                break;
                            }
                    }

                    tour.push_back(next_node);
                    visited[next_node] = true;
                    current = next_node;
                }

                double cost = compute_tour_cost(tour, matrix, n, closed);
                optimize_tsp_2opt(tour, matrix, n, closed, cost);

                if (cost < local_best_cost) {
                    local_best_cost = cost;
                    local_best_tour = tour;
                }
            }

#pragma omp critical
            {
                if (local_best_cost < gen_best_cost) {
                    gen_best_cost = local_best_cost;
                    gen_best_tour = local_best_tour;
                }
            }
        }

        if (gen_best_cost < global_best_cost) {
            global_best_cost = gen_best_cost;
            global_best_tour = gen_best_tour;
        }

        // Pheromone update (Evaporate + Elitist Deposit)
        for (size_t i = 0; i < n * n; ++i) {
            pheromone[i] = std::max(pheromone[i] * (1.0 - evaporation), 1e-10);
        }

        double d_global = 1.0 / global_best_cost;
        double d_gen = 1.0 / gen_best_cost;

        auto deposit = [&](const std::vector<int>& t, double amt) {
            for (size_t i = 0; i < n - 1; ++i) {
                pheromone[t[i] * n + t[i + 1]] += amt;
                pheromone[t[i + 1] * n + t[i]] += amt;
            }
            if (closed) {
                pheromone[t.back() * n + t.front()] += amt;
                pheromone[t.front() * n + t.back()] += amt;
            }
        };

        deposit(global_best_tour, d_global);
        deposit(gen_best_tour, d_gen);
    }
}

int main(int argc, char* argv[])
{
    std::string input_file, output_file;
    std::string metric = "duration";
    bool is_closed_loop = false;
    int restarts = 1000;
    int ants = 64;
    double evaporation = 0.1;
    double beta = 3.0;

    const char* const short_opts = "i:o:m:r:a:e:b:";
    const option long_opts[] = { { "input", required_argument, nullptr, 'i' },
        { "output", required_argument, nullptr, 'o' },
        { "metric", required_argument, nullptr, 'm' },
        { "closed", no_argument, nullptr, 'p' },
        { "restarts", required_argument, nullptr, 'r' },
        { "ants", required_argument, nullptr, 'a' },
        { "evap", required_argument, nullptr, 'e' },
        { "beta", required_argument, nullptr, 'b' },
        { nullptr, 0, nullptr, 0 } };

    int opt;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) != -1) {
        switch (opt) {
        case 'i':
            input_file = optarg;
            break;
        case 'o':
            output_file = optarg;
            break;
        case 'm':
            metric = optarg;
            break;
        case 'p':
            is_closed_loop = true;
            break;
        case 'r':
            restarts = std::stoi(optarg);
            break;
        case 'a':
            ants = std::stoi(optarg);
            break;
        case 'e':
            evaporation = std::stod(optarg);
            break;
        case 'b':
            beta = std::stod(optarg);
            break;
        default:
            std::cerr
                << "Usage: " << argv[0]
                << " -i <in.json> -o <out.json> [-m duration|distance] [--closed] "
                   "[-r generations] [--ants int] [--evap float] [--beta float]\n";
            return 1;
        }
    }

    if (input_file.empty() || output_file.empty()) {
        std::cerr << "ERROR: -i and -o are required.\n";
        return 1;
    }

    std::ifstream f_in(input_file);
    if (!f_in.is_open())
        return 1;

    json data;
    f_in >> data;

    std::vector<std::string> names = data["names"].get<std::vector<std::string>>();
    std::string matrix_key = (metric == "duration") ? "durations_s" : "distances_m";

    auto json_matrix = data[matrix_key].get<std::vector<std::vector<double>>>();
    size_t n = names.size();
    std::vector<double> flat_matrix(n * n);

    for (size_t r = 0; r < n; ++r) {
        for (size_t c = 0; c < n; ++c) {
            flat_matrix[r * n + c] = json_matrix[r][c];
        }
    }

    std::cerr << "INFO: OpenMP Multi-threading Active (" << omp_get_max_threads()
              << " threads).\n";

    std::vector<int> optimal_indices;
    double final_cost;
    solve_tsp_aco(flat_matrix, n, is_closed_loop, restarts, ants, evaporation,
        beta, optimal_indices, final_cost);

    std::vector<std::string> optimal_names;
    for (int idx : optimal_indices)
        optimal_names.push_back(names[idx]);

    std::string unit = (metric == "distance") ? "meters" : "seconds";
    std::cerr << "INFO: Optimization complete! Optimal cost: " << final_cost
              << " " << unit << "\n";

    json output_data = { { "optimization_metric", metric },
        { "route_type", is_closed_loop ? "round-trip" : "one-way" },
        { "total_cost", final_cost },
        { "cost_unit", unit },
        { "optimal_order_indices", optimal_indices },
        { "optimal_order_names", optimal_names } };

    std::ofstream f_out(output_file);
    f_out << output_data.dump(2);

    return 0;
}