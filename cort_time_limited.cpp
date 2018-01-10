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
#include "cort_time_limited.h"

typedef cort_time_limited::time_ms_t time_ms_t;

struct timeout_list;

//end_time_t and cort_time_limited are weak refrenced each other.
//end_time_t should be deconstructed earlier than cort_time_limited.

struct end_time_t{
    cort_time_limited* data;
    time_ms_t end_time;
    timeout_list * host_list;
    std::list<end_time_t>::iterator pos;
};

struct timeout_list{
    mutable std::list<end_time_t> *data;
    time_ms_t timeout;
    size_t heap_pos;
    timeout_list(){
        data = new std::list<end_time_t>();
        timeout = 0;
        heap_pos = (size_t)-1;
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
    
    cort_timer(size_t capacity = heap_size_delta){
        timer_heap = (timeout_list **)malloc(capacity * sizeof(*timer_heap));
        timer_size = 0;
        timer_capacity = capacity;
        
        for(size_t i = 0; i<short_timer_list_size; ++i){
            short_timer_search[i].timeout = i;
            short_timer_search[i].heap_pos = heap_npos;
        }
    }
    
    ~cort_timer(){
        for(size_t i = 0; i<short_timer_list_size; ++i){
            std::list<end_time_t> &data = *short_timer_search[i].data;
            for(std::list<end_time_t>::iterator it = data.begin(), end = data.end();
            it != end; ){
                //using it++ because coroutine may remove the end_time_t
                (it++)->data->resume_on_stop(cort_timer_now_ms());
            }
        }
        for(std::map<time_ms_t, timeout_list >::iterator it = long_timer_search.begin(), end = long_timer_search.end();
            it != end;){
            std::list<end_time_t> &data = *(it++)->second.data;
            for(std::list<end_time_t>::iterator it = data.begin(), end = data.end();
            it != end;){
                //using it++ because coroutine may remove the end_time_t
                (it++)->data->resume_on_stop(cort_timer_now_ms());
            }
        }
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
            size_t right_pos = left_pos+1;
            if(right_pos >= timer_size){
                goto left_cmp;
            }
            {   //avoid goto warning
                timeout_list* right_data = timer_heap[right_pos];
                time_ms_t right_time = right_data->data->back().end_time;           
                if(left_time <= right_time){
                    goto left_cmp;
                }
                if(right_time < my_time){
                    timer_heap[old_pos] = right_data;
                    right_data->heap_pos = old_pos;
                    old_pos = right_pos;
                }
                else{
                    break;
                }
            }
left_cmp:
            if(left_time < my_time){
                timer_heap[old_pos] = left_data;
                left_data->heap_pos = old_pos;
                old_pos = left_pos;
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
        if(pos_data->data->empty()){
            --timer_size;
            if(timer_size != 0){
                timeout_list* new_pos_data = (timer_heap[pos] = timer_heap[timer_size]);
                down_update_heap(pos, new_pos_data->data->back().end_time);
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
    
    end_time_t* add_timer(cort_time_limited* timer, time_ms_t timeout){
        timeout_list* data = &get_timeout_list(timeout);
        time_ms_t data_endtime = cort_timer_now_ms() + timeout;
        end_time_t result;
        result.data = timer;
        result.end_time = data_endtime;
        result.host_list = data;
        data->data->push_front(result);
        std::list<end_time_t>::iterator it_result = data->data->begin();
        end_time_t& real_result = *it_result;
        real_result.pos = it_result;
        if(data->heap_pos == heap_npos){
            insert_time_out(data, data_endtime);
        }
        return &real_result;
    }
    
    void remove_timer(end_time_t* end_time_result){
        end_time_t* addr = &(end_time_result->host_list->data->back());
        size_t old_pos = end_time_result->host_list->heap_pos;
        end_time_result->host_list->data->erase(end_time_result->pos);
        if(addr == end_time_result){
            update_time_out(old_pos);
        }
    }
};

static __thread int epfd = 0;
static __thread cort_timer* eptimer = 0;

void cort_timer_init(){
    epfd = epoll_create(1);
    if(epfd <= 0){
        exit(-1);
    }
    try{
        eptimer = new cort_timer();
    }
    catch(...){
        close(epfd);
    }
    
    signal(SIGHUP,  SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    //signal(SIGINT,  SIG_IGN);
    //signal(SIGTERM, SIG_IGN);
    //daemon(1, 1);
}
void cort_timer_destroy(){
    if(epfd > 0){
        close(epfd);
    }
    delete eptimer;
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
    lp += /*strlen("cpu MHz")*/7;
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

cort_time_limited::time_ms_t cort_timer_refresh_clock(){
    current_ms = now_ms();
    return current_ms;
}

cort_time_limited::time_ms_t cort_timer_now_ms(){
    return current_ms;
}

int cort_get_poll_fd(){
    return epfd;
}

int cort_timer_poll(cort_time_limited::time_ms_t until_ms){
    const static int max_wait_count = 4*1024;
    struct epoll_event events[max_wait_count];                    //ev用于注册事件
    cort_timer_refresh_clock();
    time_ms_t start_poll_time = current_ms;
    do{
        if(until_ms <= current_ms){
            return -1;
        }
        int nfds = epoll_wait(epfd, events, max_wait_count, (int)(until_ms - current_ms));
        cort_timer_refresh_clock();
        for(int i = 0; i < nfds; i++)
        {
            ((cort_time_limited*)events[i].data.ptr)->resume_normal(current_ms, events[i].events);
        }
        if(nfds == 0){
            return -1;
        }
        if(nfds == max_wait_count){
            cort_timer_refresh_clock();
            continue;
        }
        if(nfds < 0){//EINTR?
            cort_timer_refresh_clock();
            continue;
        }
    }while(false);
    return cort_timer_refresh_clock() - start_poll_time;
}

void cort_timer_loop(){
    while(eptimer->timer_size != 0){
        end_time_t& tref = eptimer->timer_heap[0]->data->back();
        int result = cort_timer_poll(tref.end_time);
        if(result < 0){
            tref.data->resume_on_timeout(current_ms);
        }
    }
}

struct cort_time_limited_data{
    end_time_t* overtime_timer;
};
    
cort_time_limited::cort_time_limited(){
    that = new cort_time_limited_data();
    that->overtime_timer = 0;
}

cort_time_limited::~cort_time_limited(){
    if(that->overtime_timer != 0 && eptimer != 0){
        eptimer->remove_timer(that->overtime_timer);
    }
    delete that;
}

void cort_time_limited::clear_timeout(){
    if(that->overtime_timer != 0){
        eptimer->remove_timer(that->overtime_timer);
        that->overtime_timer = 0;
    }
}

void cort_time_limited::set_timeout(time_ms_t timeout_ms){
    clear_timeout();
    this->data2.start_time_ms = cort_timer_now_ms();
    that->overtime_timer = eptimer->add_timer(this, timeout_ms);
}

void cort_time_limited::clear(){
    clear_timeout();
    cort_proto::clear();
}

void cort_time_limited::resume_on_timeout(time_ms_t now_time_ms){
    uint32_t start_time_ms = (uint32_t)this->data2.start_time_ms;
    this->data2._.time_cost_ms = now_time_ms - start_time_ms;
    this->data2._.poll_event = 0;
    this->resume();
}

void cort_time_limited::resume_on_stop(time_ms_t now_time_ms){
    uint32_t start_time_ms = (uint32_t)this->data2.start_time_ms;
    this->data2._.time_cost_ms = now_time_ms - start_time_ms;
    this->data2._.poll_event = (uint32_t)-1;
    this->resume();
}

void cort_time_limited::resume_normal(time_ms_t now_time_ms, uint32_t poll_event){
    uint32_t start_time_ms = (uint32_t)this->data2.start_time_ms;
    this->data2._.time_cost_ms = now_time_ms - start_time_ms;
    this->data2._.poll_event = poll_event;
    this->resume();
}

#ifdef TEST
int main(void)
{
    struct stdio_echo_test : public cort_time_limited{
        int last_time_out;
        struct epoll_event ev;
        CO_DECL(stdio_echo_test)
        cort_proto* start(){
        CO_BEGIN
            ev.events = EPOLLIN|EPOLLHUP;
            ev.data.ptr = this;
            epoll_ctl(cort_get_poll_fd(), EPOLL_CTL_ADD, 0, &ev); 
            last_time_out = 5000;
            set_timeout(last_time_out);
            CO_AWAIT_ANY();
            if(is_timeout()){
                printf("timeout! %dms cost\n", (int)get_time_cost());
                set_timeout((++last_time_out)%256);
                CO_AGAIN;   
            }
            if(is_stopped()){
                epoll_ctl(cort_get_poll_fd(), EPOLL_CTL_DEL, 0, &ev);
                clear_timeout();
                puts("stop?");
                CO_RETURN;
            }
            
            char buf[1024];
            int result = read(0, buf, 1023);
            if(result < 0){
                epoll_ctl(cort_get_poll_fd(), EPOLL_CTL_DEL, 0, &ev);
                clear_timeout();
                puts("read exception?");
            }
            else{
                write(1,buf,result);
                last_time_out = 4095;
                cort_timer_refresh_clock();
                //set_timeout(last_time_out);
                epoll_ctl(cort_get_poll_fd(), EPOLL_CTL_DEL, 0, &ev);
                clear_timeout();
                CO_AGAIN;
            }
        CO_END
        }
    }test_cort0, test_cort1;
    cort_timer_init();
    test_cort0.start();
    cort_timer_loop();
    test_cort1.start();
    cort_timer_loop();
    test_cort0.start();
    test_cort1.start();
    //Test cort_timer_destroy without loop before!
    //cort_timer_loop();
    //Then the couroutine should be stopped after destroy.
    cort_timer_destroy();
    cort_timer_init();
    test_cort1.start();
    cort_timer_loop();
    cort_timer_destroy();
    cort_timer_init();
    test_cort0.start();
    cort_timer_destroy();
    puts("finished"); //valgrind test past
    return 0;
}

#endif
