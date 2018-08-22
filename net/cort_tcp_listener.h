#ifndef CORT_TCP_CONNECT_LISTEN_ACCEPT_H_
#define CORT_TCP_CONNECT_LISTEN_ACCEPT_H_

#include "cort_tcp_ctrler.h"
#include <arpa/inet.h>
#include <netinet/tcp.h>

struct cort_accept_result_t{
    int accept_fd ;
    struct sockaddr_in servaddr;
};
template<typename connection_t, typename connection_waiter_t>
struct tcp_ctrler_static_creator{
	typedef tcp_ctrler_static_creator<connection_t, connection_waiter_t> this_type;
	static void create(int fd, int dest_ip, int dest_port, cort_tcp_connection_waiter* waiter, uint32_t init_poll_result){
		connection_t* result = new connection_t();
		if(waiter == 0) {
			waiter = new connection_waiter_t(fd);
		}
        waiter->set_poll_result(init_poll_result);
		result->set_connection_waiter(waiter);
		result->set_dest_addr(dest_ip, dest_port);
		((connection_waiter_t*)waiter)->ctrler_creator = this_type::create;
		result->cort_start();
	}
};

struct cort_tcp_listener : public cort_fd_waiter{
public:
	CO_DECL(cort_tcp_listener)
	
	cort_tcp_listener();
	~cort_tcp_listener();
	
	void set_backlog(int backlog_arg){
		backlog = backlog_arg;
	}

	void set_listen_port(uint16_t listen_port_arg){
		listen_port = listen_port_arg;
	}
	
	uint8_t get_errno() const {
		return errnum;
	}

	void set_errno(uint8_t err_number){
		errnum = err_number;
	}

	template<typename accept_cort_type, typename connection_waiter_t>
	void set_ctrler_creator(){
		ctrler_creator = tcp_ctrler_static_creator<accept_cort_type, connection_waiter_t>::create;
	}
	void set_ctrler_creator(void (*ctrler_creator_arg)(int accept_fd, int dest_ip, int dest_port, cort_tcp_connection_waiter* waiter, uint32_t init_poll_result)){
		ctrler_creator = ctrler_creator_arg;
	}
	void pause_accept();

	void resume_accept();

	void stop_listen();
	
	uint8_t listen_connect();
	
	cort_proto* start();
	
	virtual void on_accept(int accept_fd){} ;
private:
	void set_timeout(time_ms_t timeout_ms); //We disable user set_timeout. The cort should be never finish unless you call stop_listen or destruct it.
	void (*ctrler_creator)(int accept_fd, int dest_ip, int dest_port, cort_tcp_connection_waiter* waiter, uint32_t init_poll_result);
	int backlog;
	uint16_t listen_port;
	uint8_t errnum;
	union{
		struct{
			uint8_t disable_no_delay:1;
			uint8_t enable_close_by_reset:1;
			uint8_t disable_reuse_address:1 ;
			uint8_t enable_accept_after_recv:1;
		}_;
		uint8_t data;
	}setsockopt_arg;
public:
	void set_disable_no_delay(uint8_t value = 1){
		setsockopt_arg._.disable_no_delay = value;
	}
	
	void set_enable_close_by_reset(uint8_t value = 1){
		setsockopt_arg._.enable_close_by_reset = value;
	}
	
	void set_disable_reuse_address(uint8_t value = 1){
		setsockopt_arg._.disable_reuse_address = value;
	}
	void set_enable_accept_after_recv(uint8_t value = 1){
		setsockopt_arg._.enable_accept_after_recv = value;
	}
};

struct cort_tcp_server_waiter : public cort_tcp_connection_waiter{
	cort_tcp_server_waiter(int fd){
		set_cort_fd(fd);
	}
	void (*ctrler_creator)(int accept_fd, int dest_ip, int dest_port, cort_tcp_connection_waiter* waiter, uint32_t init_poll_result);
	virtual void keep_alive(uint32_t keep_alive_time, uint32_t ip_arg, uint16_t port_arg, uint16_t type_key_arg);
};

#endif
