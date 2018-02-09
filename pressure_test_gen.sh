#!/bin/bash

g++ -Wall -g  $@ *.cpp network/*.cpp unit_test/*.cpp  pressure_test/*.cpp -DCORT_SERVER_ECHO_TEST -o pressure_test/cort_server_echo_test
g++ -Wall -g  $@ *.cpp network/*.cpp unit_test/*.cpp  pressure_test/*.cpp -DCORT_CLIENT_ECHO_TEST -o pressure_test/cort_client_echo_test
