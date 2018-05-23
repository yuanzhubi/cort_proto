#ifdef CORT_AWAIT_ANY_TEST

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include "../cort_timeout_waiter.h"
#include "../cort_channel.h"
//We create a stdio echo test. We add some strange cases for channel test.
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
            CO_YIELD();
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
                cort_timer_destroy();
                CO_RETURN;
            }
            write(1,buf,result);
            CO_AGAIN;
        CO_END
        }
    };
    struct await_any_test : public cort_proto{
        stdio_echo_test test_cort0;
        CO_DECL(await_any_test)
        cort_proto* start(){
        CO_BEGIN
            CO_AWAIT_ANY_N(1, &test_cort0, new cort_timeout(1000));
            if(test_cort0.is_finished()){
                return start();
            }
            else{
                puts("timeout. Press ctrl+d to stop!");
            }
        CO_END
        }
    }test_cort1;
    printf("This will start an stdio echo test for the CO_AWAIT_ANY_N. \n");
    cort_timer_init();
    test_cort1.start();
    cort_timer_loop();
    cort_timer_destroy();
    puts("finished"); //valgrind test past
    return 0;
}

#endif