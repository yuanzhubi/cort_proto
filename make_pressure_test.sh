#!/bin/bash

g++ -Wall -g  $@ time/*.cpp net/*.cpp  pressure_test/*.cpp -DCORT_SERVER_ECHO_TEST -Wl,-rpath=./ -o bin_test/cort_server_echo_test.out
g++ -Wall -g  $@ time/*.cpp net/*.cpp  pressure_test/*.cpp -DCORT_CLIENT_ECHO_TEST -Wl,-rpath=./ -o bin_test/cort_client_echo_test.out
g++ -Wall -g  $@ time/*.cpp net/*.cpp  pressure_test/*.cpp -DCORT_CLIENT_ECHO_INFINITE_TEST -Wl,-rpath=./ -o bin_test/cort_client_echo_infinite_test.out

#create a hooked version of libcurl.a
cp pressure_test/curl/lib/libcurl.a pressure_test/curl/lib/libcurl_hook.a
./static_hook.sh ./pressure_test/curl/lib/libcurl_hook.a

g++ -Wall -g  $@ time/*.cpp net/*.cpp  pressure_test/*.cpp stackful/*.cpp stackful/*.S -DCORT_HOOK_LIBCURL_TEST -I./pressure_test/curl/include -L./pressure_test/curl/lib -lcurl_hook -lrt -lpthread -Wl,-rpath=./ -o bin_test/cort_hook_libcurl_test.out
g++ -Wall -g  $@ time/*.cpp net/*.cpp  pressure_test/*.cpp stackful/*.cpp stackful/*.S -DCORT_HOOK_LIBCURL_TEST -I./pressure_test/curl/include -L./pressure_test/curl/lib -lcurl -lrt -lpthread -Wl,-rpath=./ -o bin_test/cort_libcurl_test.out
