#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <stdio.h>

#include "cort_tcp_listener.h"

#define RETURN_ERROR(x) do{ \
	set_errno(x); \
	return (x); \
}while(false)

cort_tcp_listener::cort_tcp_listener(){
	backlog = 0;
	listen_port = 0;
	errnum = 0;
}

cort_tcp_listener::~cort_tcp_listener(){
	stop_listen();
}

void cort_tcp_listener::stop_listen(){
	close_cort_fd();
}

void cort_tcp_listener::pause_accept(){
	remove_poll_request();
}

void cort_tcp_listener::resume_accept(){
	set_poll_request(EPOLLIN);
}

uint8_t cort_tcp_listener::listen_connect(){
	if(listen_port == 0){
		RETURN_ERROR(cort_socket_error_codes::SOCKET_INVALID_LISTEN_ADDRESS);
	}
	
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){
		RETURN_ERROR(cort_socket_error_codes::SOCKET_CREATE_ERROR);
    }

    int flag = fcntl(sockfd, F_GETFL);
	if (-1 == flag || fcntl(sockfd, F_SETFL, flag | O_NONBLOCK) == -1){
		close(sockfd);
		RETURN_ERROR(cort_socket_error_codes::SOCKET_CREATE_ERROR);
	}
	
	int cort_yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &cort_yes, sizeof(cort_yes)) < 0){
		close(sockfd);
		RETURN_ERROR(cort_socket_error_codes::SOCKET_CREATE_ERROR);
    }
	
	struct sockaddr_in bindaddr;
	bindaddr.sin_port = htons(listen_port);
	bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bindaddr.sin_family = AF_INET;
	
	if (bind(sockfd, (struct sockaddr *) &bindaddr, sizeof(bindaddr)) < 0) {
		close(sockfd);
		RETURN_ERROR(cort_socket_error_codes::SOCKET_BIND_ERROR);
	}
	
	if(backlog == 0){
		backlog = 32;
	}
	
	int result;
	result = listen(sockfd, backlog);
	if(result < 0){
		close(sockfd);
		RETURN_ERROR(cort_socket_error_codes::SOCKET_LISTEN_ERROR);
	}
	set_cort_fd(sockfd);
	set_poll_request(EPOLLIN);
	return 0;
}

cort_proto* cort_tcp_listener::start(){
	CO_BEGIN
		if(get_cort_fd() < 0 && listen_connect() != 0){
			set_errno(cort_socket_error_codes::SOCKET_CREATE_ERROR);
			CO_RETURN;
		}
		CO_YIELD();
		if(is_timeout_or_stopped()){
			set_errno(cort_socket_error_codes::SOCKET_OPERATION_TIMEOUT);
			stop_listen();
			CO_RETURN;	
		}
		uint32_t poll_event = get_poll_result();
		if( (EPOLLIN & poll_event) == 0){
			set_errno(cort_socket_error_codes::SOCKET_CONNECT_REJECTED);
			stop_listen();
			CO_RETURN;	
		}
		int accept_fd ;
		int listen_fd = get_cort_fd();
		struct sockaddr_in servaddr;
		socklen_t addrlen = sizeof(servaddr);
	start_accept:
		#if !defined(__linux__)
		accept_fd = accept(listen_fd, (struct sockaddr*)&servaddr, &addrlen);
		if(accept_fd > 0){
			int flag = fcntl(accept_fd, F_GETFL);
			if (-1 == flag || fcntl(accept_fd, F_SETFL, flag | O_NONBLOCK) == -1){
				close(accept_fd);
				CO_AGAIN;
			}
			ctrler_creator(accept_fd, servaddr.sin_addr.s_addr, servaddr.sin_port);
			goto start_accept;
		}
		#else
		accept_fd = accept4(listen_fd, (struct sockaddr*)&servaddr, &addrlen, SOCK_NONBLOCK);
		if(accept_fd > 0){
			ctrler_creator(accept_fd, servaddr.sin_addr.s_addr, servaddr.sin_port, 0);
			goto start_accept;
		}
		#endif
		
		int thread_errno = errno;
		if (thread_errno == EINTR) {
			goto start_accept;
		}
		if ((thread_errno == EAGAIN) || (thread_errno == EWOULDBLOCK) || (on_accept_error() == 0)){
			CO_AGAIN;
		}
		CO_SWITCH; //We never finish until we face real error, but we switch the control to on_accept_error.
	CO_END
}

cort_proto* cort_tcp_listener::on_accept_error(){
	CO_BEGIN
		int thread_errno = errno;
		if(thread_errno != EMFILE && thread_errno != ENFILE && thread_errno != ENOBUFS && thread_errno != ENOMEM ){
			return 0;
		}
		pause_accept();
		CO_SLEEP(1000);
		resume_accept();
		start(); 
		CO_SWITCH;	//We never finish until we face real error, but we switch the control back to start.
	CO_END
}

static void remove_keep_alive(cort_tcp_server_waiter* tcp_cort){
	int result = tcp_cort->get_poll_result();
	if(result == 0 || (((EPOLLHUP|EPOLLRDHUP|EPOLLERR) & result) != 0)){//timeout
		tcp_cort->close_cort_fd();
		tcp_cort->release();
	}
	else{
		tcp_cort->clear();	
		tcp_cort->remove_ref();
		tcp_cort->ctrler_creator(tcp_cort->get_cort_fd(), tcp_cort->ip_v4, tcp_cort->port_v4, tcp_cort);
	}
}

static cort_proto* on_connection_keepalive_timeout_or_readable_server(cort_proto* arg){
	cort_tcp_server_waiter* tcp_cort = (cort_tcp_server_waiter*)arg;
	remove_keep_alive(tcp_cort);
	return 0;
}

void cort_tcp_server_waiter::keep_alive(uint32_t keep_alive_time, uint32_t /* ip_arg */, uint16_t /* port_arg */, uint16_t /* type_key_arg */){
	this->set_parent(0);
	this->set_timeout(keep_alive_time);
	this->set_run_function(on_connection_keepalive_timeout_or_readable_server);
	this->add_ref();
	this->set_poll_request(EPOLLIN | EPOLLRDHUP);
	this->clear_poll_result();
}
