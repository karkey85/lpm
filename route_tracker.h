//#define ENABLE_IPV6

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cstring>
#include <mutex>

struct _patricia_tree_t;
typedef struct _patricia_tree_t patricia_tree_t;

struct Route {
    std::string prefix;
    std::string nexthop;
    
    Route(const std::string& p, const std::string& nh) : prefix(p), nexthop(nh) {}
};

struct IPAddress {
    //IPVersion version;
    unsigned char bytes[16]; // support ipv6 in future
    int prefix_length;
    
    IPAddress() : prefix_length(0) {
        memset(bytes, 0, 16);
    }
};

typedef void (*RouteChangeCallback)(const std::string& ip_address,
                                     const std::string& new_nexthop,
                                     const std::string& old_nexthop);

class RouteTracker {
public:
    RouteTracker();
    ~RouteTracker();
    
    RouteTracker(const RouteTracker&) = delete;
    RouteTracker& operator=(const RouteTracker&) = delete;
    
    bool addRoute(const std::string& prefix, const std::string& nexthop);
    bool deleteRoute(const std::string& prefix);
    bool registerAddress(const std::string& ip_address, RouteChangeCallback callback);
    bool unregisterAddress(const std::string& ip_address);
    std::vector<Route> getAllRoutes() const;
    // exposing this lockless functioon for testing from single threaded environment
    Route* longestPrefixMatch(const std::string& ip_address) const;
private:
    bool parseIPAddress(const std::string& ip_str, IPAddress& result) const;
    bool parseIPv4(const std::string& ip_str, IPAddress& result) const;
    bool parseIP(const std::string& cidr, IPAddress& result) const;
    
    void insertRoute(const IPAddress& addr, const std::string& nexthop);
    bool removeRoute(const IPAddress& addr);
    Route* findLongestMatch(const IPAddress& addr) const;
    
    void notifyAffectedAddresses(const IPAddress& changed_network);
    void traversePatriciaTree(patricia_tree_t* tree, std::vector<Route>& routes) const;

    patricia_tree_t* ip_tree_;
    
    struct TrackedAddress {
        RouteChangeCallback callback;
        Route* current_route;
    };
    
    struct NotificationData {
        std::string ip_address;
        std::string old_nexthop;
        std::string new_nexthop;
        RouteChangeCallback callback;
    };
    
    std::unordered_map<std::string, TrackedAddress> tracked_addresses_;
    
    mutable std::mutex rt_mutex_;
};
