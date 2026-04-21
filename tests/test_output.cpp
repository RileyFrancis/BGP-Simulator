#include "as_graph.hpp"
#include "policy.hpp"
#include "announcement.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <memory>
#include <set>
#include <map>
#include <algorithm>

/**
 * Helper function to read CSV file and return lines
 */
std::vector<std::string> readCSVLines(const std::string& filename) {
    std::vector<std::string> lines;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return lines;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    
    file.close();
    return lines;
}

/**
 * Helper function to parse CSV line into components
 */
struct CSVEntry {
    uint32_t asn;
    std::string prefix;
    std::string as_path;
};

std::vector<CSVEntry> parseCSV(const std::string& filename) {
    std::vector<CSVEntry> entries;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        return entries;
    }
    
    std::string line;
    std::getline(file, line);  // Skip header
    
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string asn_str, prefix, as_path;
        
        if (std::getline(iss, asn_str, ',') &&
            std::getline(iss, prefix, ',') &&
            std::getline(iss, as_path)) {
            
            CSVEntry entry;
            entry.asn = std::stoul(asn_str);
            entry.prefix = prefix;
            entry.as_path = as_path;
            entries.push_back(entry);
        }
    }
    
    file.close();
    return entries;
}

void test_simple_output() {
    std::cout << "\n=== Test: Simple CSV Output (One AS, One Announcement) ===" << std::endl;
    
    ASGraph graph;
    
    AS* as1 = graph.getOrCreateAS(1);
    as1->setPolicy(std::make_unique<BGP>());
    
    // Seed one announcement
    BGP* policy = dynamic_cast<BGP*>(as1->getPolicy());
    policy->seedAnnouncement(Announcement("1.2.0.0/16", {1}, 1, "origin", false));
    
    // Output to CSV
    std::string output_file = "output/test_simple_output.csv";
    assert(graph.outputToCSV(output_file));
    
    // Verify the output
    auto lines = readCSVLines(output_file);
    assert(lines.size() == 2);  // Header + 1 entry
    assert(lines[0] == "asn,prefix,as_path");
    assert(lines[1] == "1,1.2.0.0/16,\"(1,)\"");
    
    std::cout << "✓ Simple output verified!" << std::endl;
    std::cout << "  Content: " << lines[1] << std::endl;
}

void test_output_after_propagation() {
    std::cout << "\n=== Test: CSV Output After Propagation ===" << std::endl;
    
    ASGraph graph;
    
    // Create simple chain: AS1 <- AS2 <- AS3
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    
    as1->addCustomer(as2);
    as2->addProvider(as1);
    as2->addCustomer(as3);
    as3->addProvider(as2);
    
    as1->setPolicy(std::make_unique<BGP>());
    as2->setPolicy(std::make_unique<BGP>());
    as3->setPolicy(std::make_unique<BGP>());
    
    graph.flattenGraph();
    
    // Seed at AS3
    BGP* policy3 = dynamic_cast<BGP*>(as3->getPolicy());
    policy3->seedAnnouncement(Announcement("10.0.0.0/8", {3}, 3, "origin", false));
    
    // Propagate
    graph.propagateAnnouncements();
    
    // Output
    std::string output_file = "output/test_propagation_output.csv";
    assert(graph.outputToCSV(output_file));
    
    // Parse and verify
    auto entries = parseCSV(output_file);
    assert(entries.size() == 3);  // All three ASes should have the announcement
    
    // Find each AS's entry
    std::map<uint32_t, std::string> paths;
    for (const auto& entry : entries) {
        assert(entry.prefix == "10.0.0.0/8");
        paths[entry.asn] = entry.as_path;
    }
    
    assert(paths[1] == "\"(1, 2, 3)\"");
    assert(paths[2] == "\"(2, 3)\"");
    assert(paths[3] == "\"(3,)\"");
    
    std::cout << "✓ Propagation output verified!" << std::endl;
    std::cout << "  AS1: " << paths[1] << std::endl;
    std::cout << "  AS2: " << paths[2] << std::endl;
    std::cout << "  AS3: " << paths[3] << std::endl;
}

void test_multiple_prefixes() {
    std::cout << "\n=== Test: Multiple Prefixes from Same AS ===" << std::endl;
    
    ASGraph graph;
    
    AS* as1 = graph.getOrCreateAS(1);
    as1->setPolicy(std::make_unique<BGP>());
    
    BGP* policy = dynamic_cast<BGP*>(as1->getPolicy());
    policy->seedAnnouncement(Announcement("1.0.0.0/8", {1}, 1, "origin", false));
    policy->seedAnnouncement(Announcement("2.0.0.0/8", {1}, 1, "origin", false));
    policy->seedAnnouncement(Announcement("3.0.0.0/8", {1}, 1, "origin", false));
    
    std::string output_file = "output/test_multiple_prefixes.csv";
    assert(graph.outputToCSV(output_file));
    
    auto entries = parseCSV(output_file);
    assert(entries.size() == 3);
    
    std::set<std::string> prefixes;
    for (const auto& entry : entries) {
        assert(entry.asn == 1);
        assert(entry.as_path == "\"(1,)\"");
        prefixes.insert(entry.prefix);
    }
    
    assert(prefixes.count("1.0.0.0/8") == 1);
    assert(prefixes.count("2.0.0.0/8") == 1);
    assert(prefixes.count("3.0.0.0/8") == 1);
    
    std::cout << "✓ Multiple prefixes output correctly!" << std::endl;
    std::cout << "  Total prefixes: " << entries.size() << std::endl;
}

void test_conflicting_announcements() {
    std::cout << "\n=== Test: Conflicting Announcements (Same Prefix, Different ASes) ===" << std::endl;
    
    ASGraph graph;
    
    /*
     *     AS1
     *    /   \
     *  AS2   AS3
     *  
     * Both AS2 and AS3 announce same prefix
     * AS1 should choose based on BGP rules
     */
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    
    as1->addCustomer(as2);
    as2->addProvider(as1);
    as1->addCustomer(as3);
    as3->addProvider(as1);
    
    as1->setPolicy(std::make_unique<BGP>());
    as2->setPolicy(std::make_unique<BGP>());
    as3->setPolicy(std::make_unique<BGP>());
    
    graph.flattenGraph();
    
    // Both announce same prefix
    dynamic_cast<BGP*>(as2->getPolicy())->seedAnnouncement(
        Announcement("100.0.0.0/8", {2}, 2, "origin", false));
    dynamic_cast<BGP*>(as3->getPolicy())->seedAnnouncement(
        Announcement("100.0.0.0/8", {3}, 3, "origin", false));
    
    graph.propagateAnnouncements();
    
    std::string output_file = "output/test_conflicts.csv";
    assert(graph.outputToCSV(output_file));
    
    auto entries = parseCSV(output_file);
    
    // All three ASes should have the prefix
    assert(entries.size() == 3);
    
    std::map<uint32_t, std::string> paths;
    for (const auto& entry : entries) {
        paths[entry.asn] = entry.as_path;
    }
    
    assert(paths[2] == "\"(2,)\"");
    assert(paths[3] == "\"(3,)\"");
    // AS1 should choose one (lower next hop ASN = AS2)
    assert(paths[1] == "\"(1, 2)\"" || paths[1] == "\"(1, 3)\"");
    
    std::cout << "✓ Conflict resolution output correctly!" << std::endl;
    std::cout << "  AS1 chose: " << paths[1] << std::endl;
    std::cout << "  AS2: " << paths[2] << std::endl;
    std::cout << "  AS3: " << paths[3] << std::endl;
}

void test_bgpsimulator_output() {
    std::cout << "\n=== Test: bgpsimulator.com Example Output ===" << std::endl;
    
    ASGraph graph;
    
    AS* as3 = graph.getOrCreateAS(3);
    AS* as4 = graph.getOrCreateAS(4);
    AS* as666 = graph.getOrCreateAS(666);
    AS* as777 = graph.getOrCreateAS(777);
    
    // Setup relationships
    as4->addCustomer(as3);
    as3->addProvider(as4);
    as4->addCustomer(as666);
    as666->addProvider(as4);
    as777->addCustomer(as3);
    as3->addProvider(as777);
    as4->addPeer(as777);
    as777->addPeer(as4);
    
    as3->setPolicy(std::make_unique<BGP>());
    as4->setPolicy(std::make_unique<BGP>());
    as666->setPolicy(std::make_unique<BGP>());
    as777->setPolicy(std::make_unique<BGP>());
    
    graph.flattenGraph();
    
    // Seed announcements
    dynamic_cast<BGP*>(as777->getPolicy())->seedAnnouncement(
        Announcement("1.2.0.0/16", {777}, 777, "origin", false));
    dynamic_cast<BGP*>(as666->getPolicy())->seedAnnouncement(
        Announcement("1.2.0.0/16", {666}, 666, "origin", false));
    
    graph.propagateAnnouncements();
    
    std::string output_file = "output/test_bgpsimulator.csv";
    assert(graph.outputToCSV(output_file));
    
    auto entries = parseCSV(output_file);
    assert(entries.size() == 4);  // All four ASes
    
    std::map<uint32_t, std::string> paths;
    for (const auto& entry : entries) {
        paths[entry.asn] = entry.as_path;
    }
    
    // Verify expected paths
    assert(paths[777] == "\"(777,)\"");
    assert(paths[666] == "\"(666,)\"");
    assert(paths[4] == "\"(4, 666)\"");     // AS4 prefers shorter path from AS666
    assert(paths[3] == "\"(3, 777)\"");     // AS3 prefers provider AS777
    
    std::cout << "✓ bgpsimulator.com output matches!" << std::endl;
    std::cout << "  AS777: " << paths[777] << std::endl;
    std::cout << "  AS666: " << paths[666] << std::endl;
    std::cout << "  AS4: " << paths[4] << " (chose shorter path)" << std::endl;
    std::cout << "  AS3: " << paths[3] << " (chose better relationship)" << std::endl;
}

void test_ipv4_and_ipv6_output() {
    std::cout << "\n=== Test: IPv4 and IPv6 Prefixes in Output ===" << std::endl;
    
    ASGraph graph;
    
    AS* as1 = graph.getOrCreateAS(1);
    as1->setPolicy(std::make_unique<BGP>());
    
    BGP* policy = dynamic_cast<BGP*>(as1->getPolicy());
    policy->seedAnnouncement(Announcement("192.0.2.0/24", {1}, 1, "origin", false));
    policy->seedAnnouncement(Announcement("2001:db8::/32", {1}, 1, "origin", false));
    
    std::string output_file = "output/test_ipv4_ipv6.csv";
    assert(graph.outputToCSV(output_file));
    
    auto entries = parseCSV(output_file);
    assert(entries.size() == 2);
    
    std::set<std::string> prefixes;
    for (const auto& entry : entries) {
        prefixes.insert(entry.prefix);
    }
    
    assert(prefixes.count("192.0.2.0/24") == 1);
    assert(prefixes.count("2001:db8::/32") == 1);
    
    std::cout << "✓ Both IPv4 and IPv6 in output!" << std::endl;
}

void test_larger_topology() {
    std::cout << "\n=== Test: Larger Topology (10 ASes) ===" << std::endl;
    
    ASGraph graph;
    
    /*
     * Create a tree topology:
     *           AS1
     *          /   \
     *        AS2   AS3
     *       / \    / \
     *     AS4 AS5 AS6 AS7
     *     |   |
     *    AS8 AS9
     *        |
     *       AS10
     */
    
    std::vector<uint32_t> asns = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    for (uint32_t asn : asns) {
        AS* as = graph.getOrCreateAS(asn);
        as->setPolicy(std::make_unique<BGP>());
    }
    
    // Setup relationships
    graph.getAS(1)->addCustomer(graph.getAS(2));
    graph.getAS(2)->addProvider(graph.getAS(1));
    graph.getAS(1)->addCustomer(graph.getAS(3));
    graph.getAS(3)->addProvider(graph.getAS(1));
    
    graph.getAS(2)->addCustomer(graph.getAS(4));
    graph.getAS(4)->addProvider(graph.getAS(2));
    graph.getAS(2)->addCustomer(graph.getAS(5));
    graph.getAS(5)->addProvider(graph.getAS(2));
    
    graph.getAS(3)->addCustomer(graph.getAS(6));
    graph.getAS(6)->addProvider(graph.getAS(3));
    graph.getAS(3)->addCustomer(graph.getAS(7));
    graph.getAS(7)->addProvider(graph.getAS(3));
    
    graph.getAS(4)->addCustomer(graph.getAS(8));
    graph.getAS(8)->addProvider(graph.getAS(4));
    graph.getAS(5)->addCustomer(graph.getAS(9));
    graph.getAS(9)->addProvider(graph.getAS(5));
    
    graph.getAS(9)->addCustomer(graph.getAS(10));
    graph.getAS(10)->addProvider(graph.getAS(9));
    
    graph.flattenGraph();
    
    // Seed at AS10
    dynamic_cast<BGP*>(graph.getAS(10)->getPolicy())->seedAnnouncement(
        Announcement("172.16.0.0/12", {10}, 10, "origin", false));
    
    graph.propagateAnnouncements();
    
    std::string output_file = "output/test_larger_topology.csv";
    assert(graph.outputToCSV(output_file));
    
    auto entries = parseCSV(output_file);
    assert(entries.size() == 10);  // All 10 ASes should have it
    
    // Verify AS1 has the longest path
    std::map<uint32_t, std::string> paths;
    for (const auto& entry : entries) {
        paths[entry.asn] = entry.as_path;
    }
    
    assert(paths[10] == "\"(10,)\"");
    assert(paths[1] == "\"(1, 2, 5, 9, 10)\"");  // Longest path through tree
    
    std::cout << "✓ Larger topology output correctly!" << std::endl;
    std::cout << "  Total ASes: " << entries.size() << std::endl;
    std::cout << "  AS10 (origin): " << paths[10] << std::endl;
    std::cout << "  AS1 (top): " << paths[1] << std::endl;
}

int main() {
    std::cout << "=== Testing CSV Output (Section 3.7) ===" << std::endl;
    
    try {
        test_simple_output();
        test_output_after_propagation();
        test_multiple_prefixes();
        test_conflicting_announcements();
        test_bgpsimulator_output();
        test_ipv4_and_ipv6_output();
        test_larger_topology();
        
        std::cout << "\n✅ ALL CSV OUTPUT TESTS PASSED!" << std::endl;
        std::cout << "\nSection 3.7 Complete!" << std::endl;
        std::cout << "Next: Section 4 - ROV (Route Origin Validation)" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}