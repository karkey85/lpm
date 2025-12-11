/**
 * @file main.cpp
 * @brief Test and demonstration program for RouteTracker
 */

#include "route_tracker.h"
#include <iostream>
#include <iomanip>
using namespace  std;

// Test Cases
void testAllRoutes() {
    std::cout << std::string(70, '=') << "\n";
    cout << "Test 1: Get All Routes" << std::endl;
    
    RouteTracker tracker;
    
    std::cout << "Adding multiple routes...\n";
    tracker.addRoute("10.0.0.0/8", "nh1");
    tracker.addRoute("172.16.0.0/12", "nh2");
    tracker.addRoute("192.168.0.0/16", "nh3");
    tracker.addRoute("192.168.1.0/24", "nh4");
    std::cout << "\n";
    
    std::cout << "All routes in the system:\n";
    std::vector<Route> routes = tracker.getAllRoutes();
    for (size_t i = 0; i < routes.size(); ++i) {
        std::cout << "  " << std::setw(25) << std::left << routes[i].prefix 
                 << " -> " << routes[i].nexthop << "\n";
    }
    
    std::cout << "\nTotal routes: " << routes.size() << "\n";
    tracker.deleteRoute("10.0.0.0/8");
    tracker.deleteRoute("172.16.0.0/12");
    tracker.deleteRoute("192.168.0.0/16");
    tracker.deleteRoute("192.168.1.0/24");
}

void testBasics() {
    std::cout << std::string(70, '=') << "\n";
    cout << "Test 2: Basic Route Operations" << std::endl;
    
    RouteTracker tracker;
    
    // Add routes
    std::cout << "Adding routes...\n";
    tracker.addRoute("192.168.0.0/16", "nh1");
    tracker.addRoute("192.168.1.0/24", "nh2");
    tracker.addRoute("192.168.1.128/25", "nh3");
    tracker.addRoute("10.0.0.0/8", "nh4");
    std::cout << "Added 4 routes\n\n";
    
    // Test longest prefix matching
    std::cout << "Testing longest prefix matching:\n";
    std::vector<std::string> test_ips = {
        "192.168.1.200",
        "192.168.1.50",
        "192.168.2.1",
        "10.5.5.5",
        "8.8.8.8"
    };
    
    for (size_t i = 0; i < test_ips.size(); ++i) {
        Route* result = tracker.longestPrefixMatch(test_ips[i]);
        std::cout << "  " << std::setw(15) << std::left << test_ips[i] << " -> ";
        if (result) {
            std::cout << "Route: " << std::setw(20) << result->prefix 
                     << " Nexthop: " << result->nexthop << "\n";
            delete result;
        } else {
            std::cout << "No route found\n";
        }
    }
    
    // Delete a route
    std::cout << "\nDeleting route 192.168.1.0/24...\n";
    tracker.deleteRoute("192.168.1.0/24");
    
    Route* result = tracker.longestPrefixMatch("192.168.1.50");
    std::cout << "  192.168.1.50 now matches: ";
    if (result) {
        std::cout << result->prefix << " -> " << result->nexthop << "\n";
        delete result;
    }
    tracker.deleteRoute("192.168.0.0/16" );
    tracker.deleteRoute("192.168.1.128/25");
    tracker.deleteRoute("10.0.0.0/8");
}

//typedef void (*RouteChangeCallback)(const std::string& ip_address,
//                                     const std::string& new_nexthop,
//                                     const std::string& old_nexthop);
static int callback_count = 0;
void appCallback(const std::string& ip_address,
                                     const std::string& new_nexthop,
                                     const std::string& old_nexthop) {
    callback_count++;
}

void testAddressTracker() {
    std::cout << std::string(70, '=') << "\n";
    cout << "Test 3: Address Tracking with Callbacks" << std::endl;
    
    RouteTracker tracker;
    
    // Add routes
    std::cout << "Adding routes...\n";
    tracker.addRoute("192.168.0.0/16", "nh1");

    std::cout << "Registering addresses for tracking:\n";
    tracker.registerAddress("192.168.0.1", &appCallback);
    
    tracker.addRoute("192.168.0.0/16", "nh2");

    std::cout << "Total callbacks invoked: " << callback_count << "\n";
    
    std::cout << "Deleting routes...\n";
    tracker.deleteRoute("192.168.0.0/16");
std::cout << "Total callbacks invoked: " << callback_count << "\n";
}


void testEdgeCases() {
    std::cout << std::string(70, '=') << "\n";
    cout << "Test 4: Edge Cases" << endl;
    
    RouteTracker tracker;
    
    // Test with default route
    std::cout << "Testing default route (0.0.0.0/0):\n";
    tracker.addRoute("0.0.0.0/0", "default");
    Route* result = tracker.longestPrefixMatch("1.2.3.4");
    if (result) {
        std::cout << "  1.2.3.4 -> " << result->prefix << " -> " << result->nexthop << "\n";
        delete result;
    }
    std::cout << "\n";
    
    std::cout << "Testing exact host match (8.8.8.8/32):\n";
    tracker.addRoute("8.8.8.8/32", "host");
    result = tracker.longestPrefixMatch("8.8.8.8");
    if (result) {
        std::cout << "  8.8.8.8 -> " << result->prefix << " -> " << result->nexthop << "\n";
        delete result;
    }
    std::cout << "\n";
    
    std::cout << "Deleting non-existent route:\n";
    bool deleted = tracker.deleteRoute("5.5.5.0/24");
    std::cout << "  Result: " << (deleted ? "Deleted" : "Not found") << "\n";
    std::cout << "\n";
    
    std::cout << "Testing route update:\n";
    tracker.addRoute("10.0.0.0/8", "nh_old");
    std::cout << "  Added: 10.0.0.0/8 -> nh_old\n";
    
    tracker.addRoute("10.0.0.0/8", "nh_new");
    std::cout << "  Updated to: new NH\n";
    
    result = tracker.longestPrefixMatch("10.1.2.3");
    if (result) {
        std::cout << "  Lookup result: " << result->nexthop << "\n";
        delete result;
    }

    tracker.deleteRoute("8.8.8.8/32");
    tracker.deleteRoute("0.0.0.0/0");
    tracker.deleteRoute("10.0.0.0/8");
}

struct ThreadArgs {
    long id;
    RouteTracker* tracker;
};

void* AddRouteThread(void* arg) {
    ThreadArgs* args = static_cast<ThreadArgs*>(arg);
    long id = args->id;
    RouteTracker* rTracker = args->tracker;

    for (int i = 0; i < 1000 ; i++) {
        // Create different prefixes for each iteration
        std::string prefix = "10." + std::to_string(id) + "." + std::to_string(i % 255) + ".0/24";
        std::string nexthop = "NH-" + std::to_string(id);

        rTracker->addRoute(prefix, nexthop);
        //std::cout << "Thread " << id << " running now and adding route: " << prefix << std::endl;

        // Optional: slow down to avoid flooding console
        // usleep(1000); // 1 ms
    }
    rTracker->getAllRoutes();

    std::cout << "Thread " << id << " (AddRoute) finished." << std::endl;
    return nullptr;
}

void* DelRouteThread(void* arg) {
    ThreadArgs* args = static_cast<ThreadArgs*>(arg);
    long id = args->id;
    RouteTracker* rTracker = args->tracker;

    for (int i = 0; i < 1000 ; i++) {
        // Create different prefixes for each iteration
        std::string prefix = "10." + std::to_string(id) + "." + std::to_string(i % 255) + ".0/24";
        std::string nexthop = "NH-" + std::to_string(id);

        rTracker->deleteRoute(prefix);
        //std::cout << "Thread " << id << " running now and deleting route: " << prefix << std::endl;

        // Optional: slow down to avoid flooding console
         //usleep(1000); // 1 ms
    }
    rTracker->getAllRoutes();

    std::cout << "Thread " << id << " (DeleteRoute) finished." << std::endl;
    return nullptr;
}

void* RegisterThread(void* arg) {
    ThreadArgs* args = static_cast<ThreadArgs*>(arg);
    long id = args->id;
    RouteTracker* rTracker = args->tracker;

    for (int i = 0; i < 1000 ; i++) {
        // Create different prefixes for each iteration
        std::string prefix = "10." + std::to_string(id) + "." + std::to_string(i % 255) + ".1";
        std::string nexthop = "NH-" + std::to_string(id);

        rTracker->registerAddress(prefix, &appCallback);
        //std::cout << "Thread " << id << " running now and register address: " << prefix << std::endl;

        // Optional: slow down to avoid flooding console
        // usleep(1000); // 1 ms
    }

    std::cout << "Thread " << id << " (register) finished." << std::endl;
    return nullptr;
}

void* UnregisterThread(void* arg) {
    ThreadArgs* args = static_cast<ThreadArgs*>(arg);
    long id = args->id;
    RouteTracker* rTracker = args->tracker;

    for (int i = 0; i < 1000 ; i++) {
        // Create different prefixes for each iteration
        std::string prefix = "10." + std::to_string(id) + "." + std::to_string(i % 255) + ".1";
        std::string nexthop = "NH-" + std::to_string(id);

        rTracker->unregisterAddress(prefix);
        //std::cout << "Thread " << id << " running now and unregister address " << prefix << std::endl;

        // Optional: slow down to avoid flooding console
        // usleep(1000); // 1 ms
    }

    std::cout << "Thread " << id << " (unregister) finished." << std::endl;
    return nullptr;
}

void testMutexLocks() {
    std::cout << std::string(70, '=') << "\n";
    cout << "Test 5: Mutex testing running parallel threads" << endl;
// create two threads
    pthread_t t1, t2;

    RouteTracker tracker;
    ThreadArgs args1{1, &tracker};
    ThreadArgs args2{2, &tracker};

    pthread_create(&t1, nullptr, AddRouteThread, &args1);
    pthread_create(&t2, nullptr, AddRouteThread, &args2);

    pthread_join(t1, nullptr);
    pthread_join(t2, nullptr);

    std::cout << "Both Add route threads joined!\n";

    pthread_t t3, t4;
    pthread_create(&t3, nullptr, DelRouteThread, &args1);
    pthread_create(&t4, nullptr, DelRouteThread, &args2);

    pthread_join(t3, nullptr);
    pthread_join(t4, nullptr);

    std::cout << "Both Del Route threads joined!\n";

    pthread_t t5, t6, t7, t8;
    pthread_create(&t5, nullptr, RegisterThread, &args1);
    pthread_create(&t6, nullptr, UnregisterThread, &args1);
    pthread_create(&t7, nullptr, RegisterThread, &args2);
    pthread_create(&t8, nullptr, UnregisterThread, &args2);

    pthread_join(t5, nullptr);
    pthread_join(t6, nullptr);
    pthread_join(t7, nullptr);
    pthread_join(t8, nullptr);

    std::cout << "All reg/unreg threads joined!\n";
}

void testDeadLocks() {
    std::cout << std::string(70, '=') << "\n";
    cout << "Test 5: DEADLOCK testing running parallel threads" << endl;
    RouteTracker tracker;
    ThreadArgs args1{1, &tracker};

    pthread_t t5, t6, t7, t8;
    pthread_create(&t5, nullptr, AddRouteThread, &args1);
    pthread_create(&t6, nullptr, DelRouteThread, &args1);
    pthread_create(&t7, nullptr, RegisterThread, &args1);
    pthread_create(&t8, nullptr, UnregisterThread, &args1);

    pthread_join(t5, nullptr);
    pthread_join(t6, nullptr);
    pthread_join(t7, nullptr);
    pthread_join(t8, nullptr);

    std::cout << "All threads joined!\n";
}

int main() {
    std::cout << "\n";
    std::cout << "      IP Route Tracker: Test Suite \n";
    
    try {
        testAllRoutes();
        testBasics();
        testAddressTracker();
        testEdgeCases();
        
        testMutexLocks();

        testDeadLocks();

        std::cout << "\n";
    std::cout << std::string(70, '=') << "\n";
        std::cout << "\n";
        
        std::cout << "\n";
        std::cout << " All tests completed successfully!\n";
        std::cout << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\n Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}


