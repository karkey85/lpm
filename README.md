LPM library implementation with callbacks to application:

Files used:
main.cpp ----> example test file which demos the usage of addRoute()/registerAddress
route_tracker.cpp  --> core library having API like addRoute()/deleteRoute()
route_tracker.h
patricia.cxx ---> open source patricia tree implementation
patricia.h

Assumptions/Future Enhancements:
1. Only IPv4 is supported by the library. Could extend for ipv6 but this is extensible for ipv6 both patricia tree and route tracker.
2. Assuming first time call to registerAddress/unregisterAddress also invokes callback

Design:
1. Used Patricia tree (patricia.cxx and patricia.h) from this blog. https://github.com/pavel-odintsov/fastnetmon/blob/master/src/libpatricia/patricia.c
2. Only one lock used for route add/delete and tracking address. This can be improved further,
3. Before invoking callbacks, locks are released using c++ scoped locks
4. currently notifyChangedAddresses in addRoute() and deleteRoute() is one pass of all entries in trackedIpadresses_ which can be improved by modifying or storing these trackedIPaddresses_ as prefix tree in another object.

Testing:
1. Basic prefix tree testing
2. API tests
3. Mutex log tests by spawning parallel threads and running the same APIs .
4. deadlock test also done by running different APIs in parallel threads 
5. used address sanitizer to check memory corruption, lock issue and use after free issue. fixed many using this g++ option -fsanitize=address -fno-omit-frame-pointer -g -O1

Compilation:
 g++ -fsanitize=address -fno-omit-frame-pointer -g -O1 main.cpp route_tracker.cpp patricia.cxx route_tracker.h patricia.h -lpthread -lm -o route_tracker
