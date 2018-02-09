#!/bin/bash

g++ -Wall -g $@ *.cpp network/*.cpp unit_test/*.cpp  pressure_test/*.cpp -DCORT_PROTO_TEST -o unit_test/cort_proto_test
g++ -Wall -g $@ *.cpp network/*.cpp unit_test/*.cpp  pressure_test/*.cpp -DCORT_TIMEOUT_WAITER_TEST -o unit_test/cort_timeout_waiter_test
g++ -Wall -g $@ *.cpp network/*.cpp unit_test/*.cpp  pressure_test/*.cpp -DCORT_TCP_CTRLER_TEST -o unit_test/cort_tcp_ctrler_test
