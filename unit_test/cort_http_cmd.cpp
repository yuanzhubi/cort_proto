#ifdef CORT_HTTP_CMD
#include <unistd.h>
#include <stdio.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <sys/wait.h>
#include <vector>
#include <sys/epoll.h>
#include "../net/cort_tcp_listener.h"
#include <fcntl.h>

const char* http_port = "3000";

static uint32_t check_http_packet(const char *data, uint32_t len){
    if(len > 4 && 
        data[len-1] == '\n' && data[len-2] == '\r' && data[len-3] == '\n' && data[len-4] == '\r' 
        && data[0]=='G' && data[1]=='E' && data[2]=='T' && data[3]==' ' 
        && std::find(data+4, data+len, ' ') < data+len-4){ //query ends with ' '
        return len;
    }
    return 0;
}

static bool hex2dec_c( char input[2], char* output)
{
    char result0 = 0;
    if      ('a' <= input[0] && input[0] <='f') { result0=input[0]-97+10; }
    else if ('A' <= input[0] && input[0] <='F') { result0=input[0]-65+10; }
    else if ('0' <= input[0] && input[0] <='9') { result0=input[0]-48;    }
    else return false;
    
    char result1 = 0;
    if      ('a' <= input[1] && input[1] <='f') { result1=input[1]-97+10; }
    else if ('A' <= input[1] && input[1] <='F') { result1=input[1]-65+10; }
    else if ('0' <= input[1] && input[1] <='9') { result1=input[1]-48;    }
    else return false;

    *output = (result1*16+result0);
    return true;
}

struct HttpPacket{
    char*  cmd;
    std::vector<char*> args_list;
    bool init(char *data, size_t len)
    { 
        char *name = 0;
        char *end  = data + len;
        write(1, data, len);
        name = data + sizeof("GET /") - 1;
        data = std::find(name, end, '?');
        if(data != end){
            if(name == data){ // 127.0.0.1/?             
                              // 127.0.0.1./
                *name-- = '/';
                *name = '.';
                cmd = name;          
            }else{            // 127.0.0.1/cmd?
                              // 127.0.0../cmd 
                *data = 0;
                name -= 2;
                *name = '.';
                cmd = name;
                args_list.push_back(cmd);
            }
            ++data;
            
            char* query_end = std::find(data, end, ' ');
            if(query_end != end){
                *query_end = '&';
                for(char* index = data, *index_find = std::find(index, query_end, '&');true; 
                    index = ++index_find, index_find = std::find(index, query_end, '&')){
                    char* decoder = index, *new_cmd = index;
                    while(index != index_find){
                        if(*decoder == '%'){     //decoding %xx
                            ++decoder;
                            if(!(decoder+2 <= index_find)){
                                return false;
                            }if(!hex2dec_c(decoder++, index++)){
                                return false;
                            }
                            ++decoder;
                        }else if(*decoder == '+'){//replace '+' with ' ' 
                            *index++ = ' ';
                            ++decoder;
                        }else{
                            *index++ = *decoder++;
                        }
                    }
                    
                    *index = 0;
                    if(args_list.empty()){
                        args_list.push_back(cmd);
                    }else{
                        args_list.push_back(new_cmd);
                    }
                    if(query_end == index_find){ //The last '&' at query_end found
                        break;
                    }
                }
            }else{
                return false;
            }
        }else{// 127.0.0.1/cmd/blbalbal
            char* cmd_begin = name-2;
            *cmd_begin = '.';// 127.0.0../cmd/blbalbal
            *std::find(name, end, ' ') = 0;
            if(strcmp("./favicon.ico", cmd_begin) == 0){ //Some browser always send the request. Rejected!
                return false;
            }
            args_list.push_back(cmd_begin);
            cmd = cmd_begin;
        }
        args_list.push_back(0);
        return true;
    }
};

static char output_header[] =   "HTTP/1.1 200 OK\r\n"
                                "Connection: close\r\n"
                                "Transfer-Encoding: chunked\r\n"
                                "Content-Type: text/plain; charset=utf-8\r\n";

static char recv_bad_format[] = "Receive bad data format!";
static char pipe_create_error[] = "Pipe create error!";
static char fork_error[] = "Fork error!";


struct cort_http_proxy_server : cort_tcp_ctrler{
    CO_DECL(cort_http_proxy_server)
    HttpPacket packet;
    int fds[2];
    
    const static int read_buf_size = 0xfff0;
    char send_buf[read_buf_size + 8];
    static recv_buffer_ctrl::recv_buffer_size_t recv_check_function(recv_buffer_ctrl* arg, cort_tcp_ctrler* p){
        uint32_t size = p->get_recved_size();
        char* buf = p->get_recv_buffer();
        if(size == 0){
            return 0;
        }

        if(check_http_packet(buf, size) == size){
            if(((cort_http_proxy_server*)p)->packet.init(buf,size)) return size;
            return recv_buffer_ctrl::unexpected_data_received;
        }
        return 0;
    }
    
    cort_proto* on_finish(){
        cort_tcp_ctrler::on_finish();
        delete this;
        return 0;
    }
    
    struct cort_forward : public cort_fd_waiter{
        CO_DECL(cort_forward)
        std::string result;
        cort_proto* start(){
        CO_BEGIN
            result.resize(0);
            set_poll_request(EPOLLIN);
            CO_YIELD();
            if((get_poll_result() & EPOLLIN) != 0){
                char buf[read_buf_size];
                int count = read(get_cort_fd(), buf, sizeof(buf));
                if(count > 0){
                    result.assign(buf, count);
                }
            }
            remove_poll_request();
            CO_RETURN;
        CO_END
        }
    }fwder;
    bool wait_send_final_packet;
    cort_proto* start(){
        wait_send_final_packet = false;
        CO_BEGIN
            init_time_cost();
            set_timeout(1000000);
            set_recv_check_function(cort_http_proxy_server::recv_check_function);
            alloc_recv_buffer();
            CO_AWAIT(lock_recv());
            if(get_errno() != 0 || packet.cmd == 0){
                set_send_buffer(recv_bad_format, sizeof(recv_bad_format) - 1);
                lock_send();
                CO_RETURN;  
            }
            if(pipe(fds) == -1){
                set_send_buffer(pipe_create_error, sizeof(pipe_create_error) - 1);
                lock_send();
                CO_RETURN; 
            }
            pid_t pid;
            if ((pid = fork()) < 0){
                set_send_buffer(fork_error, sizeof(fork_error) - 1);
                lock_send();
                CO_RETURN; 
            }else if (pid == 0){
                close(fds[0]);
                if(dup2(fds[1], 1) != 1 || execv(packet.cmd, &(*packet.args_list.begin()))){
                   close(fds[1]);
                }
                exit(0);
            }
            close(fds[1]);
            fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL) | O_NONBLOCK);
            
            //fwder will forward the son process result.
            fwder.set_cort_fd(fds[0]);
            fwder.start();
            set_send_buffer(output_header, sizeof(output_header) - 1);
            CO_AWAIT_ALL(&fwder, lock_send());
            
            if(get_errno() != 0 ){
                puts(cort_socket_error_codes::error_info(get_errno()));
                fwder.close_cort_fd();
                CO_RETURN;
            }
            if(wait_send_final_packet){
                CO_RETURN;
            }
            if(!fwder.result.empty()){
                int size = (int)fwder.result.size();
                char* result = send_buf + snprintf(send_buf, sizeof(send_buf), "\r\n%X\r\n", size);
                memcpy(result, fwder.result.c_str(), size);
                set_send_buffer(send_buf, result - send_buf + size);
                CO_AWAIT_ALL_AGAIN(&fwder, lock_send());
            }
            
            fwder.close_cort_fd();
            set_send_buffer("\r\n0\r\n\r\n", 7);
            CO_AWAIT(lock_send());
        CO_END
    }
};

cort_tcp_listener proxy_listener;
struct stdio_switcher : public cort_fd_waiter{
    CO_DECL(stdio_switcher)
    cort_proto* on_finish(){
        remove_poll_request();
        proxy_listener.stop_listen();
        cort_timer_destroy();  
        return 0;
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

bool is_proxy = true;

int main(int argc, char* argv[]){

    if(argc > 1){
        http_port = argv[1];    
    }
    cort_timer_init();  

    uint8_t err_code;

    proxy_listener.set_listen_port((uint16_t)atoi(http_port));
    proxy_listener.set_ctrler_creator<cort_http_proxy_server, cort_tcp_server_waiter>();
    proxy_listener.start();
    
    if((err_code = proxy_listener.get_errno()) != 0){
        puts(cort_socket_error_codes::error_info(err_code));
        exit(err_code);
    }
    
    switcher.start();
    cort_timer_loop();
    cort_timer_destroy();
    return 0;   
}
#endif
