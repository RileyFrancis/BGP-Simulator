#include "as_graph.hpp"
#include <iostream>
#include <cassert>

void test_simple_graph() {
    std::cout << "\n=== Test: Simple Manual Graph ===" << std::endl;
    
    ASGraph graph;
    
    // Manually create a simple graph
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    
    // AS1 is customer of AS2
    as2->addCustomer(as1);
    as1->addProvider(as2);
    
    // AS2 is customer of AS3
    as3->addCustomer(as2);
    as2->addProvider(as3);
    
    assert(graph.getNumASes() == 3);
    assert(as1->getProviders().size() == 1);
    assert(as2->getProviders().size() == 1);
    assert(as3->getCustomers().size() == 1);
    
    // No cycles
    assert(!graph.hasCycles());
    
    std::cout << "✓ Simple graph test passed!" << std::endl;
}

void test_cycle_detection() {
    std::cout << "\n=== Test: Cycle Detection ===" << std::endl;
    
    ASGraph graph;
    
    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);
    
    // Create a cycle: 1 -> 2 -> 3 -> 1
    as2->addCustomer(as1);
    as1->addProvider(as2);
    
    as3->addCustomer(as2);
    as2->addProvider(as3);
    
    as1->addCustomer(as3);  // This creates the cycle!
    as3->addProvider(as1);
    
    assert(graph.hasCycles());
    
    std::cout << "✓ Cycle detection test passed!" << std::endl;
}

void test_caida_file() {
    std::cout << "\n=== Test: Read CAIDA File ===" << std::endl;
    
    ASGraph graph;
    
    // Test with one of the provided files
    bool success = graph.buildFromCAIDAFile("bench/prefix/CAIDAASGraphCollector_2025.10.15.txt");
    
    assert(success);
    assert(graph.getNumASes() > 0);
    
    std::cout << "✓ CAIDA file test passed!" << std::endl;
    std::cout << "  ASes: " << graph.getNumASes() << std::endl;
    std::cout << "  Provider-Customer edges: " << graph.getNumProviderCustomerEdges() << std::endl;
    std::cout << "  Peer edges: " << graph.getNumPeerEdges() << std::endl;
}

int main() {
    std::cout << "Running AS Graph Tests..." << std::endl;
    
    test_simple_graph();
    test_cycle_detection();
    test_caida_file();
    
    std::cout << "\n✓ All tests passed!" << std::endl;
    return 0;
}