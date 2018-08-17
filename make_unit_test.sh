#!/bin/bash

g++ -Wall -g $@  unit_test/*.cpp   -DCORT_PROTO_TEST -Wl,-rpath=./ -o cort_proto_test.out
g++ -Wall -g $@  time/*.cpp unit_test/*.cpp   -DCORT_TIMEOUT_WAITER_TEST -Wl,-rpath=./ -o cort_timeout_waiter_test.out
g++ -Wall -g $@  time/*.cpp net/*.cpp unit_test/*.cpp   -DCORT_TCP_CTRLER_TEST -Wl,-rpath=./ -o cort_tcp_ctrler_test.out
g++ -Wall -g $@  time/*.cpp unit_test/*.cpp   -DCORT_FUTURE_TEST -Wl,-rpath=./ -o cort_future_test.out
