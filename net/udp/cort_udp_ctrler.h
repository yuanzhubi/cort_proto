#ifndef CORT_UDP_CONNECT_SEND_RECV_H_
#define CORT_UDP_CONNECT_SEND_RECV_H_

#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <sys/uio.h>
#include "../cort_timeout_waiter.h"
#include "cort_tcp_ctrler.h"

struct cort_udp_connection_waiter;
struct cort_udp_connection_waiter_client;
struct cort_udp_ctrler;
struct cort_udp_connect_send_recv_data;

struct cort_udp_connection_waiter : public cort_fd_waiter{
	uint32_t 	ip_v4;
	uint16_t 	port_v4;
	uint16_t 	type_key; 
	
	typedef cort_fd_waiter parent_type;
	CO_DECL_PROTO(cort_udp_connection_waiter)

	//We assert the parent cort should only await this cort, and do not need other time information.
	void on_finish();
	
	//You can await this function.
	cort_proto* try_connect();
	
	//You can await this function.
	cort_proto* try_send();
	
	//You can await this function.
	cort_proto* try_recv();
	
	bool is_connected() const {
		return get_cort_fd() > 0;
	}
	
	int get_connected_fd() const {
		return get_cort_fd();
	}
	
	void set_errno(uint8_t err);
	
	void close_connection(uint8_t err = 0);
	
	virtual void keep_alive(uint32_t keep_alive_time, uint32_t ip_arg, uint16_t port_arg, uint16_t type_key_arg) = 0;
};

struct cort_udp_ctrler : public cort_fd_waiter{
	CO_DECL_PROTO(cort_udp_ctrler)
public:	
	void clear();
	virtual ~cort_udp_ctrler();
	cort_udp_ctrler();
private:
	typedef cort_proto parent_type;

public:	
//Common API
	
	//Both ip and port have to use network byte order!
	void set_dest_addr(uint32_t ip, uint16_t port);
	
	//Port should use local order!
	void set_dest_addr(const char* ip, uint16_t port);
	
	uint8_t get_errno() const {
		return errnum;
	}
	void set_errno(uint8_t err){
		errnum = err;
	}
	
	void set_timeout(uint32_t timeout_arg){
		timeout = timeout_arg;
	}
	
	//type_key:ip:port will form the search key for the keep alive connection. So it can be your sub_class type_id or other application key.
	void set_type_key(uint16_t type_key_arg){
		type_key = type_key_arg;
	}

//Connect API
    
	//You can await this function.
	cort_proto* try_connect();

	void set_keep_alive(uint32_t alive_ms){
		keep_alive_ms = alive_ms;
	}
	
	static cort_udp_connection_waiter_client* get_keep_alive_connection_client(uint32_t ip_arg, uint16_t port_arg, uint16_t type_key_arg = 0);
	
//Send API
public:
	//You can await this function.
	cort_proto* try_send();
	
	
	bool is_send_finished() const{
		return send_buffer.empty();
	}
	
	//weak reference, return zero if too much data are waiting in the sending queue.
	char* set_send_buffer(char* src_buffer, int32_t arg_size){
		return send_buffer.set_send_buffer(src_buffer, arg_size);
	}

	//strong reference, return zero if too much data are waiting in the sending queue.
	char* alloc_send_buffer(int32_t arg_size){
		return send_buffer.alloc_send_buffer(arg_size);
	}
	
	//strong reference, return zero if too much data to be sended
	char* copy_send_buffer(char* src_buffer, int32_t arg_size){
		return send_buffer.copy_send_buffer(src_buffer, arg_size);
	}

//Recv API
public:
	//You can await this function.
	cort_proto* try_recv();
	
	char* get_recv_buffer() const{
		return recv_buffer.recv_buffer;
	}
	
	recv_buffer_ctrl::recv_buffer_size_t  get_recv_buffer_size() const {
		return recv_buffer.recved_size;
	}
	
	void set_recv_check_function(recv_buffer_ctrl::recv_buffer_size_t (*recv_check_function)(recv_buffer_ctrl*, cort_udp_ctrler*) ){
		recv_buffer.set_recv_check_function(recv_check_function);
	}
	
	//Strong reference
	char* alloc_recv_buffer(recv_buffer_ctrl::recv_buffer_size_t init_size = recv_buffer_ctrl::default_init_recv_buffer_size){
		return recv_buffer.realloc_recv_buffer(init_size);
	}
	
	//Weak reference
	char* set_recv_buffer(char* buffer, recv_buffer_ctrl::recv_buffer_size_t buffer_size){
		return recv_buffer.set_recv_buffer(buffer, buffer_size);
	}
	
//Operation
public:
	//Use cort_udp_connection_waiter, for example.
	template<typename connection_waiter_t>
	void set_connection_waiter(connection_waiter_t* arg){
		connection_waiter = arg;
	}
	
	//We user space codes just have the "weak reference" of connection.
	//Use lock_waiter to get a strong reference of the waiter of the connection.
	cort_udp_connection_waiter* lock_waiter();
	
	struct cort_udp_connection_waiter_for_connect : public cort_udp_connection_waiter{
		CO_DECL(cort_udp_connection_waiter_for_connect, try_connect)
	};
	
	//This is not const. Use it only you want to await the waiter!
	cort_udp_connection_waiter_for_connect* lock_connect(){
		return (cort_udp_connection_waiter_for_connect*)lock_waiter();
	}
	
	struct cort_udp_connection_waiter_for_send : public cort_udp_connection_waiter{
		CO_DECL(cort_udp_connection_waiter_for_send, try_send)
		cort_proto* start(){
			return this->try_send();
		}
	};
	
	//This is not const. Use it only you want to await the waiter!
	cort_udp_connection_waiter_for_send* lock_send(){
		return (cort_udp_connection_waiter_for_send*)lock_waiter();
	}
	
	struct cort_udp_connection_waiter_for_recv : public cort_udp_connection_waiter{
		CO_DECL(cort_udp_connection_waiter_for_recv, try_recv)
	};
	
	//This is not const. Use it only you want to await the waiter!
	cort_udp_connection_waiter_for_recv* lock_recv(){
		return (cort_udp_connection_waiter_for_recv*)lock_waiter();
	}
	
	cort_shared_ptr<cort_udp_connection_waiter> connection_waiter;

//Send & Receive
	recv_buffer_ctrl 	recv_buffer;
	send_buffer_ctrl    send_buffer;
	
//Connection
	uint32_t    timeout;
	uint32_t    timecost;
	uint32_t 	keep_alive_ms;
	uint32_t 	ip_v4;
	uint16_t 	port_v4;
	uint16_t 	type_key; 
	
//Rest
	uint8_t 	errnum;
	union{
		struct{
			uint8_t disable_no_delay:1;
			uint8_t enable_close_by_reset:1;
			uint8_t enable_reuse_address:1;
		}_;
		uint8_t data;
	}setsockopt_arg;
	
//Connection option.	
public:	
	//void set_disable_no_delay(uint8_t value = 1){
	//	setsockopt_arg._.disable_no_delay = value;
	//}
	
	//void set_enable_close_by_reset(uint8_t value = 1){
	//	setsockopt_arg._.enable_close_by_reset = value;
	//}
	
	//void set_enable_reuse_address(uint8_t value = 1){
	//	setsockopt_arg._.enable_reuse_address = value;
	//}
	
	void refresh_socket_option();
	
//Time
public:
	uint32_t get_time_cost() const {
		return timecost;
	}
	
	void init_time_cost(){
		timecost = (uint32_t)cort_timer_now_ms();
	}
	
	void finish_time_cost(){
		timecost = (uint32_t)cort_timer_now_ms() - timecost;
	}
	
protected:
	void on_finish();
	virtual void on_connect_finish(){};
	
	virtual void on_send_finish(){};
	
	virtual void on_recv_finish(){};
	
	//You should call it after your couroutine on_finish because we may set_timeout for keepalive.
	virtual void on_connection_inactive();
};

//We provide a usual send request then receive response finally stored in the connection pool rpc model. 
struct cort_udp_request_response : public cort_udp_ctrler{
	CO_DECL(cort_udp_request_response)
	
	//We provide a default action : connect, send data then receive data.
	//This is the ususal case in RPC for client side.
	//You should set_recv_buffer_ctrl to inform when the receive is finished.
	cort_proto* start();
protected:
	void on_finish();
};

struct cort_udp_connection_waiter_client : public cort_udp_connection_waiter{
	size_t waiter_pos;
	void keep_alive(uint32_t keep_alive_time, uint32_t ip_arg, uint16_t port_arg, uint16_t type_key_arg);
	static size_t clear_keep_alive_connection(size_t count, uint32_t ip_arg = 0, uint16_t port_arg = 0, uint16_t type_key_arg = 0);
};

#endif