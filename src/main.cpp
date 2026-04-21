#include "as_graph.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0]
                  << " <caida_file> <anns_csv> <rov_asns_file> <output_csv>" << std::endl;
        std::cerr << "Example: " << argv[0]
                  << " bench/prefix/CAIDAASGraphCollector_2025.10.15.txt"
                  << " bench/prefix/anns.csv bench/prefix/rov_asns.csv output/ribs.csv"
                  << std::endl;
        return 1;
    }

    const std::string caida_file   = argv[1];
    const std::string anns_file    = argv[2];
    const std::string rov_asns_file = argv[3];
    const std::string output_file  = argv[4];

    // Step 1: Build the AS graph from CAIDA topology
    ASGraph graph;
    if (!graph.buildFromCAIDAFile(caida_file)) {
        std::cerr << "Failed to build AS graph from: " << caida_file << std::endl;
        return 1;
    }

    // Step 2: Read ROV-deploying ASNs (one per line)
    std::unordered_set<uint32_t> rov_asns;
    {
        std::ifstream rov_file(rov_asns_file);
        if (!rov_file.is_open()) {
            std::cerr << "Warning: could not open ROV ASNs file: " << rov_asns_file << std::endl;
        } else {
            std::string line;
            while (std::getline(rov_file, line)) {
                if (line.empty()) continue;
                try {
                    rov_asns.insert(std::stoul(line));
                } catch (...) {}
            }
            std::cout << "Loaded " << rov_asns.size() << " ROV ASNs" << std::endl;
        }
    }

    // Step 3: Assign BGP or ROV policies to every AS
    graph.initializePolicies(rov_asns);

    // Step 4: Flatten the graph (compute propagation ranks)
    graph.flattenGraph();

    // Step 5: Seed announcements from CSV
    if (!graph.seedFromCSV(anns_file)) {
        std::cerr << "Failed to seed announcements from: " << anns_file << std::endl;
        return 1;
    }

    // Step 6: Propagate announcements (UP -> ACROSS -> DOWN)
    graph.propagateAnnouncements();

    // Step 7: Write output CSV
    if (!graph.outputToCSV(output_file)) {
        std::cerr << "Failed to write output to: " << output_file << std::endl;
        return 1;
    }

    return 0;
}
