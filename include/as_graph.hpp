#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cstdint>
#include "as.hpp"   

/**
 * ASGraph - Manages the entire AS topology
 * Builds the graph from CAIDA data, detects cycles, and handles propagation
 */
class ASGraph {
public:
    ASGraph();
    ~ASGraph();
    
    // Build the graph from CAIDA topology file
    bool buildFromCAIDAFile(const std::string& filename);
    
    // Get an AS by its ASN (returns nullptr if not found)
    AS* getAS(uint32_t asn) const;
    
    // Get all ASes
    const std::unordered_map<uint32_t, std::unique_ptr<AS>>& getAllASes() const {
        return ases_;
    }
    
    // Check for cycles in provider-customer relationships
    bool hasCycles() const;
    
    // Graph statistics (useful for debugging)
    size_t getNumASes() const { return ases_.size(); }
    size_t getNumProviderCustomerEdges() const;
    size_t getNumPeerEdges() const;

    // Helper: Get or create an AS
    AS* getOrCreateAS(uint32_t asn);
    
private:
    // Storage for all AS objects (ASN -> AS object)
    std::unordered_map<uint32_t, std::unique_ptr<AS>> ases_;
    
    // Helper: Parse a single line from CAIDA file
    bool parseCAIDALine(const std::string& line);
    
    // Helper: DFS for cycle detection
    bool hasCyclesDFS(uint32_t asn, 
                      std::unordered_map<uint32_t, int>& visited,
                      std::unordered_map<uint32_t, int>& recursion_stack) const;
};