#ifdef CORT_TIMEOUT_WAITER_TEST

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include "../cort_timeout_waiter.h"
//We create a stdio echo test. We add some strange cases for invalid order of cort_timer_init(), cort_timer_loop(), cort_timer_destroy().
int main(void)
{
    struct stdio_echo_test : public cort_fd_waiter{
        int last_time_out;
        struct epoll_event ev;
        CO_DECL(stdio_echo_test)
        cort_proto* start(){
        CO_BEGIN
            set_cort_fd(0);
            set_poll_request(EPOLLIN|EPOLLHUP);
            last_time_out = 5000;
            set_timeout(last_time_out);
            CO_YIELD();
            if(is_timeout()){
                printf("timeout! %dms cost\n", (int)get_time_past());
                set_timeout((++last_time_out)%256); //We will check the timer accuracy.
                CO_AGAIN;   
            }
            if(is_stopped()){
                puts("current coroutine is canceled!");
                CO_RETURN;
            }
            if(get_poll_result() != EPOLLIN){
                remove_poll_request();
                puts("exception happened?");
                CO_RETURN;
            }
            
            char buf[1024] ;
            int result = read(0, buf, 1023);
            if(result < 0){
                remove_poll_request();
                puts("read exception?");
                CO_RETURN;
            }
            if(result == 0){//using ctrl+d in *nix
                remove_poll_request();
                CO_RETURN;
            }
            write(1,buf,result);
            last_time_out = 4095;
            set_timeout(last_time_out);
            CO_SLEEP_AGAIN(10);
        CO_END
        }
    }test_cort0, test_cort1;
    printf("This will start an stdio echo test for the cort_time_waiter and cort_fd_waiter. \n");
    cort_timer_init();
    test_cort0.start();
    cort_timer_loop();
    test_cort1.start();
    cort_timer_loop();
    test_cort0.start();
    test_cort1.start();
    //Test cort_timer_destroy without loop before!
    //cort_timer_loop();
    //Then the couroutine should be stopped after destroy.
    cort_timer_destroy();
    cort_timer_init();
    cort_timer_init();
    test_cort0.start();
    cort_timer_destroy();
    puts("finished"); //valgrind test past
    return 0;
}

#endif