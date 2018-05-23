#ifdef CORT_TIMEOUT_WAITER_TEST

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include "../cort_timeout_waiter.h"
//We create a stdio echo test. We add some strange cases for invalid order of cort_timer_init(), cort_timer_loop(), cort_timer_destroy().
int main(int argc, char* argv[])
{
    int last_time_out_ms = 5000;
    if(argc > 1){
        last_time_out_ms = atoi(argv[1]);
    }
    printf("This will start an stdio echo test for the cort_time_waiter and cort_fd_waiter. \n");
    cort_timer_init();
    
    CO_ASYNC_LB(cort_fd_waiter, (
        set_cort_fd(0);
        set_poll_request(EPOLLIN|EPOLLHUP);
        set_timeout(last_time_out_ms);
        CO_YIELD();
        if(is_timeout()){
            printf("timeout! %dms cost\n", (int)get_time_past());
            set_timeout(last_time_out_ms); //We will check the timer accuracy.
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
        
        char buf[512] ;
        int result = read(0, buf, 511);
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
        set_timeout(last_time_out_ms);
        CO_AGAIN;
    ), last_time_out_ms) ;
    cort_timer_loop();
    cort_timer_destroy();
    puts("finished"); //valgrind test past
    return 0;
}

#endif