#ifdef CORT_SERVER_ECHO_TEST
#include <unistd.h>
#include <stdio.h>
#include "../net/cort_tcp_listener.h"
int sleep_ms_count = 0;
unsigned int error_count_total;
unsigned int success_count_total;
unsigned int total_time_cost;

struct print_result_cort: public cort_auto_delete{
    CO_DECL(print_result_cort)
    cort_proto* start(){
        CO_BEGIN
            unsigned int total = error_count_total + success_count_total;
            if(total == 0){
                total = 1;
            }
            printf("succeed: %u, error: %u, averaget_time_cost: %fms \n", success_count_total, error_count_total, ((double)(total_time_cost))/total);
            success_count_total = 0, error_count_total = 0, total_time_cost = 0;
        CO_END
    }
};

struct cort_tcp_echo_server : cort_tcp_ctrler{
    CO_DECL(cort_tcp_echo_server)
    static recv_buffer_ctrl::recv_buffer_size_t recv_check_function(recv_buffer_ctrl* arg, cort_tcp_ctrler* p){
        uint32_t size = p->get_recv_buffer_size();
        char* buf = p->get_recv_buffer();
        if(size == 0){
            return 0;
        }
        if(buf[size-1] == '\0'){
            return size;
        }
        return 0;
    }
    
    void on_finish(){
        cort_tcp_ctrler::on_finish();
        finish_time_cost();
        total_time_cost += get_time_cost();
        if(get_errno() != 0){ //Remote closes the connection? Or remote sends data that break the "ping-pong" rule? Or timeout.
            ++error_count_total;
        }
        else{
            ++success_count_total;
        }
        on_connection_inactive();
        delete this;
    }
    cort_proto* start(){
        CO_BEGIN
            init_time_cost();
            set_timeout(300);
            set_keep_alive(1000);
            set_recv_check_function(cort_tcp_echo_server::recv_check_function);
            alloc_recv_buffer();
            CO_AWAIT(lock_recv());
            if(get_errno() != 0){
                CO_RETURN;  
            }
            CO_SLEEP_IF(sleep_ms_count != 0, sleep_ms_count); //We simulate the real payload time cost.
            if(get_errno() != 0){ //Remote closes the connection? Or remote sends data that break the "ping-pong" rule? Or timeout.
                CO_RETURN;  
            }
            set_send_buffer(get_recv_buffer(), get_recv_buffer_size());
            CO_AWAIT(lock_send());
            if(get_errno() != 0){
                CO_RETURN;  
            }
        CO_END
    }
};

cort_tcp_listener listener, listener1, listener2;
#include <sys/epoll.h>
struct stdio_switcher : public cort_fd_waiter{
    CO_DECL(stdio_switcher)
    void on_finish(){
        remove_poll_request();
        listener.stop_listen();
        listener1.stop_listen();
        listener2.stop_listen();
        cort_timer_destroy();   
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
    if(argc > 1){
        sleep_ms_count = atoi(argv[1]);
    }
    cort_timer_init();  
    printf( "This will start an echo server listen port 8888, 8889, 8890. Press ctrl+d to stop. \n"
            "arg1: sleep microseconds before response, default: 0. \n"
    );
    listener.set_listen_port(8888);
    listener1.set_listen_port(8889);
    listener2.set_listen_port(8890);
    uint8_t err_code;

    listener.set_ctrler_creator<cort_tcp_echo_server, cort_tcp_server_waiter>();
    listener1.set_ctrler_creator<cort_tcp_echo_server, cort_tcp_server_waiter>();
    listener2.set_ctrler_creator<cort_tcp_echo_server, cort_tcp_server_waiter>();
    listener.start();
    if((err_code = listener.get_errno()) != 0){
        puts(cort_socket_error_codes::error_info(err_code));
    }
    listener1.start();
    if((err_code = listener1.get_errno()) != 0){
        puts(cort_socket_error_codes::error_info(err_code));
    }
    listener2.start();
    if((err_code = listener2.get_errno()) != 0){
        puts(cort_socket_error_codes::error_info(err_code));
    }
    switcher.start();
    cort_repeater<print_result_cort> logger;
    logger.set_repeat_per_second(1);
    logger.start();
    cort_timer_loop();
    cort_timer_destroy();
    return 0;   
}
#endif
