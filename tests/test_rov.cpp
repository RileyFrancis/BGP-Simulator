#include "as_graph.hpp"
#include "policy.hpp"
#include "announcement.hpp"
#include <iostream>
#include <fstream>
#include <cassert>
#include <memory>
#include <unordered_set>
#include <cstdio>

void test_rov_drops_invalid() {
    std::cout << "\n=== Test: ROV Drops ROV-Invalid Announcements ===" << std::endl;

    ROV rov;

    Announcement invalid("1.2.0.0/16", {666}, 666, "customer", true);
    rov.receiveAnnouncement(invalid);
    rov.processReceivedAnnouncements();

    assert(rov.getLocalRIB().empty());
    std::cout << "✓ ROV-invalid announcement was dropped" << std::endl;
}

void test_rov_accepts_valid() {
    std::cout << "\n=== Test: ROV Accepts ROV-Valid Announcements ===" << std::endl;

    ROV rov;

    Announcement valid("1.2.0.0/16", {777}, 777, "customer", false);
    rov.receiveAnnouncement(valid);
    rov.processReceivedAnnouncements();

    assert(rov.getLocalRIB().size() == 1);
    assert(rov.getLocalRIB().at("1.2.0.0/16").getNextHopASN() == 777);
    std::cout << "✓ ROV-valid announcement was accepted" << std::endl;
}

void test_rov_ignores_invalid_keeps_valid() {
    std::cout << "\n=== Test: ROV Ignores Invalid, Keeps Valid (Same Prefix) ===" << std::endl;

    ROV rov;

    Announcement invalid("1.2.0.0/16", {666}, 666, "customer", true);
    Announcement valid("1.2.0.0/16", {777}, 777, "customer", false);

    rov.receiveAnnouncement(invalid);
    rov.receiveAnnouncement(valid);
    rov.processReceivedAnnouncements();

    assert(rov.getLocalRIB().size() == 1);
    assert(rov.getLocalRIB().at("1.2.0.0/16").getNextHopASN() == 777);
    std::cout << "✓ ROV correctly ignored invalid, kept valid" << std::endl;
}

void test_bgp_does_not_drop_invalid() {
    std::cout << "\n=== Test: Plain BGP Accepts ROV-Invalid (No Defense) ===" << std::endl;

    BGP bgp;

    Announcement invalid("1.2.0.0/16", {666}, 666, "customer", true);
    bgp.receiveAnnouncement(invalid);
    bgp.processReceivedAnnouncements();

    assert(bgp.getLocalRIB().size() == 1);
    std::cout << "✓ BGP (non-ROV) still accepts rov_invalid announcements" << std::endl;
}

void test_rov_blocks_propagation() {
    std::cout << "\n=== Test: ROV Blocks Invalid From Propagating Further ===" << std::endl;

    /*
     * AS3 (BGP, origin of ROV-invalid ann)
     *  |
     * AS1 (ROV) -- AS2 (BGP, peer)
     *
     * AS1 should drop the invalid ann, so AS2 never receives it.
     */

    ASGraph graph;

    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);

    as1->addCustomer(as3);
    as3->addProvider(as1);
    as1->addPeer(as2);
    as2->addPeer(as1);

    as1->setPolicy(std::make_unique<ROV>());
    as2->setPolicy(std::make_unique<BGP>());
    as3->setPolicy(std::make_unique<BGP>());

    graph.flattenGraph();

    dynamic_cast<BGP*>(as3->getPolicy())->seedAnnouncement(
        Announcement("1.2.0.0/16", {3}, 3, "origin", true));

    graph.propagateAnnouncements();

    BGP* p1 = dynamic_cast<BGP*>(as1->getPolicy());
    BGP* p2 = dynamic_cast<BGP*>(as2->getPolicy());

    assert(p1->getLocalRIB().empty());
    assert(p2->getLocalRIB().empty());

    std::cout << "✓ AS1 (ROV) blocked invalid; AS2 (BGP peer) received nothing" << std::endl;
}

void test_rov_does_not_block_valid_propagation() {
    std::cout << "\n=== Test: ROV Does Not Block Valid Announcements ===" << std::endl;

    ASGraph graph;

    AS* as1 = graph.getOrCreateAS(1);
    AS* as2 = graph.getOrCreateAS(2);
    AS* as3 = graph.getOrCreateAS(3);

    as1->addCustomer(as3);
    as3->addProvider(as1);
    as1->addPeer(as2);
    as2->addPeer(as1);

    as1->setPolicy(std::make_unique<ROV>());
    as2->setPolicy(std::make_unique<BGP>());
    as3->setPolicy(std::make_unique<BGP>());

    graph.flattenGraph();

    dynamic_cast<BGP*>(as3->getPolicy())->seedAnnouncement(
        Announcement("1.2.0.0/16", {3}, 3, "origin", false));

    graph.propagateAnnouncements();

    BGP* p1 = dynamic_cast<BGP*>(as1->getPolicy());
    BGP* p2 = dynamic_cast<BGP*>(as2->getPolicy());

    assert(p1->getLocalRIB().size() == 1);
    assert(p2->getLocalRIB().size() == 1);

    std::cout << "✓ ROV allows valid announcements to propagate normally" << std::endl;
}

void test_initialize_policies() {
    std::cout << "\n=== Test: initializePolicies Assigns Correct Types ===" << std::endl;

    ASGraph graph;
    graph.getOrCreateAS(1);
    graph.getOrCreateAS(2);
    graph.getOrCreateAS(3);

    std::unordered_set<uint32_t> rov_asns = {1, 2};
    graph.initializePolicies(rov_asns);

    assert(dynamic_cast<ROV*>(graph.getAS(1)->getPolicy()) != nullptr);
    assert(dynamic_cast<ROV*>(graph.getAS(2)->getPolicy()) != nullptr);
    assert(dynamic_cast<ROV*>(graph.getAS(3)->getPolicy()) == nullptr);
    assert(dynamic_cast<BGP*>(graph.getAS(3)->getPolicy()) != nullptr);

    std::cout << "✓ AS1,AS2 -> ROV; AS3 -> BGP" << std::endl;
}

void test_seed_from_csv() {
    std::cout << "\n=== Test: seedFromCSV Seeds Correctly ===" << std::endl;

    const std::string fname = "output/test_seed_input.csv";
    {
        std::ofstream f(fname);
        f << "seed_asn,prefix,rov_invalid\n";
        f << "1,10.0.0.0/8,False\n";
        f << "2,10.0.0.0/8,True\n";
    }

    ASGraph graph;
    graph.getOrCreateAS(1);
    graph.getOrCreateAS(2);
    graph.initializePolicies({});

    graph.seedFromCSV(fname);

    BGP* p1 = dynamic_cast<BGP*>(graph.getAS(1)->getPolicy());
    BGP* p2 = dynamic_cast<BGP*>(graph.getAS(2)->getPolicy());

    assert(p1->getLocalRIB().size() == 1);
    assert(p1->getLocalRIB().at("10.0.0.0/8").isROVInvalid() == false);
    assert(p2->getLocalRIB().size() == 1);
    assert(p2->getLocalRIB().at("10.0.0.0/8").isROVInvalid() == true);

    std::cout << "✓ Announcements seeded from CSV with correct rov_invalid flags" << std::endl;
}

int main() {
    std::cout << "=== Testing ROV Functionality (Section 4) ===" << std::endl;

    try {
        test_rov_drops_invalid();
        test_rov_accepts_valid();
        test_rov_ignores_invalid_keeps_valid();
        test_bgp_does_not_drop_invalid();
        test_rov_blocks_propagation();
        test_rov_does_not_block_valid_propagation();
        test_initialize_policies();
        test_seed_from_csv();

        std::cout << "\n✅ ALL ROV TESTS PASSED!" << std::endl;
        std::cout << "\nSection 4 Complete!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
