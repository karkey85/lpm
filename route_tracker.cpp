
#include <sstream>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <arpa/inet.h>


#include "route_tracker.h"

extern "C" {
#include "patricia.h"
}

void freePatriciaNode(patricia_node_t* node) {

    if (!node) return;

    // left subtree
    if (node->l) freePatriciaNode(node->l);

    // right
    if (node->r) freePatriciaNode(node->r);

    // my data added 
    if (node->data) {
        delete static_cast<std::string*>(node->data);
        node->data = nullptr;
    }

    if (node->prefix) {
        Deref_Prefix(node->prefix);
        node->prefix = nullptr;
    }

    free(node);
}


RouteTracker::RouteTracker() {
    ip_tree_ = New_Patricia(32);
  //  ip_tree_->free_user_data = free_route_data;
}

RouteTracker::~RouteTracker() {
    {
        std::lock_guard<std::mutex> _lock(rt_mutex_);
        for (std::unordered_map<std::string, TrackedAddress>::iterator it = tracked_addresses_.begin(); it != tracked_addresses_.end(); ++it) {
            if (it->second.current_route) {
                delete it->second.current_route;
            }
        }
        tracked_addresses_.clear();
    }

    if (ip_tree_) {
        freePatriciaNode(ip_tree_->head);
        free(ip_tree_);
        ip_tree_ = nullptr;
    }    
    //if (ip_tree_) {
    //    Destroy_Patricia(ip_tree_, nullptr);
    //}
}

bool RouteTracker::addRoute(const std::string& prefix, const std::string& nexthop) {
    if (prefix.empty() || nexthop.empty()) {
        return false;
    }
    //std::cout << " addRoute: " << "pfx:" << prefix << "nh:" << nexthop << "\n";    
    IPAddress addr;
    if (!parseIP(prefix, addr)) {
        return false;
    }
    
        std::unique_lock<std::mutex> rlock(rt_mutex_);
    {
        insertRoute(addr, nexthop);
    }
    
    //notifyAffectedAddresses(addr);
    std::vector<NotificationData> notifications;
    
    {
        //std::lock_guard<std::mutex> tlock(rt_mutex_);
        
        for (std::unordered_map<std::string, TrackedAddress>::iterator it = tracked_addresses_.begin(); it != tracked_addresses_.end(); ++it) {
            std::string ip_str = it->first;
            TrackedAddress& tracked = it->second;
            
            Route* new_route = longestPrefixMatch(ip_str);
            
            bool route_changed = false;
            if ((new_route == nullptr && tracked.current_route != nullptr) ||
                (new_route != nullptr && tracked.current_route == nullptr) ||
                (new_route != nullptr && tracked.current_route != nullptr && 
                 (new_route->prefix != tracked.current_route->prefix || new_route->nexthop != tracked.current_route->nexthop))) {
                route_changed = true;
            }
            
            if (route_changed) {
                NotificationData data;
                data.ip_address = ip_str;
                data.old_nexthop = tracked.current_route ? tracked.current_route->nexthop : "";
                data.new_nexthop = new_route ? new_route->nexthop : "";
                data.callback = tracked.callback;
                
                if (tracked.current_route) {
                    delete tracked.current_route;
                }
                tracked.current_route = new_route;
                
                notifications.push_back(data);
            } else {
                delete new_route;
            }
        }
    }
    rlock.unlock(); 
    for (size_t i = 0; i < notifications.size(); ++i) {
        try {
            notifications[i].callback(notifications[i].ip_address, notifications[i].new_nexthop, notifications[i].old_nexthop);
        } catch (...) {
        }
    }

    return true;
}

bool RouteTracker::deleteRoute(const std::string& prefix) {
    IPAddress addr;
    if (!parseIP(prefix, addr)) {
        return false;
    }
    
    //std::cout << " deleteRoute: " << "pfx:" << prefix << "\n";    
    std::unique_lock<std::mutex> rlock(rt_mutex_);
    bool deleted;
    {
        deleted = removeRoute(addr);
    }
    
    if (deleted) {
    //    notifyAffectedAddresses(addr);
    std::vector<NotificationData> notifications;
    
    {
        //std::lock_guard<std::mutex> tlock(rt_mutex_);
        
        for (std::unordered_map<std::string, TrackedAddress>::iterator it = tracked_addresses_.begin(); it != tracked_addresses_.end(); ++it) {
            std::string ip_str = it->first;
            TrackedAddress& tracked = it->second;
            
            Route* new_route = longestPrefixMatch(ip_str);
            
            bool route_changed = false;
            if ((new_route == nullptr && tracked.current_route != nullptr) ||
                (new_route != nullptr && tracked.current_route == nullptr) ||
                (new_route != nullptr && tracked.current_route != nullptr && 
                 (new_route->prefix != tracked.current_route->prefix || new_route->nexthop != tracked.current_route->nexthop))) {
                route_changed = true;
            }
            
            if (route_changed) {
                NotificationData data;
                data.ip_address = ip_str;
                data.old_nexthop = tracked.current_route ? tracked.current_route->nexthop : "";
                data.new_nexthop = new_route ? new_route->nexthop : "";
                data.callback = tracked.callback;
                
                if (tracked.current_route) {
                    delete tracked.current_route;
                }
                tracked.current_route = new_route;
                
                notifications.push_back(data);
            } else {
                delete new_route;
            }
        }
    }
    rlock.unlock(); 
    for (size_t i = 0; i < notifications.size(); ++i) {
        try {
            notifications[i].callback(notifications[i].ip_address, notifications[i].new_nexthop, notifications[i].old_nexthop);
        } catch (...) {
        }
    }

    }
    
    return deleted;
}

// this is internal and protected by lock.
Route* RouteTracker::longestPrefixMatch(const std::string& ip_address) const {
    IPAddress addr;
    if (!parseIPAddress(ip_address, addr)) {
        return nullptr;
    }
    
    addr.prefix_length = 32;
    
    //std::lock_guard<std::mutex> rlock(rt_mutex_);
    
    return findLongestMatch(addr);
}

bool RouteTracker::registerAddress(const std::string& ip_address, RouteChangeCallback callback) {
    if (ip_address.empty() || !callback) {
        return false;
    }
    
    IPAddress addr;
    if (!parseIPAddress(ip_address, addr)) {
        return false;
    }
    
    std::unique_lock<std::mutex> tlock(rt_mutex_);
    Route* route = longestPrefixMatch(ip_address);
  // invoke callback so remove locks before that
                NotificationData data;
                data.ip_address = ip_address;
                data.new_nexthop = route ? route->nexthop : "";
                data.old_nexthop = "";
                data.callback = callback;
    
    
    std::unordered_map<std::string, TrackedAddress>::iterator it = tracked_addresses_.find(ip_address);
    if (it != tracked_addresses_.end() && it->second.current_route) {
        delete it->second.current_route;
    }
    
    tracked_addresses_[ip_address] = {callback, route};
    
#if 1
  tlock.unlock();
 
       data.callback(data.ip_address, data.new_nexthop, data.old_nexthop);
#endif

    return true;
}

bool RouteTracker::unregisterAddress(const std::string& ip_address) {
    std::unique_lock<std::mutex> tlock(rt_mutex_);
     RouteChangeCallback local_callback; 
    std::unordered_map<std::string, TrackedAddress>::iterator it = tracked_addresses_.find(ip_address);
    if (it != tracked_addresses_.end()) {
        local_callback = it->second.callback;
        if (it->second.current_route) {
            delete it->second.current_route;
        }
        tracked_addresses_.erase(it);
#if 1
  tlock.unlock();
  // invoke callback so remove locks before that
                NotificationData data;
                data.ip_address = ip_address;
                data.new_nexthop = "";
                data.old_nexthop = "";
                data.callback = local_callback;
 
       data.callback(data.ip_address, data.new_nexthop, data.old_nexthop);
#endif
        return true;
    }
    return false;
}

std::vector<Route> RouteTracker::getAllRoutes() const {
    std::lock_guard<std::mutex> rlock(rt_mutex_);
    
    std::vector<Route> routes;
    
    traversePatriciaTree(ip_tree_, routes);
    
return routes;
}

void RouteTracker::insertRoute(const IPAddress& addr, const std::string& nexthop) {
    patricia_tree_t* tree =ip_tree_; 
    
    prefix_t* prefix;
    {
        struct in_addr sin;
        memcpy(&sin, addr.bytes, sizeof(struct in_addr));
        prefix = New_Prefix(AF_INET, &sin, addr.prefix_length);
    }
    
    patricia_node_t* node =  patricia_lookup(tree, prefix);
    
    if (node->data) {
        delete static_cast<std::string*>(node->data);
    }
    node->data = new std::string(nexthop);
    
    Deref_Prefix(prefix);
}

bool RouteTracker::removeRoute(const IPAddress& addr) {
    patricia_tree_t* tree = ip_tree_;
    
    prefix_t* prefix;
    //if (addr.version == IPVersion::IPv4) {
        struct in_addr sin;
        memcpy(&sin, addr.bytes, sizeof(struct in_addr));
        prefix = New_Prefix(AF_INET, &sin, addr.prefix_length);
    //}
    
    patricia_node_t* node = patricia_search_exact(tree, prefix);
    Deref_Prefix(prefix);
    
    if (!node) {
        return false;
    }
    
    if (node->data) {
        delete static_cast<std::string*>(node->data);
        node->data = nullptr;
    }
    
    patricia_remove(tree, node);
    
    return true;
}

Route* RouteTracker::findLongestMatch(const IPAddress& addr) const {
    patricia_tree_t* tree = ip_tree_;
    
    prefix_t* prefix;
    //if (addr.version == IPVersion::IPv4) {
        struct in_addr sin;
        memcpy(&sin, addr.bytes, sizeof(struct in_addr));
        prefix = New_Prefix(AF_INET, &sin, addr.prefix_length);
    //}
    
    patricia_node_t* node = patricia_search_best(tree, prefix);
    Deref_Prefix(prefix);
    
    if (!node || !node->data) {
        return nullptr;
    }
    
    std::string prefix_str = prefix_toa(node->prefix);
    std::string nexthop = *static_cast<std::string*>(node->data);
    
    return new Route(prefix_str, nexthop);
}
#if 0
void RouteTracker::notifyAffectedAddresses(const IPAddress& /* changed_network */) {
    std::vector<NotificationData> notifications;
    
    {
        std::lock_guard<std::mutex> tlock(rt_mutex_);
        
        for (std::unordered_map<std::string, TrackedAddress>::iterator it = tracked_addresses_.begin(); it != tracked_addresses_.end(); ++it) {
            std::string ip_str = it->first;
            TrackedAddress& tracked = it->second;
            
            Route* new_route = longestPrefixMatch(ip_str);
            
            bool route_changed = false;
            if ((new_route == nullptr && tracked.current_route != nullptr) ||
                (new_route != nullptr && tracked.current_route == nullptr) ||
                (new_route != nullptr && tracked.current_route != nullptr && 
                 (new_route->prefix != tracked.current_route->prefix || new_route->nexthop != tracked.current_route->nexthop))) {
                route_changed = true;
            }
            
            if (route_changed) {
                NotificationData data;
                data.ip_address = ip_str;
                data.old_nexthop = tracked.current_route ? tracked.current_route->nexthop : "";
                data.new_nexthop = new_route ? new_route->nexthop : "";
                data.callback = tracked.callback;
                
                if (tracked.current_route) {
                    delete tracked.current_route;
                }
                tracked.current_route = new_route;
                
                notifications.push_back(data);
            } else {
                delete new_route;
            }
        }
    }
    
    for (size_t i = 0; i < notifications.size(); ++i) {
        try {
            notifications[i].callback(notifications[i].ip_address, notifications[i].new_nexthop, notifications[i].old_nexthop);
        } catch (...) {
        }
    }
}
#endif
void RouteTracker::traversePatriciaTree(patricia_tree_t* tree, std::vector<Route>& routes) const {
    if (!tree || !tree->head) return;
    
    patricia_node_t* node;
    PATRICIA_WALK(tree->head, node) {
        if (node->data) {
            std::string prefix_str = prefix_toa(node->prefix);
            std::string nexthop = *static_cast<std::string*>(node->data);
            routes.push_back(Route(prefix_str, nexthop));
        }
    } PATRICIA_WALK_END;
}

// Helpr for future compatibility for ipv6
bool RouteTracker::parseIPAddress(const std::string& ip_str, IPAddress& result) const {
    if (ip_str.empty()) {
        return false;
    }
    
    if (parseIPv4(ip_str, result)) {
        result.prefix_length = 32;
        return true;
    }
    
    return false;
}

bool RouteTracker::parseIPv4(const std::string& ip_str, IPAddress& result) const {
    memset(result.bytes, 0, 16);
    
    if (inet_pton(AF_INET, ip_str.c_str(), result.bytes) == 1) {
        return true;
    }
    return false;
}

bool RouteTracker::parseIP(const std::string& cidr, IPAddress& result) const {
    if (cidr.empty()) {
        return false;
    }
    
    size_t slash_pos = cidr.find('/');
    if (slash_pos == std::string::npos) {
        return false;
    }
    
    if (slash_pos == 0 || slash_pos == cidr.length() - 1) {
        return false;
    }
    
    std::string ip_part = cidr.substr(0, slash_pos);
    std::string prefix_part = cidr.substr(slash_pos + 1);
    
    if (!parseIPAddress(ip_part, result)) {
        return false;
    }
    
    try {
        int prefix_len = std::stoi(prefix_part);
        int max_len = 32;
        
        if (prefix_len < 0 || prefix_len > max_len) {
            return false;
        }
        
        result.prefix_length = prefix_len;
        return true;
    } catch (...) {
        return false;
    }

}


