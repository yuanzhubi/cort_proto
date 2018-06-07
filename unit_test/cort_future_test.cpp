#ifdef CORT_FUTURE_TEST

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include "../cort_timeout_waiter.h"
#include "../cort_future.h"
//We create a stdio echo test. We add some strange cases for invalid order of cort_timer_init(), cort_timer_loop(), cort_timer_destroy().

cort_future future;
char buf[512]={0} ;
int last_time_out_ms = 5000;

struct cort_stdio_listen:public cort_fd_waiter{
    CO_DECL(cort_stdio_listen)
    cort_proto* start(){
        CO_BEGIN
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

            int result = read(0, buf, 511);
            if(result < 0){
                remove_poll_request();
                puts("read exception?");
                CO_RETURN;
            }
            remove_poll_request();
        CO_END
    }
}listener;

int main(int argc, char* argv[])
{
   
    if(argc > 1){
        last_time_out_ms = atoi(argv[1]);
    }
    printf("This will start an stdio echo test for the cort_time_waiter and cort_fd_waiter. \n");
    cort_timer_init();
    future.await(&listener);
    struct cort_echo:public cort_proto{
        CO_DECL(cort_echo)
        cort_proto* start(){
            CO_BEGIN
               CO_AWAIT(&future);
               puts(buf);
            CO_END
        }
    }a,b;
    a.start();
    b.start();
    cort_timer_loop();
    cort_timer_destroy();
    puts("finished"); //valgrind test past
    return 0;
}

#endif