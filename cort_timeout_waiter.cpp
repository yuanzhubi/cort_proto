#include <sys/time.h>
#include <stdlib.h>
#include <map>
#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <stdint.h>
#include <list>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include "cort_timeout_waiter.h"

struct cort_timer;

typedef cort_timeout_waiter::time_ms_t time_ms_t;
static __thread int epfd = 0;
static __thread cort_timer* eptimer = 0;

static __thread uint32_t epollfd_total_count = 0;

struct timeout_list;

//cort_timeout_waiter_data and cort_timeout_waiter are weak refrenced each other.
//cort_timeout_waiter_data should be deconstructed earlier than cort_timeout_waiter.

struct cort_timeout_waiter_data{
    cort_timeout_waiter* data;
    time_ms_t end_time;
    timeout_list * host_list;
    std::list<cort_timeout_waiter_data>::iterator pos;
};

struct timeout_list{
    mutable std::list<cort_timeout_waiter_data> *data;
    time_ms_t timeout;
    size_t heap_pos;
    const static size_t heap_npos = size_t(-1);
    timeout_list(){
        data = new std::list<cort_timeout_waiter_data>();
        timeout = 0;
        heap_pos = heap_npos;
    }
    //move constructor in fact.
    timeout_list(const timeout_list& rhs){
        data = rhs.data;
        heap_pos = rhs.heap_pos;
        timeout = 0;
        rhs.data = 0;
    }
    ~timeout_list(){
        delete data;
    }
};

struct cort_timer{
    timeout_list **timer_heap;
    size_t timer_size;
    size_t timer_capacity;
    const static size_t short_timer_list_size = 4096;
    const static size_t heap_size_delta = 64;
    const static size_t heap_npos = size_t(-1);
    
    std::map<time_ms_t, timeout_list > long_timer_search;
    timeout_list short_timer_search[short_timer_list_size];
    
	cort_timeout_waiter_data* get_next_timer() const{
		if(timer_size != 0){
			return &eptimer->timer_heap[0]->data->back();
		}
		return 0;
	}
    cort_timer(size_t capacity = heap_size_delta){
        timer_heap = (timeout_list **)malloc(capacity * sizeof(*timer_heap));
        timer_size = 0;
        timer_capacity = capacity;
        for(size_t i = 0; i<short_timer_list_size; ++i){
            short_timer_search[i].timeout = i;
            //short_timer_search[i].heap_pos = heap_npos;
        }
    }
    
    ~cort_timer(){
        for(size_t i = 0; i<short_timer_list_size; ++i){
            std::list<cort_timeout_waiter_data> *data = short_timer_search[i].data;
            for(std::list<cort_timeout_waiter_data>::iterator it = data->begin(), end = data->end();it != end; ){
                cort_timeout_waiter_data *p = &(*it);
                p->data->resume_on_stop(); //This function may erase any element(even p itself!), so we need to be careful to iterate the rest.
                if(data->empty()){ // All are erased!
                    break;
                }
                it = data->begin();
                if(p == &(*it)){ // == means it is not erased!
                    ++it;
                    p->data->clear_timeout();// "--it" is erased in the function.
                }
            }
        }
        for(std::map<time_ms_t, timeout_list >::iterator mapit = long_timer_search.begin(), end = long_timer_search.end();
            mapit != end;){
            std::list<cort_timeout_waiter_data> *data = mapit->second.data;
            for(std::list<cort_timeout_waiter_data>::iterator it = data->begin(), end = data->end();it != end;){
                cort_timeout_waiter_data *p = &(*it);
                p->data->resume_on_stop(); //This function may erase any element, so we need to be careful to iterate the rest.
                
                if(long_timer_search.empty()){ //Parent container are all erased
                    goto deconstructor_end;
                }
                if(long_timer_search.begin()->second.data != data){ //Parent container is erased
                    break;
                }
                
                it = data->begin();             
                if(p == &(*it)){ // != means it is erased!
                    ++it;
                    p->data->clear_timeout();
                }
                
                if(long_timer_search.empty()){ //Parent container are all erased
                    goto deconstructor_end;
                }
                if(long_timer_search.begin()->second.data != data){ //Parent container are erased
                    break;
                }
            }
            // old mapit must be erased!
            mapit = long_timer_search.begin();
        }
    deconstructor_end:
        free(timer_heap);
    }
    
    timeout_list& get_timeout_list(time_ms_t timeout){
        if(timeout < short_timer_list_size){
            return short_timer_search[timeout];
        }       
        timeout_list& data = long_timer_search[timeout];
        if(data.timeout != timeout){
            data.timeout = timeout;
        }
        return data;
    }
    
    bool up_update_heap(size_t pos, time_ms_t my_time){
        timeout_list* pos_data = timer_heap[pos];
        size_t old_pos = pos;
        if(pos != 0){
            size_t new_pos = pos >> 1;
            do{
                timeout_list* new_data = timer_heap[new_pos];
                if(my_time < new_data->data->back().end_time){
                    timer_heap[old_pos] = new_data;
                    new_data->heap_pos = old_pos;
                }
                else{
                    break;
                }
                old_pos = new_pos;
                new_pos >>= 1;
            }while(old_pos != 0);
            if(old_pos != pos){
                timer_heap[old_pos] = pos_data;
                pos_data->heap_pos = old_pos;
                return true;
            }
        }
        return false;
    }
    
    bool down_update_heap(size_t pos, time_ms_t my_time){
        timeout_list* pos_data = timer_heap[pos];
        size_t old_pos = pos;
        while(true){
            size_t left_pos = (old_pos<<1)+1;
            if(left_pos >= timer_size){
                break;
            }
            timeout_list* left_data = timer_heap[left_pos];
            time_ms_t left_time = left_data->data->back().end_time;         
            size_t right_pos = left_pos + 1;
            if(right_pos < timer_size){
                timeout_list* right_data = timer_heap[right_pos];
                time_ms_t right_time = right_data->data->back().end_time;           
                if(left_time > right_time){
                    left_data = right_data;
					left_time = right_time;
                }
            }
            if(left_time < my_time){
                timer_heap[old_pos] = left_data;
                left_data->heap_pos = old_pos;
                old_pos = left_pos;
				continue;
            }
            else{
                break;
            }
        }
        if(old_pos != pos){
            timer_heap[old_pos] = pos_data;
            pos_data->heap_pos = old_pos;
            return true;
        }
        return false;
    }
    
    void insert_time_out(timeout_list* pos_data, time_ms_t time_out){
        if(timer_size == timer_capacity){
            timer_capacity += heap_size_delta;
            timer_heap = (timeout_list**)realloc(timer_heap, timer_capacity*sizeof(*timer_heap));
        }
        pos_data->heap_pos = timer_size;
        timer_heap[timer_size] = pos_data;
        up_update_heap(timer_size++, time_out);
    }
    
    void update_time_out(size_t pos){
        timeout_list* pos_data = timer_heap[pos];
        co_unlikely_if(pos_data->data->empty()){
            --timer_size;
            if(timer_size != 0){
				if(pos != timer_size){
					timeout_list* new_pos_data = (timer_heap[pos] = timer_heap[timer_size]);
					new_pos_data->heap_pos = pos;
					down_update_heap(pos, new_pos_data->data->back().end_time);
				}
            }
            if(pos_data->timeout >= short_timer_list_size){
                long_timer_search.erase(pos_data->timeout);
            }
            else{
                pos_data->heap_pos = heap_npos;
            }
            return;
        }
        time_ms_t my_time = pos_data->data->back().end_time;
        up_update_heap(pos, my_time) || down_update_heap(pos, my_time);
    }
    
    cort_timeout_waiter_data* add_timer(cort_timeout_waiter* timer, time_ms_t timeout){
        timeout_list* data = &get_timeout_list(timeout);
        time_ms_t data_endtime = cort_timer_now_ms() + timeout;
        cort_timeout_waiter_data result;
        result.data = timer;
        result.end_time = data_endtime;
        result.host_list = data;
        data->data->push_front(result);
        std::list<cort_timeout_waiter_data>::iterator it_result = data->data->begin();
        cort_timeout_waiter_data& real_result = *it_result;
        real_result.pos = it_result;
        if(data->heap_pos == heap_npos){
            insert_time_out(data, data_endtime);
        }
        return &real_result;
    }
    
    void remove_timer(cort_timeout_waiter_data* end_time_result){
        cort_timeout_waiter_data* addr = &(end_time_result->host_list->data->back());
        size_t old_pos = end_time_result->host_list->heap_pos;
        end_time_result->host_list->data->erase(end_time_result->pos);
        if(addr == end_time_result){
            update_time_out(old_pos);
        }
    }
};

int cort_timer_init(){
	if(eptimer != 0){
		return -1;
	}
	epollfd_total_count = 0;
	eptimer = new cort_timer();
    epfd = epoll_create(1);
    if(epfd <= 0){
        return -1;
    }
    signal(SIGHUP,  SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
	return 0;
    //signal(SIGINT,  SIG_IGN);
    //signal(SIGTERM, SIG_IGN);
    //daemon(1, 1);
}
void cort_timer_destroy(){
    if(epfd > 0){
        close(epfd);
		epfd = 0;
    }
    delete eptimer;
	eptimer = 0;
}

#if defined( __USE_RDTSCP__) 
static inline unsigned long long counter(void)
{
    register uint32_t lo, hi;
    register unsigned long long o;
    __asm__ __volatile__ (
            "rdtscp" : "=a"(lo), "=d"(hi)
            );
    o = hi;
    o <<= 32;
    return (o | lo);
}

static inline time_ms_t getCpuKhz()
{
    FILE *fp = fopen("/proc/cpuinfo","r");
    if(!fp) return 1;
    static char buf[4096] = {0};
    fread(buf,1,sizeof(buf),fp);
    fclose(fp);
    char *lp = strstr(buf,"cpu MHz");
    if(!lp) return 1;
    lp += sizeof("cpu MHz") - 1;
    while(*lp == ' ' || *lp == '\t' || *lp == ':'){
        ++lp;
    }
    double mhz = atof(lp);
    time_ms_t u = ((time_ms_t)mhz * 1000ull);
    return u;
}
#endif

static inline time_ms_t now_ms()
{
#if defined( __USE_RDTSCP__) 
    static uint32_t khz = getCpuKhz();
    return counter() / khz;
#else
    struct timeval now;gettimeofday( &now,NULL );
    time_ms_t u = now.tv_sec * 1000ull + (now.tv_usec / 1000); //almost now.tv_usec / 1000
    return u;
#endif
}

static time_ms_t current_ms = now_ms();

cort_timeout_waiter::time_ms_t cort_timer_refresh_clock(){
    current_ms = now_ms();
    return current_ms;
}

cort_timeout_waiter::time_ms_t cort_timer_now_ms(){
    return current_ms;
}

int cort_get_poll_fd(){
    return epfd;
}

int cort_timer_poll(cort_timeout_waiter::time_ms_t until_ms){
    const static int max_wait_count = 4*1024;
    struct epoll_event events[max_wait_count];                   
    do{
		int wait_time;
        if(until_ms <= cort_timer_refresh_clock()){
			if(until_ms == 0){
				wait_time = 1*1000;
			}
			else{
				return -1;
			}
        }else{
			wait_time = int(until_ms - current_ms);
		}
		if(epfd == 0){
			return 0;
		}
        int nfds = epoll_wait(epfd, events, max_wait_count, wait_time);
        cort_timer_refresh_clock();
        if(nfds == 0){
            return -1;
        }
        if(nfds < 0){//EINTR?
            continue;
        }
        for(int i = 0; i < nfds; i++){
            ((cort_fd_waiter*)events[i].data.ptr)->resume_on_poll(events[i].events);
			if(epfd == 0){
				return 0;
			}
        }
		if(nfds == max_wait_count){
            continue;
        }
    }while(false);
    return 0;
}

void cort_timer_loop(){
	while(true){
		if(eptimer == 0){
			break;
		}
		
		for(cort_timeout_waiter_data* ptimer =  eptimer->get_next_timer();ptimer != 0;ptimer =  eptimer->get_next_timer()){
			int result = cort_timer_poll(ptimer->end_time);
			if(result < 0){
				ptimer->data->resume_on_timeout();
			}
			if(eptimer == 0){
				break;
			}
		}
		if(epollfd_total_count != 0){
			cort_timer_poll(0);
			continue;
		}
		break;
	}
}

uint32_t cort_timeout_waiter::get_time_past() const{
    return (uint32_t)(cort_timer_now_ms() - start_time_ms);
}

cort_timeout_waiter::cort_timeout_waiter(){
    that = 0;
    start_time_ms = 0;
	time_cost_ms = 0;
	ref_count = 0;
}

cort_timeout_waiter::~cort_timeout_waiter(){
    if(that != 0 && eptimer != 0){
        eptimer->remove_timer(that);
    }
}

time_ms_t cort_timeout_waiter::get_timeout_time() const {
	return that->end_time;
}

void cort_timeout_waiter::clear_timeout(){
    if(that != 0 && eptimer != 0){
        eptimer->remove_timer(that);
        that = 0;
    }
}
void cort_timeout_waiter::on_finish(){
    time_cost_ms = (time_cost_ms & normal_masker) | ((time_cost_ms_t)(cort_timer_now_ms() - start_time_ms));
    clear_timeout();
	cort_proto::on_finish();
}
void cort_timeout_waiter::set_timeout(time_ms_t timeout_ms){
    clear_timeout();
	time_cost_ms = 0;
	start_time_ms = cort_timer_now_ms();
	that = eptimer->add_timer(this, timeout_ms);
}

void cort_timeout_waiter::clear(){
    start_time_ms = 0;
	time_cost_ms = 0;
	clear_timeout();
    cort_proto::clear();
}

void cort_timeout_waiter::resume_on_timeout(){  
	this->clear_timeout();
    time_cost_ms = timeout_masker;
    this->resume();
}

void cort_timeout_waiter::resume_on_stop(){ 
	time_cost_ms = stopped_masker;
    this->resume();
}

cort_fd_waiter::~cort_fd_waiter(){
	close_cort_fd();
}

void cort_fd_waiter::resume_on_poll(uint32_t poll_event){
    this->poll_result = poll_event;
    this->resume();
}

int cort_fd_waiter::set_poll_request(uint32_t arg_poll_request){
	clear_poll_result();
	if(poll_request == arg_poll_request){
		return 0;
	}
    struct epoll_event event;
    event.data.ptr = this;
    event.events = arg_poll_request;
	int result = 0; 
	if(poll_request == 0){
		result = epoll_ctl(epfd, EPOLL_CTL_ADD, cort_fd, &event);
		if(result == 0){
			++epollfd_total_count;
		}
	}
	else{
		result = epoll_ctl(epfd, EPOLL_CTL_MOD, cort_fd, &event);
	}
	poll_request = arg_poll_request;
    return result;
}

int cort_fd_waiter::remove_poll_request(){
	struct epoll_event event;
    int result = epoll_ctl(epfd, EPOLL_CTL_DEL, cort_fd, &event);
	if(result == 0 && poll_request != 0){
		--epollfd_total_count;
		poll_request = 0;
	}
	return result;
}

void cort_fd_waiter::close_cort_fd(){
	if(cort_fd > -1){
		if(poll_request != 0){
			--epollfd_total_count;
		}
		close(cort_fd);
	}
	cort_fd = -1;
	poll_request = 0;
	poll_result = 0;
}

