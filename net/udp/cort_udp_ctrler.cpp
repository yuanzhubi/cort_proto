#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <map>
#include <vector>

#include "cort_udp_ctrler.h"

cort_udp_ctrler::cort_udp_ctrler(){
//Connection	
	timeout = 0;
	timecost = 0;
	keep_alive_ms = 0;
	ip_v4 = 0;
	port_v4 = 0;
	type_key = 0;
	
	errnum = 0;
}

cort_udp_ctrler::~cort_udp_ctrler(){
}

void cort_udp_ctrler::clear(){
	parent_type::clear();
	recv_buffer.clear();
	send_buffer.clear();
	errnum = 0;
}

void cort_udp_ctrler::set_dest_addr(uint32_t ip, uint16_t port){
	ip_v4 = ip;
	port_v4 = port;
}

void cort_udp_ctrler::set_dest_addr(const char* ip, uint16_t port){
	uint32_t ip_int ;
	inet_pton(AF_INET, ip, &ip_int);
	set_dest_addr(ip_int, htons(port));
}

void cort_udp_ctrler::refresh_socket_option(){
	cort_udp_connection_waiter* result = this->connection_waiter.get_ptr();
	if(result != 0){
		int fd = result->get_cort_fd();
		if(fd > 0){
			if(setsockopt_arg._.enable_reuse_address != 0){
				int cort_yes = 1;
				setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &cort_yes, sizeof(cort_yes));
			}
		}	
	}
}

struct ip_v4_key{
	union{
		struct{
			uint32_t ip_v4;
			uint16_t port_v4;
			uint16_t type_key;
		}s_data;
		uint64_t i_data;
	}data;
};

//Here cort_udp_connection_waiter_client* element in the vector should be its only strong reference!
static __thread std::map<uint64_t, std::vector<cort_udp_connection_waiter_client* > >* connection_udp_pool = 0;
static void remove_keep_alive(cort_udp_connection_waiter_client* udp_cort){
	ip_v4_key key;
	key.data.s_data.ip_v4 = udp_cort->ip_v4;
	key.data.s_data.port_v4 = udp_cort->port_v4;
	key.data.s_data.type_key = udp_cort->type_key;
	std::map<uint64_t, std::vector<cort_udp_connection_waiter_client* > >::iterator it = connection_udp_pool->find(key.data.i_data);
	if(it != connection_udp_pool->end()){
		std::vector<cort_udp_connection_waiter_client* >& dq = it->second;
		cort_udp_connection_waiter_client* last = dq.back();
		last->waiter_pos = udp_cort->waiter_pos;
		dq[udp_cort->waiter_pos] = last;
		dq.pop_back();
		
		udp_cort->clear();
		udp_cort->close_cort_fd();
		udp_cort->release();
		if(dq.empty()){
			if(connection_udp_pool->size() == 1){
				delete connection_udp_pool;
				connection_udp_pool = 0;
			}else{
				connection_udp_pool->erase(it);
			}
		}
	}
}

static cort_proto* on_connection_keepalive_timeout_or_readable(cort_proto* arg){
	cort_udp_connection_waiter_client* udp_cort = (cort_udp_connection_waiter_client*)arg;
	remove_keep_alive(udp_cort);
	return 0;
}

static cort_proto* stop_poll_when_notification(cort_proto* arg){
	cort_udp_connection_waiter* udp_cort = (cort_udp_connection_waiter*)arg;
	if(udp_cort->get_parent() != 0  //This error happened after the connection finished some jobs before its parent knows. So do not pass error codes to parent.
		|| udp_cort->release() != 0){ //Or it is referenced by others. Usually, it should be only referenced by its parent!
		if(udp_cort->is_timeout_or_stopped()){
			udp_cort->close_connection();
			return udp_cort;
		}
		udp_cort->clear_timeout();
		udp_cort->remove_poll_request();
	}
	return udp_cort;
}

cort_proto* cort_udp_connection_waiter::on_finish(){
	clear_poll_result();
	set_run_function(stop_poll_when_notification);
	if(get_parent() == 0){
		this->release();
	}		
	else{
		uint32_t poll_req = get_poll_request();
		if( (EPOLLOUT & poll_req) != 0){ //Sendable should not be polled in default.
			set_poll_request((~EPOLLOUT) & poll_req);
		}
	}
    //You are not really finished! So do not call on_finish function of your super class to avoid clear_timeout and clear run_function.
    //return cort_fd_waiter::on_finish();
    return 0;
}

void cort_udp_connection_waiter_client::keep_alive(uint32_t keep_alive_ms, uint32_t ip_v4, uint16_t port_v4, uint16_t type_key){
	ip_v4_key key;
	key.data.s_data.ip_v4 = ip_v4;
	key.data.s_data.port_v4 = port_v4;
	key.data.s_data.type_key = type_key;
	
	this->ip_v4 = ip_v4;
	this->port_v4 = port_v4;
	this->type_key = type_key;
	this->add_ref();
	this->set_timeout(keep_alive_ms);
	this->set_run_function(on_connection_keepalive_timeout_or_readable);
	this->set_parent(0);
	this->set_poll_request(EPOLLIN | EPOLLRDHUP);
	if(connection_udp_pool == 0){
		connection_udp_pool = new std::map<uint64_t, std::vector<cort_udp_connection_waiter_client* > >();
	}
	std::vector<cort_udp_connection_waiter_client* > & container = (*connection_udp_pool)[key.data.i_data];
	this->waiter_pos = container.size();
	container.push_back(this);
}

cort_udp_connection_waiter_client* cort_udp_ctrler::get_keep_alive_connection_client(uint32_t ip_arg, uint16_t port_arg, uint16_t type_key_arg){
	if(connection_udp_pool == 0){
		return 0;
	}
	ip_v4_key key;
	key.data.s_data.ip_v4 = ip_arg;
	key.data.s_data.port_v4 = port_arg;
	key.data.s_data.type_key = type_key_arg;
	std::map<uint64_t, std::vector<cort_udp_connection_waiter_client* > >::iterator it = connection_udp_pool->find(key.data.i_data);
	cort_udp_connection_waiter_client* p = 0;
	if(it != connection_udp_pool->end()){
		std::vector<cort_udp_connection_waiter_client* >& dq = it->second;
		p = dq.back();
		p->clear();
		p->remove_ref();
		dq.pop_back();
		if(dq.empty()){
			if(connection_udp_pool->size() == 1){
				delete connection_udp_pool;
				connection_udp_pool = 0;
			}
			else{
				connection_udp_pool->erase(it);
			}
		}	
	}
	return p;
}

void cort_udp_connection_waiter::set_errno(uint8_t err){
	cort_udp_ctrler* parent_waiter = (cort_udp_ctrler*)get_parent();
	parent_waiter->set_errno(err);
	this->clear_timeout();
}

void cort_udp_connection_waiter::close_connection(uint8_t err){
	close_cort_fd();
	set_errno(err);
}

const static uint32_t connec_poll_request = EPOLLOUT | EPOLLIN | EPOLLRDHUP;
cort_proto* cort_udp_connection_waiter::try_connect(){
	CO_BEGIN
		if(is_connected()){
			CO_RETURN;
		}
		cort_udp_ctrler* parent_waiter = (cort_udp_ctrler*)get_parent();
		if(parent_waiter->ip_v4 == 0 || parent_waiter->port_v4 == 0){
			set_errno(cort_socket_error_codes::SOCKET_INVALID_CONNECT_ADDRESS);
			CO_RETURN;
		}
		if(parent_waiter->timeout != 0){
			this->set_timeout(parent_waiter->timeout);
			parent_waiter->timeout = 0;
		}
		int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if(sockfd == -1){
			set_errno(cort_socket_error_codes::SOCKET_CREATE_ERROR);
			CO_RETURN;
		}
		struct sockaddr_in servaddr;
		bzero(&servaddr,sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = (parent_waiter->port_v4);
		servaddr.sin_addr.s_addr = (parent_waiter->ip_v4);
		
		int flag = fcntl(sockfd, F_GETFL);
		if (-1 == flag || fcntl(sockfd, F_SETFL, flag | O_NONBLOCK) == -1){
			close(sockfd);
			set_errno(cort_socket_error_codes::SOCKET_CREATE_ERROR);
			CO_RETURN;
		}
		int status;
	connect_again:
		status = connect(sockfd,  (struct sockaddr*)(&servaddr), sizeof(sockaddr_in));
		if(status != 0){
			int thread_errno = errno;
			if(thread_errno == EISCONN){
				set_cort_fd(sockfd);
				CO_RETURN;
			}
			else if(thread_errno != EINPROGRESS){
				if(thread_errno == EINTR){
					goto connect_again;
				}
				else if(thread_errno == EADDRNOTAVAIL){
					cort_udp_connection_waiter_client::clear_keep_alive_connection(cort_socket_config::SOCKET_KEEPALIVE_AUTO_RELEASE_COUNT, 
						parent_waiter->ip_v4, parent_waiter->port_v4, parent_waiter->type_key
					);
				}
				close(sockfd);
				set_errno(cort_socket_error_codes::SOCKET_CONNECT_ERROR);
				CO_RETURN;
			}
		}
		else{
			set_cort_fd(sockfd);
			parent_waiter->refresh_socket_option();
			CO_RETURN;
		}
		set_cort_fd(sockfd);
		set_poll_request(connec_poll_request);
		CO_YIELD();
		if(is_timeout_or_stopped()){
			close_connection(cort_socket_error_codes::SOCKET_OPERATION_TIMEOUT);
			CO_RETURN;	
		}
		uint32_t poll_event = get_poll_result();
		if( ((EPOLLHUP|EPOLLRDHUP|EPOLLERR) & poll_event) != 0){
			close_connection(cort_socket_error_codes::SOCKET_CONNECT_REJECTED);
			CO_RETURN;	
		}
		if((EPOLLOUT & poll_event) == 0){
			close_connection(cort_socket_error_codes::SOCKET_CONNECT_ERROR);
			CO_RETURN;
		}
		((cort_udp_ctrler*)get_parent())->refresh_socket_option();
	CO_END
}

const static uint32_t send_poll_request = EPOLLOUT | EPOLLIN;
cort_proto* cort_udp_connection_waiter::try_send(){
	CO_BEGIN
		cort_udp_ctrler* parent_waiter = (cort_udp_ctrler*)get_parent();
		if(!is_connected()){
			set_errno(cort_socket_error_codes::SOCKET_STATE_ERROR);
			CO_RETURN;
		}
		if(parent_waiter->timeout != 0){
			this->set_timeout(parent_waiter->timeout);
			parent_waiter->timeout = 0;
		}
		send_buffer_ctrl& ctrler = parent_waiter->send_buffer;
        do{
		size_t send_last_index = ctrler.get_free_index();
		bool send_finished = true;
		if(send_last_index != 0){			
			int fd = get_connected_fd();
			ssize_t current_sended_size;
		send_label:
			if(send_last_index == 1){
                struct sockaddr_in servaddr;
                bzero(&servaddr,sizeof(servaddr));
                servaddr.sin_family = AF_INET;
                servaddr.sin_port = (parent_waiter->port_v4);
                servaddr.sin_addr.s_addr = (parent_waiter->ip_v4);
				current_sended_size = sendto(fd, ctrler.send_data[0].iov_base, ctrler.send_data[0].iov_len, 0, (sockaddr*)(&servaddr), sizeof(servaddr));
			}
			else{
				current_sended_size = writev(fd, ctrler.send_data, send_last_index);
			}
			
			co_unlikely_if(current_sended_size < 0){
				int thread_errno = errno;
				if(thread_errno == EINTR) {
					goto send_label;
				}
				close_connection(cort_socket_error_codes::SOCKET_SEND_ERROR);
				CO_RETURN;
			}
            for(size_t i = 0; i < send_last_index ; ++i){
				if(current_sended_size >= (ssize_t)ctrler.send_data[i].iov_len){
					current_sended_size -= ctrler.send_data[i].iov_len;
					int32_t back_result = ((int32_t*)ctrler.send_data_tag)[i];
					if(back_result >= 0){
						free((char*)(ctrler.send_data[i].iov_base) - ctrler.send_data_tag[i]);
					}
					ctrler.send_data[i].iov_base = 0;
				}else {//current_sended_size maybe zero!
					ctrler.send_data[i].iov_base = (char*)(ctrler.send_data[i].iov_base) + current_sended_size;
					ctrler.send_data[i].iov_len -= current_sended_size;
					ctrler.send_data_tag[i] += current_sended_size;
					send_finished = false;
					if(i != 0){
						size_t j = 0;
						for( ;i < send_last_index && ctrler.send_data[i].iov_base !=0; ++i,++j ){ //move ahead!
							ctrler.send_data[j] = ctrler.send_data[i];
							ctrler.send_data_tag[j] = ctrler.send_data_tag[i];
						}
						ctrler.send_data[j].iov_base = 0;
					}
					break;
				}
			}
			//Send finished?
			if(send_finished){
				CO_RETURN;
			}
            continue;
		}
        }while(false);
        
	CO_END
}

const static uint32_t recv_poll_request = EPOLLIN;
cort_proto* cort_udp_connection_waiter::try_recv(){
	CO_BEGIN 
		if(!is_connected()){
			set_errno(cort_socket_error_codes::SOCKET_STATE_ERROR);
			CO_RETURN;
		}
		if(is_timeout_or_stopped()){
			close_connection(cort_socket_error_codes::SOCKET_OPERATION_TIMEOUT);
			CO_RETURN;	
		}
		uint32_t poll_event = get_poll_result();
		if( ((EPOLLERR) & poll_event) != 0){
			close_connection(cort_socket_error_codes::SOCKET_RECV_ERROR);
			CO_RETURN;	
		}
		
		if((poll_event & EPOLLIN) == 0){
			set_poll_request(recv_poll_request);
			CO_AGAIN;
		}

		cort_udp_ctrler* parent_waiter = (cort_udp_ctrler*)get_parent();
		
		if(parent_waiter->timeout != 0){
			this->set_timeout(parent_waiter->timeout);
			parent_waiter->timeout = 0;
		}
	
		recv_buffer_ctrl::recv_buffer_size_t to_recved_size;
		ssize_t recved_size;
		recv_buffer_ctrl* rcv_buf = &parent_waiter->recv_buffer;
		if(rcv_buf->recv_buffer == 0 && rcv_buf->realloc_recv_buffer() == 0){ //alloc error.
			close_connection(cort_socket_error_codes::SOCKET_ALLOC_MEMORY_ERROR);
			CO_RETURN;
		}
		int fd = get_cort_fd();
	recv_label:
		recved_size = recv(fd, rcv_buf->recv_buffer + rcv_buf->recved_size, 
				rcv_buf->recv_buffer_size - rcv_buf->recved_size, 0);
		if(recved_size > 0){
			rcv_buf->recved_size += recved_size;
			if(rcv_buf->data0._.recv_check_further_needed != 0){ //You have to recv recv_buffer_size in total
				if(rcv_buf->recved_size == rcv_buf->recv_buffer_size) {
					CO_RETURN; //recv_finished!
				}
				set_poll_request(recv_poll_request);
				CO_AGAIN;
			}
			//We do not realloc or poll to recv more here because we need check whether we have receive enough first.
		}
		else if(recved_size < 0){
			int thread_errno = errno;
			if (thread_errno == EINTR) {
				goto recv_label;
			}
			if ((thread_errno == EAGAIN) || (thread_errno == EWOULDBLOCK) ){
				set_poll_request(recv_poll_request);
				CO_AGAIN;
			}
			close_connection(cort_socket_error_codes::SOCKET_RECEIVE_ERROR);
			CO_RETURN;
		}
		else if(recved_size == 0){
			close_connection(cort_socket_error_codes::SOCKET_REMOTE_CANCELED);
			CO_RETURN;
		}
		
		to_recved_size = rcv_buf->recv_check(rcv_buf, parent_waiter);
		if(to_recved_size == rcv_buf->recved_size){
			if(rcv_buf->data0._.recv_finished_shrink_needed != 0){
				rcv_buf->shrink_to_fit();
			}
			CO_RETURN; //recv finished
		}
		if(to_recved_size == recv_buffer_ctrl::unexpected_data_received){
			close_connection(cort_socket_error_codes::SOCKET_RECEIVED_CHECK_ERROR);
			CO_RETURN; //recv bad data
		}
		else if(to_recved_size == 0){
			if(rcv_buf->recv_buffer_size == rcv_buf->recved_size){
				if(rcv_buf->realloc_recv_buffer(rcv_buf->recv_buffer_size<<1) != 0){
					goto recv_label;
				}
				close_connection(cort_socket_error_codes::SOCKET_ALLOC_MEMORY_ERROR);
				CO_RETURN;
			}
		}
		else{		
			if(to_recved_size > 0){
				if(to_recved_size <= rcv_buf->recved_size){
					CO_RETURN; //Even recv more than expected, we think it is ok
				}
				rcv_buf->data0._.recv_check_further_needed = 1;
			}else{
				to_recved_size = -to_recved_size;
			}
			if(to_recved_size > rcv_buf->recv_buffer_size){
				if(rcv_buf->recv_buffer_size == rcv_buf->recved_size && rcv_buf->realloc_recv_buffer(to_recved_size) != 0){
					goto recv_label;
				}
				rcv_buf->realloc_recv_buffer(to_recved_size);
			}
		}				
		if(rcv_buf->recv_buffer == 0){ //realloc error.
			close_connection(cort_socket_error_codes::SOCKET_ALLOC_MEMORY_ERROR);
			CO_RETURN;
		}
		set_poll_request(recv_poll_request);
		CO_AGAIN;
	CO_END
}

//This is not const. Use it only you want to await it!
cort_udp_connection_waiter* cort_udp_ctrler::lock_waiter(){
	if(!this->connection_waiter){
		if(this->keep_alive_ms > 0){
			this->connection_waiter = get_keep_alive_connection_client(this->ip_v4, this->port_v4, this->type_key);
		}
		if(!this->connection_waiter){
			this->connection_waiter.init<cort_udp_connection_waiter_client>();
		}
	}
	cort_udp_connection_waiter* result = this->connection_waiter.get_ptr();
	result->set_parent(this);
	return result;
}

cort_proto* cort_udp_ctrler::try_connect(){
	CO_BEGIN
		CO_AWAIT(lock_connect());
	CO_END
}

cort_proto* cort_udp_ctrler::try_send(){
	CO_BEGIN
		if(!this->connection_waiter){
			this->set_errno(cort_socket_error_codes::SOCKET_STATE_ERROR);
			CO_RETURN;
		}
		CO_AWAIT(lock_send());
	CO_END
}

cort_proto* cort_udp_ctrler::try_recv(){
	CO_BEGIN
		if(!this->connection_waiter){
			this->set_errno(cort_socket_error_codes::SOCKET_STATE_ERROR);
			CO_RETURN;
		}
		CO_AWAIT(lock_recv());
	CO_END
}

void cort_udp_ctrler::on_connection_inactive(){
	if(connection_waiter){
		if(keep_alive_ms > 0 && get_errno() == 0 && connection_waiter->is_connected()){
			connection_waiter->keep_alive(keep_alive_ms, ip_v4, port_v4, type_key);
		}
		connection_waiter.clear();
	}
}

cort_proto* cort_udp_ctrler::on_finish(){
	if(connection_waiter){
		if(get_errno() != 0){
			connection_waiter.clear();
		}
	}
	return cort_proto::on_finish();
}

static cort_proto* release_when_notification(cort_proto* arg){
	cort_udp_connection_waiter_client* udp_cort = (cort_udp_connection_waiter_client*)arg;
	udp_cort->release();
	return 0;
}

size_t cort_udp_connection_waiter_client::clear_keep_alive_connection(size_t count, uint32_t ip_arg, uint16_t port_arg, uint16_t type_key_arg){
	size_t result = 0;
	if(connection_udp_pool == 0){
		return result;
	}
	
	std::map<uint64_t, std::vector<cort_udp_connection_waiter_client* > >::iterator it = connection_udp_pool->begin();
	size_t max_vector_size = it->second.size();
	
	if(ip_arg != 0){
		ip_v4_key key;
		key.data.s_data.ip_v4 = ip_arg;
		key.data.s_data.port_v4 = port_arg;
		key.data.s_data.type_key = type_key_arg;
		it = connection_udp_pool->find(key.data.i_data);
	}else{
		std::map<uint64_t, std::vector<cort_udp_connection_waiter_client* > >::iterator end = connection_udp_pool->end();
		std::map<uint64_t, std::vector<cort_udp_connection_waiter_client* > >::iterator begin = it;
		++begin;
		for(;begin != end;++begin){
			size_t new_size = begin->second.size();
			if(new_size > max_vector_size){
				max_vector_size = new_size;
				it = begin;
			}
		}
	}
	if(it != connection_udp_pool->end()){
		std::vector<cort_udp_connection_waiter_client* >& dq = it->second;
		while(!dq.empty() && count-- != 0){
			cort_udp_connection_waiter_client* last = dq.back();				
			dq.pop_back();
			last->close_cort_fd();
			last->set_run_function(release_when_notification);
			++result;
		}
		if(dq.empty()){
			if(connection_udp_pool->size() == 1){
				delete connection_udp_pool;
				connection_udp_pool = 0;
			}else{
				connection_udp_pool->erase(it);
			}
		}	
	}
	return result;
}

cort_proto* cort_udp_request_response::on_finish(){
	finish_time_cost();
	return cort_udp_ctrler::on_finish();
}

cort_proto* cort_udp_request_response::start(){
	//Default is send first then recv mode.
	CO_BEGIN
		init_time_cost();
		CO_AWAIT(lock_connect());
		co_unlikely_if(get_errno() != 0){
			CO_RETURN;	
		}
		CO_AWAIT(lock_send());
		co_unlikely_if(get_errno() != 0){
			CO_RETURN;	
		}
		CO_AWAIT(lock_recv());
		co_unlikely_if(get_errno() != 0){
			CO_RETURN;	
		}
		on_connection_inactive();
	CO_END
}
