#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <limits>
#include <getopt.h>

// Include the downloaded header-only JSON library
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// --- Helper Functions ---

double compute_tour_cost(const std::vector<int>& tour, const std::vector<std::vector<double>>& matrix, bool closed) {
    double cost = 0.0;
    for (size_t i = 0; i < tour.size() - 1; ++i) {
        cost += matrix[tour[i]][tour[i + 1]];
    }
    if (closed) {
        cost += matrix[tour.back()][tour.front()];
    }
    return cost;
}

void optimize_tsp_2opt(std::vector<int>& tour, const std::vector<std::vector<double>>& matrix, bool closed, double& best_cost) {
    size_t n = tour.size();
    bool improved = true;

    while (improved) {
        improved = false;
        for (size_t i = 1; i < n - 1; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                if (j - i == 1) continue; // Reversing adjacent nodes does nothing

                // In-place reversal for extreme speed
                std::reverse(tour.begin() + i, tour.begin() + j);
                double new_cost = compute_tour_cost(tour, matrix, closed);

                if (new_cost < best_cost) {
                    best_cost = new_cost;
                    improved = true;
                } else {
                    // Revert the reversal if it didn't improve the cost
                    std::reverse(tour.begin() + i, tour.begin() + j);
                }
            }
        }
    }
}

void solve_tsp_intensive(const std::vector<std::vector<double>>& matrix, bool closed, int restarts, std::vector<int>& global_best_tour, double& global_best_cost) {
    size_t n = matrix.size();
    global_best_cost = std::numeric_limits<double>::infinity();

    // Phase 1: Multi-start Nearest Neighbor + 2-opt
    for (size_t start_node = 0; start_node < n; ++start_node) {
        std::vector<int> tour;
        std::vector<bool> visited(n, false);
        
        tour.push_back(start_node);
        visited[start_node] = true;
        int current_node = start_node;

        for (size_t step = 1; step < n; ++step) {
            double min_dist = std::numeric_limits<double>::infinity();
            int next_node = -1;

            for (size_t candidate = 0; candidate < n; ++candidate) {
                if (!visited[candidate] && matrix[current_node][candidate] < min_dist) {
                    min_dist = matrix[current_node][candidate];
                    next_node = candidate;
                }
            }
            tour.push_back(next_node);
            visited[next_node] = true;
            current_node = next_node;
        }

        double cost = compute_tour_cost(tour, matrix, closed);
        optimize_tsp_2opt(tour, matrix, closed, cost);

        if (cost < global_best_cost) {
            global_best_cost = cost;
            global_best_tour = tour;
        }
    }

    // Phase 2: Iterated Local Search (Random Restarts)
    if (restarts > 0) {
        std::vector<int> base_tour(n);
        for (size_t i = 0; i < n; ++i) base_tour[i] = i;

        std::random_device rd;
        std::mt19937 g(rd());

        for (int r = 0; r < restarts; ++r) {
            std::shuffle(base_tour.begin(), base_tour.end(), g);
            double cost = compute_tour_cost(base_tour, matrix, closed);
            optimize_tsp_2opt(base_tour, matrix, closed, cost);

            if (cost < global_best_cost) {
                global_best_cost = cost;
                global_best_tour = base_tour;
            }
        }
    }
}

// --- Main Program ---

int main(int argc, char* argv[]) {
    std::string input_file;
    std::string output_file;
    std::string metric = "duration";
    bool is_closed_loop = true;
    int restarts = 250;

    // Define command-line options
    const char* const short_opts = "i:o:m:r:";
    const option long_opts[] = {
        {"input", required_argument, nullptr, 'i'},
        {"output", required_argument, nullptr, 'o'},
        {"metric", required_argument, nullptr, 'm'},
        {"open", no_argument, nullptr, 'p'}, // 'p' for open path flag internally
        {"restarts", required_argument, nullptr, 'r'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'i': input_file = optarg; break;
            case 'o': output_file = optarg; break;
            case 'm': metric = optarg; break;
            case 'p': is_closed_loop = false; break;
            case 'r': restarts = std::stoi(optarg); break;
            default:
                std::cerr << "Usage: " << argv[0] << " -i <input.json> -o <output.json> [-m duration|distance] [--open] [-r restarts]\n";
                return 1;
        }
    }

    if (input_file.empty() || output_file.empty()) {
        std::cerr << "ERROR: Both -i/--input and -o/--output are required.\n";
        return 1;
    }

    std::cerr << "INFO: Loading matrix data from " << input_file << "...\n";

    // Parse JSON
    std::ifstream f_in(input_file);
    if (!f_in.is_open()) {
        std::cerr << "ERROR: Input matrix file not found: " << input_file << "\n";
        return 1;
    }

    json data;
    try {
        f_in >> data;
    } catch (const json::parse_error& e) {
        std::cerr << "ERROR: JSON parsing failed: " << e.what() << "\n";
        return 1;
    }

    std::vector<std::string> names = data["names"].get<std::vector<std::string>>();
    std::string matrix_key = (metric == "duration") ? "durations_s" : "distances_m";

    if (!data.contains(matrix_key)) {
        std::cerr << "ERROR: Expected matrix key '" << matrix_key << "' not found in input JSON.\n";
        return 1;
    }

    std::vector<std::vector<double>> matrix = data[matrix_key].get<std::vector<std::vector<double>>>();
    size_t num_locations = names.size();

    if (num_locations < 2) {
        std::cerr << "ERROR: At least 2 locations are required to compute an optimal ordering.\n";
        return 1;
    }

    std::cerr << "INFO: Optimizing order for " << num_locations << " locations based on " << metric << "...\n";
    std::cerr << "INFO: Route Type: " << (is_closed_loop ? "Round-trip Loop" : "One-way Linear Path") << "\n";
    std::cerr << "INFO: Intensive search enabled: " << restarts << " random restarts.\n";

    // Run solver
    std::vector<int> optimal_indices;
    double final_cost;
    solve_tsp_intensive(matrix, is_closed_loop, restarts, optimal_indices, final_cost);

    // Map indices to names
    std::vector<std::string> optimal_names;
    for (int idx : optimal_indices) {
        optimal_names.push_back(names[idx]);
    }

    std::string unit = (metric == "distance") ? "meters" : "seconds";
    std::cerr << "INFO: Optimization complete! Optimal cost: " << final_cost << " " << unit << "\n";

    // Construct Output JSON
    json output_data = {
        {"optimization_metric", metric},
        {"route_type", is_closed_loop ? "round-trip" : "one-way"},
        {"total_cost", final_cost},
        {"cost_unit", unit},
        {"optimal_order_indices", optimal_indices},
        {"optimal_order_names", optimal_names}
    };

    // Write to file
    std::ofstream f_out(output_file);
    if (!f_out.is_open()) {
        std::cerr << "ERROR: Failed to write output to " << output_file << "\n";
        return 1;
    }
    
    f_out << output_data.dump(2); // Indent with 2 spaces
    std::cerr << "INFO: Optimal ordering saved to " << output_file << "\n";

    return 0;
}
