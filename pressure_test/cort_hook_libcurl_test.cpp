#ifdef CORT_HOOK_LIBCURL_TEST
#include <unistd.h>
#include <stdio.h>
#include <string>

#include <curl/curl.h>

#include "../net/cort_tcp_ctrler.h"
#include "../stackful/cort_stackful_hook.h"

int timeout = 300;
int keepalive_timeout = 300;

//const char *ip = "203.205.128.137";
const char* ip = "183.3.226.21";
unsigned int speed = 100;
std::string query = "http://";

unsigned int error_count_total = 0;
unsigned int success_count_total = 0;
unsigned int total_time_cost = 0;
unsigned int cort_count = 0;
struct print_result_cort: public cort_auto{
    CO_DECL(print_result_cort)
    cort_proto* start(){
        CO_BEGIN
            unsigned int total = error_count_total + success_count_total;
            if(total == 0){
                total = 1;
            }
            printf("succeed: %u, error: %u, fd_count:%u, cort_count:%u, averaget_cost: %-4.3fms \n", 
                success_count_total, error_count_total, (unsigned)cort_fd_waiter::cort_waited_fd_count_thread(), cort_count, ((double)(total_time_cost))/total);
            success_count_total = 0, error_count_total = 0, total_time_cost = 0;
        CO_END
    }
};

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata){
    return size * nmemb;
}

co_local<int> query_time_cost(0);
struct libcurl_cort : public cort_stackful_fds_waiter{
    CO_STACKFUL_DECL(libcurl_cort)
    libcurl_cort(){
        alloc_stack(12*1024);   //8K will lead some error
    }
    cort_proto* start(){
        CO_BEGIN
            ++cort_count;
            cort_timeout_waiter::time_ms_t begin_time = cort_timer_now_ms();
            CURL *curl;            
            CURLcode res;          
            curl = curl_easy_init();       
            if(curl != NULL){
                curl_easy_setopt(curl, CURLOPT_URL, query.c_str());  
                curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 2500L);
                curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 2L);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
                res = curl_easy_perform(curl);
                if(res == CURLE_OK){
                    ++success_count_total;
                }
                else{
                    ++error_count_total;
                }
                curl_easy_cleanup(curl);
            }
            cort_timeout_waiter::time_ms_t end_time = cort_timer_now_ms_refresh();
            total_time_cost += end_time - begin_time;
            query_time_cost = end_time - begin_time;
            *(&query_time_cost) =  end_time - begin_time;
            --cort_count;
        CO_END
    }
};

struct send_cort : public cort_auto{
    CO_DECL(send_cort)
    libcurl_cort cort_test0;
    
    cort_proto* start(){
        CO_BEGIN
            CO_AWAIT(&cort_test0);
        CO_END
    }
};

#include <sys/epoll.h>
struct stdio_switcher : public cort_fd_waiter{
    CO_DECL(stdio_switcher)
    cort_proto* on_finish(){
        remove_poll_request();
        cort_timer_destroy();   //This will stop the timer loop; 
                                //but you need to stop wait stdin first by remove_poll_request, or else the loop will wait this cort.
        return cort_fd_waiter::on_finish();
    }
    cort_proto* start(){
    CO_BEGIN
        set_cort_fd(0);
        set_poll_request(EPOLLIN|EPOLLHUP);
        CO_YIELD();
        if(get_poll_result() != EPOLLIN){
            puts("exception happened?");
            CO_RETURN;
        }
        char buf[1024] ;
        int result = read(0, buf, 1023);
        if(result == 0){    //using ctrl+d in *nix
            CO_RETURN;
        }
        CO_AGAIN;
    CO_END
    }
}switcher;

int main(int argc, char* argv[]){
    cort_timer_init();
    printf( "This will start a curl client test(The not hooked version can not elegant quit and is with poor performance). Press ctrl+d to stop. \n"
            "arg1: ip, default: 183.3.226.21 \n"
            "arg2: query per second, default: 100 \n"
    );
    if(argc > 1){
        ip = argv[1];
    }

    if(argc > 2){
        speed = (unsigned int)(atoi(argv[2]));
    }
    query += ip;
    //query += "/pingd?dm=www.qq.com&url=/"; //http://pingfore.qq.com/pingd?dm=news.qq.com&url=/  300ms latency, 62bytes response, 
    query += "/kvcollect";                   //http://btrace.qq.com/kvcollect                     300ms latency, 147bytes response
#if !defined(UNIT_TEST)
    cort_repeater<send_cort> tester;
    tester.set_repeat_per_second(speed);
    
    cort_repeater<print_result_cort> logger;
    logger.set_repeat_per_second(1);
    logger.start();
#else
    send_cort tester;
#endif
    
    tester.start();
    switcher.start();
    cort_timer_loop();
    
    printf("fd_count:%u\n", cort_fd_waiter::cort_waited_fd_count_thread());
    cort_timer_destroy();
    return 0;   
}

#endif