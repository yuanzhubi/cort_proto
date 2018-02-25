#ifndef CORT_TCP_CONNECT_SEND_RECV_H_
#define CORT_TCP_CONNECT_SEND_RECV_H_

#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <sys/uio.h>
#include "../cort_timeout_waiter.h"

struct cort_tcp_connection_waiter;
struct cort_tcp_connection_waiter_client;
struct cort_tcp_ctrler;
struct cort_tcp_connect_send_recv_data;

namespace cort_socket_config{	//When the following config is changed, you have to compile again!
	const static size_t SOCKET_KEEPALIVE_AUTO_RELEASE_COUNT = 24;
	const static size_t SOCKET_RECV_BUFFER_DEFAULT_SIZE = 4096-64;
};

namespace cort_socket_error_codes{
	template<int N, uint8_t code>
	struct error_str;
	template<int N>
	struct error_str<N, 0>{
		const char* info[256];
	};
	template<int N, uint8_t code>
	struct error_str : public error_str<N, code - 1>{ 
	};
	
#define CO_DECL_CODES(error_name, error_code) \
	const static uint8_t error_name = error_code; \
	template<int N> \
	struct error_str<N, error_code> : public error_str<N, error_code - 1>{ \
		error_str(){ \
			error_str<N, 0>::info[error_code] = #error_name; \
		} \
	}
	const char* error_info(uint8_t code);
	
	//1: socket create error
	CO_DECL_CODES(SOCKET_CREATE_ERROR, 1);
	
	//2: socket connect error
	CO_DECL_CODES(SOCKET_CONNECT_ERROR, 2);

	//3: socket operation timeout or stopped
	CO_DECL_CODES(SOCKET_OPERATION_TIMEOUT, 3);
	
	//4: socket connect rejected
	CO_DECL_CODES(SOCKET_CONNECT_REJECTED, 4);
	
	//5: remote cancled the operation, including:
	//remote close or reset the connection;
	//remote send data when you do not want to receive.
	CO_DECL_CODES(SOCKET_REMOTE_CANCELED, 5);
	
	//6: send data failed. Maybe some system error. 
	CO_DECL_CODES(SOCKET_SEND_ERROR, 6);

	//7: realloc failed, maybe too much data to be received.
	CO_DECL_CODES(SOCKET_ALLOC_MEMORY_ERROR, 7);

	//8: recv_check failed, maybe received bad format data.
	CO_DECL_CODES(SOCKET_RECEIVED_CHECK_ERROR, 8);
	
	//9: recv data failed. Maybe some system error.
	CO_DECL_CODES(SOCKET_RECEIVE_ERROR, 9);
	
	//50: bind error
	CO_DECL_CODES(SOCKET_BIND_ERROR, 50);
	
	//80: state error. Maybe receive data when you do not need.
	CO_DECL_CODES(SOCKET_STATE_ERROR, 80);
	
	//100: poll error.
	CO_DECL_CODES(SOCKET_POLL_ERROR, 100);
	
	CO_DECL_CODES(SOCKET_LISTEN_ERROR, 150);
	
	CO_DECL_CODES(SOCKET_ACCEPT_ERROR, 180);

	CO_DECL_CODES(SOCKET_INVALID_CONNECT_ADDRESS, 254);
	CO_DECL_CODES(SOCKET_INVALID_LISTEN_ADDRESS, 255);	
};

struct send_buffer_ctrl{
	typedef uint32_t size_type;
	const static uint8_t max_send_queue_size = 3; //sizeof = 3*(16+4)+4
	const static uint8_t send_npos = max_send_queue_size;
	iovec send_data[max_send_queue_size];
	size_type send_data_tag[max_send_queue_size]; 

	// capacity_size must be power of 2
	send_buffer_ctrl(){	
		send_data[0].iov_base = 0;
	}

	~send_buffer_ctrl(){
		clear();
	}
	
	void clear(){
		for(int i = 0; i<max_send_queue_size; ++i){
			if(send_data[i].iov_base == 0){
				break;
			}
			int32_t back_result = ((int32_t*)send_data_tag)[i];
			if(back_result >= 0){
				free((char*)(send_data[i].iov_base) - send_data_tag[i]);
			}
		}
		send_data[0].iov_base = 0;
	}
	
	size_t get_free_index() const{
		size_t i = 0;
		for(; i < max_send_queue_size;++i){
			if(send_data[i].iov_base == 0){
				return i;
			}
		}
		return i;
	}
	
	//weak reference
	char* set_send_buffer(char* src_buffer, int32_t arg_size){
		size_t index = get_free_index();
		if(index == max_send_queue_size){
			return 0;
		}
		send_data[index].iov_base = src_buffer;
		send_data[index].iov_len = arg_size;
		send_data_tag[index] = 1<<31;
		if(index != max_send_queue_size - 1){
			send_data[index+1].iov_base = 0;
		}
		return src_buffer;
	}

	//strong reference
	char* alloc_send_buffer(int32_t arg_size){
		size_t index = get_free_index();
		if(index == max_send_queue_size){
			return 0;
		}
		char* result = (char*)malloc(arg_size);
		send_data[index].iov_base = result;
		send_data[index].iov_len = arg_size;
		send_data_tag[index] = 0;
		if(index != max_send_queue_size - 1){
			send_data[index+1].iov_base = 0;
		}
		return result;
	}
	
	//strong reference
	char* copy_send_buffer(char* src_buffer, int32_t arg_size){
		size_t index = get_free_index();
		if(index == max_send_queue_size){
			return 0;
		}
		char* result = (char*)(send_data[index].iov_base = (char*)malloc(arg_size));
		if(result != 0){
			(char*)memcpy(result, src_buffer, arg_size);
		}
		send_data[index].iov_len = arg_size;
		send_data_tag[index] = 0;
		if(index != max_send_queue_size - 1){
			send_data[index+1].iov_base = 0;
		}
		return result;
	}
	
	inline bool empty() const{return (send_data[0].iov_base == 0);}
};

struct recv_buffer_ctrl{
	typedef int32_t recv_buffer_size_t;
	char* recv_buffer;	
	recv_buffer_size_t recv_buffer_size;
	recv_buffer_size_t recved_size;
	union{
		struct{
			char recv_check_further_needed;			//inner status, do not set
			char recv_finished_shrink_needed;		//recv_finished_shrink_needed means after recv finished, whether we realloc the recv buffer to fit the actual need.
			char is_weak_reference;
		}_;
		size_t result_int;
	}data0;
	
	static recv_buffer_size_t recv_check_packet(recv_buffer_ctrl* arg, cort_tcp_ctrler*){
		return arg->recved_size;
	}
	
	const static recv_buffer_size_t unexpected_data_received = recv_buffer_size_t(1<<31);
	
	//recv_check is used to check whether the receive is finished.
	//return 0: more bytes data to be received, but we do not know how much. If recv_buffer is not enough, we will realloc_recv_buffer(2*m_recv_buffer_size)
	//return positive number x: x bytes data to be received in total. If recv_buffer is no enough, we will realloc_recv_buffer(x)
	//return recv_buffer_size: receive data finished. More data received will lead connection close!
	//return 1<<31: check failed. Unexpected data received, or you just want to stop further receiving.
	//return negative number y: probably, -y bytes data to be received in total(unlike to positive number x, y is just a hint for buffer allocation).
	//You can call realloc_recv_buffer manually in this function.
	//Using function pointer instead of virtual function enable us reuse the struct for the same ip:port.
	//You can subclass the recv_buffer_ctrl to add members if recv_check has to update the state of arg.
	//recv_check is initialized to recv_check_packet as defualt;
	recv_buffer_size_t (*recv_check)(recv_buffer_ctrl*, cort_tcp_ctrler*);
	
	void set_recv_check_function(recv_buffer_size_t (*recv_check_arg)(recv_buffer_ctrl*, cort_tcp_ctrler*)){
		recv_check = recv_check_arg;
	}
	
	//When recv_buffer_size is not enough, the recevier will use realloc for more size.
	//return new allocated memory address,
	//return 0 if realloc failed, or previous buffer is weak reference that can not be enlarged.
	//For example, you can limit the new_buffer_size, or ignore the new_buffer_size when recv_buffer == 0 for a smaller initial buffer size.
	const static recv_buffer_size_t default_init_recv_buffer_size = cort_socket_config::SOCKET_RECV_BUFFER_DEFAULT_SIZE;
	
	//strong reference
	char* realloc_recv_buffer(recv_buffer_size_t new_buffer_size = default_init_recv_buffer_size){
		if(recv_buffer == 0 || data0._.is_weak_reference == 1){
			recv_buffer = (char*)malloc(new_buffer_size);
			data0._.is_weak_reference = 0;	
		}else if((new_buffer_size > recv_buffer_size) || (data0._.recv_finished_shrink_needed == 1)){
			recv_buffer = (char*)realloc(recv_buffer, new_buffer_size);
		}
		recv_buffer_size = new_buffer_size; 
		return recv_buffer;
	}
	
	//weak reference
	char* set_recv_buffer(char* buf, recv_buffer_size_t buf_size){
		if(data0._.is_weak_reference == 0 && recv_buffer != 0){
			free(recv_buffer);
		}		
		recv_buffer = buf;
		recv_buffer_size = buf_size;
		recved_size = 0;
		data0.result_int = 0;
		data0._.is_weak_reference = 1;
		return recv_buffer; 
	}
	
	void shrink_to_fit(){
		if(recv_buffer != 0 && recv_buffer_size > recved_size && data0._.is_weak_reference != 0){
			recv_buffer = (char*)realloc(recv_buffer, recved_size);
			recv_buffer_size = recved_size;
		}
	}
	
	virtual ~recv_buffer_ctrl(){
		if(recv_buffer != 0 && data0._.is_weak_reference == 0 ){
			free(recv_buffer);
		}
	}

	recv_buffer_ctrl(){
		recv_buffer = 0;
		recv_buffer_size = 0;
		recved_size = 0;
		data0.result_int = 0;
		recv_check = &recv_check_packet;
	}

	void clear(){
		recved_size = 0;
		data0.result_int = 0;
	}
private:
	recv_buffer_ctrl(const recv_buffer_ctrl&);
};

struct cort_tcp_connection_waiter : public cort_fd_waiter{
	uint32_t 	ip_v4;
	uint16_t 	port_v4;
	uint16_t 	type_key; 
	
	typedef cort_fd_waiter parent_type;
	CO_DECL_PROTO(cort_tcp_connection_waiter)

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

struct cort_tcp_ctrler : public cort_proto{
	CO_DECL_PROTO(cort_tcp_ctrler)
public:	
	void clear();
	virtual ~cort_tcp_ctrler();
	cort_tcp_ctrler();
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
	
	static cort_tcp_connection_waiter_client* get_keep_alive_connection_client(uint32_t ip_arg, uint16_t port_arg, uint16_t type_key_arg = 0);
	
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
	
	void set_recv_check_function(recv_buffer_ctrl::recv_buffer_size_t (*recv_check_function)(recv_buffer_ctrl*, cort_tcp_ctrler*) ){
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
	//Use cort_tcp_connection_waiter, for example.
	template<typename connection_waiter_t>
	void set_connection_waiter(connection_waiter_t* arg){
		connection_waiter = arg;
	}
	
	//We user space codes just have the "weak reference" of connection.
	//Use lock_waiter to get a strong reference of the waiter of the connection.
	cort_tcp_connection_waiter* lock_waiter();
	
	struct cort_tcp_connection_waiter_for_connect : public cort_tcp_connection_waiter{
		CO_DECL(cort_tcp_connection_waiter_for_connect, try_connect)
	};
	
	//This is not const. Use it only you want to await the waiter!
	cort_tcp_connection_waiter_for_connect* lock_connect(){
		return (cort_tcp_connection_waiter_for_connect*)lock_waiter();
	}
	
	struct cort_tcp_connection_waiter_for_send : public cort_tcp_connection_waiter{
		CO_DECL(cort_tcp_connection_waiter_for_send, try_send)
		cort_proto* start(){
			return this->try_send();
		}
	};
	
	//This is not const. Use it only you want to await the waiter!
	cort_tcp_connection_waiter_for_send* lock_send(){
		return (cort_tcp_connection_waiter_for_send*)lock_waiter();
	}
	
	struct cort_tcp_connection_waiter_for_recv : public cort_tcp_connection_waiter{
		CO_DECL(cort_tcp_connection_waiter_for_recv, try_recv)
	};
	
	//This is not const. Use it only you want to await the waiter!
	cort_tcp_connection_waiter_for_recv* lock_recv(){
		return (cort_tcp_connection_waiter_for_recv*)lock_waiter();
	}
	
	cort_shared_ptr<cort_tcp_connection_waiter> connection_waiter;

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
	void set_disable_no_delay(uint8_t value = 1){
		setsockopt_arg._.disable_no_delay = value;
	}
	
	void set_enable_close_by_reset(uint8_t value = 1){
		setsockopt_arg._.enable_close_by_reset = value;
	}
	
	void set_enable_reuse_address(uint8_t value = 1){
		setsockopt_arg._.enable_reuse_address = value;
	}
	
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
struct cort_tcp_request_response : public cort_tcp_ctrler{
	CO_DECL(cort_tcp_request_response)
	
	//We provide a default action : connect, send data then receive data.
	//This is the ususal case in RPC for client side.
	//You should set_recv_buffer_ctrl to inform when the receive is finished.
	cort_proto* start();
protected:
	void on_finish();
};

struct cort_tcp_connection_waiter_client : public cort_tcp_connection_waiter{
	size_t waiter_pos;
	void keep_alive(uint32_t keep_alive_time, uint32_t ip_arg, uint16_t port_arg, uint16_t type_key_arg);
	static size_t clear_keep_alive_connection(size_t count, uint32_t ip_arg = 0, uint16_t port_arg = 0, uint16_t type_key_arg = 0);
};

#endif