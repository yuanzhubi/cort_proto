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

const char* default_http_port = "3000";

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
    int type; //0 output text; 1 download file
    bool init(char *data, size_t len)
    { 
        type = 0;
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
        
        if(args_list.size() == 2 && strcmp("download", args_list[1]) == 0){
            type = 1;
            cmd = (char*)"/bin/cat";
            args_list[1] = args_list[0];
            args_list[0] = cmd;
        }
        copy(args_list.begin(), args_list.end(), std::ostream_iterator<const char*>(std::cout, " "));
        args_list.push_back(0);
        return true;
    }
};

#define HEADER_COMMON   "Connection: close\r\n" \
                        "Transfer-Encoding: chunked\r\n" \
                        "Content-Type: text/plain; charset=utf-8\r\n"
                        
#define HEADER_DOWNLOAD "Connection: close\r\n" \
                        "Transfer-Encoding: chunked\r\n" \
                        "Content-Type: application/octet-stream\r\n"
                        
#define HEADER_200  "HTTP/1.1 200 OK\r\n" HEADER_COMMON
#define HEADER_200_DOWNLOAD  "HTTP/1.1 200 OK\r\n" HEADER_DOWNLOAD
#define HEADER_400  "HTTP/1.1 400 Bad Request\r\n" HEADER_COMMON
#define HEADER_404  "HTTP/1.1 404 Not Found\r\n" HEADER_COMMON
#define HEADER_500  "HTTP/1.1 500 Internal Error\r\n" HEADER_COMMON
#define END_COMMON  "\r\n0\r\n\r\n"

//static bool is_spawn_process = false;

static char recv_bad_format[] = HEADER_400 END_COMMON;

static char fork_error[] = HEADER_500 END_COMMON;

static char pipe_create_error[] = HEADER_500 END_COMMON;

static char pipe_dup_error[] = HEADER_500 END_COMMON;

static char exec_error[] = HEADER_404 END_COMMON;

cort_tcp_listener proxy_listener;

struct cort_http_proxy_server : cort_tcp_ctrler{
    CO_DECL(cort_http_proxy_server)
    HttpPacket packet;
    int fds[2];
    
    std::string send_result;
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
        bool is_first;
        bool is_finished;
        int type;
        cort_forward():is_first(true), is_finished(false){
        }
        void pack_result(const char* buf, int count){
            if(is_first){
                if(count > 9 && memcmp("HTTP/1.1 ", buf, 9) == 0){
                    result.assign(buf, count);
                    return;
                }
                if(is_finished ){
                    if(count == 0){
                        result = HEADER_200 END_COMMON;
                        return;
                    }else{
                        result = HEADER_200;
                    }
                }else{
                    switch(type){
                    case 1:
                        result = HEADER_200_DOWNLOAD;
                        break;
                    default:
                        result = HEADER_200;
                    }
                    
                }
            }else if(count == 0){
                result = END_COMMON;
                return;
            }
            char buf_count[12];
            snprintf(buf_count, 12, "\r\n%X\r\n", count);
            result += buf_count;
            result.append(buf, count);
            if(is_finished && count != 0){
                result += END_COMMON;
            }
        }
        cort_proto* start(){
        const static int read_buf_size = 0xfff0;
        CO_BEGIN
            is_finished = false;
            result.resize(0);
            set_poll_request(EPOLLIN);
            set_timeout(10000000);
            CO_YIELD();
            if(is_timeout_or_stopped()){
                CO_RETURN;
            }
            if((get_poll_result() & EPOLLIN) != 0){
                char buf[read_buf_size];
                int count = read(get_cort_fd(), buf, sizeof(buf));
                if(count > 0){ //The return value of pipe reading is too strange for me so we do not 
                    //is_finished = (get_poll_result() != EPOLLIN); EPOLLHUP may appear when the son process exits.
                    is_finished = false;
                    pack_result(buf, count);
                }else{
                    is_finished = true;
                    pack_result(buf, 0);
                }
            }else{
                is_finished = true;
                pack_result(0, 0);
            }
            is_first = false;
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
            set_timeout(10000000);
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
                close(fds[0]);
                close(fds[1]);
                set_send_buffer(fork_error, sizeof(fork_error) - 1);
                lock_send();
                CO_RETURN; 
            }else if (pid == 0){
                close(fds[0]);
                if(dup2(fds[1], 1) != 1){
                    write(fds[1], pipe_dup_error, sizeof(pipe_dup_error) - 1);
                }else if(execv(packet.cmd, &(*packet.args_list.begin())) != 0){
                    write(fds[1], exec_error, sizeof(exec_error) - 1);
                }
                
                close(fds[1]);
                proxy_listener.stop_listen(); 
                exit(0);//We can not exit from main function gracefully.
            }
            close(fds[1]);
            
            fwder.type = packet.type;
            //fwder will forward the son process result.
            fwder.set_cort_fd(fds[0]);
            fwder.start();

            CO_AWAIT(&fwder);
            
            if(get_errno() != 0 ){
                puts(cort_socket_error_codes::error_info(get_errno()));  
                CO_RETURN;
            }
            
            if(!fwder.result.empty()){
                send_result.assign(fwder.result.c_str(), fwder.result.size());
                set_send_buffer(send_result.c_str(), send_result.size());
                
                if(!fwder.is_finished){//Waiting some more data from fwder and send back data.
                    CO_AWAIT_ALL_AGAIN(&fwder, lock_send());
                }
            }else{//Exception happened?
                CO_RETURN;
            }
            CO_AWAIT(lock_send()); //Sending the rest data.
        CO_END
    }
};

//Press ctrl+d to stop the server.
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
        default_http_port = argv[1];    
    }
    puts("./run port(default 3000)\r\n"
        "Press ctrl+d to stop the server.");
        
    cort_timer_init();  

    uint8_t err_code;

    proxy_listener.set_listen_port((uint16_t)atoi(default_http_port));
    proxy_listener.set_ctrler_creator<cort_http_proxy_server, cort_tcp_server_waiter>();
    proxy_listener.start();
    
    if((err_code = proxy_listener.get_errno()) != 0){
        puts(cort_socket_error_codes::error_info(err_code));
        exit(errno);
    }
    
    switcher.start();
    cort_timer_loop();
    cort_timer_destroy();
    return 0;   
}
#endif
