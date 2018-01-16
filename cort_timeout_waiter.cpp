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
#include "cort_timeout_waiter.h"

typedef cort_timeout_waiter::time_ms_t time_ms_t;

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
            std::list<cort_timeout_waiter_data> *data = short_timer_search[i].data;
            for(std::list<cort_timeout_waiter_data>::iterator it = data->begin(), end = data->end();it != end; ){
                cort_timeout_waiter_data *p = &(*it);
                p->data->resume_on_stop(); //This function may erase any element, so we need to be careful to iterate the rest.
                if(data->empty()){ // All are erased!
                    break;
                }
                it = data->begin();
                if(p == &(*it)){ // != means it is erased!
                    ++it;
                    p->data->clear_timeout();
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
            size_t right_pos = left_pos+1;
            if(right_pos >= timer_size){
                goto left_cmp;
            }{   //avoid goto warning
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
    struct epoll_event events[max_wait_count];                    //ev用于注册事件
    cort_timer_refresh_clock();
    time_ms_t start_poll_time = current_ms;
    do{
        if(until_ms <= current_ms){
            return -1;
        }
        int nfds = epoll_wait(epfd, events, max_wait_count, (int)(until_ms - current_ms));
        cort_timer_refresh_clock();
        if(nfds == 0){
            return -1;
        }
        if(nfds == max_wait_count){
            continue;
        }
        if(nfds < 0){//EINTR?
            continue;
        }
        for(int i = 0; i < nfds; i++)
        {
            ((cort_fd_waiter*)events[i].data.ptr)->resume_on_poll(events[i].events);
        }
    }while(false);
    return cort_timer_refresh_clock() - start_poll_time;
}

void cort_timer_loop(){
    while(eptimer->timer_size != 0){
        cort_timeout_waiter_data& tref = eptimer->timer_heap[0]->data->back();
        int result = cort_timer_poll(tref.end_time);
        if(result < 0){
            tref.data->resume_on_timeout();
        }
    }
}

time_ms_t cort_timeout_waiter::get_time_past() const{
    return cort_timer_now_ms() - (data_time.start_time_ms & ((timeout_masker | stopped_masker) - 1));
}

cort_timeout_waiter::cort_timeout_waiter(){
    that = 0;
    data_time.start_time_ms = 0;
}

cort_timeout_waiter::~cort_timeout_waiter(){
    if(that != 0 && eptimer != 0){
        eptimer->remove_timer(that);
    }
    delete that;
}

void cort_timeout_waiter::clear_timeout(){
    if(that != 0){
        eptimer->remove_timer(that);
        that = 0;
    }
}
cort_proto* cort_timeout_waiter::on_finish(){
    data_time.start_time_ms = cort_timer_now_ms() - (data_time.start_time_ms & ((timeout_masker | stopped_masker) - 1));
    return 0;
}
void cort_timeout_waiter::set_timeout(time_ms_t timeout_ms){
    clear_timeout();
    this->data_time.start_time_ms = cort_timer_now_ms();
    that = eptimer->add_timer(this, timeout_ms);
}

void cort_timeout_waiter::clear(){
    clear_timeout();
    cort_proto::clear();
}

void cort_timeout_waiter::resume_on_timeout(){  
    data_time.start_time_ms |= timeout_masker;
    this->resume();
}

void cort_timeout_waiter::resume_on_stop(){ 
    data_time.start_time_ms |= stopped_masker;
    this->resume();
}

void cort_fd_waiter::resume_on_poll(uint32_t poll_event){
    this->poll_result = poll_event;
    this->resume();
}

int cort_fd_waiter::set_poll_request(int arg_fd, uint32_t arg_poller_event){
    cort_fd = arg_fd;
    struct epoll_event event;
    event.data.ptr = this;
    event.events = arg_poller_event;
    return epoll_ctl(cort_get_poll_fd(), EPOLL_CTL_ADD, arg_fd, &event);
}

int cort_fd_waiter::reset_poll_request(uint32_t arg_poller_event){
    struct epoll_event event;
    event.data.ptr = this;
    event.events = arg_poller_event;
    return epoll_ctl(cort_get_poll_fd(), EPOLL_CTL_ADD, cort_fd, &event);
}

int cort_fd_waiter::remove_poll_request(){
    struct epoll_event event;
    return epoll_ctl(cort_get_poll_fd(), EPOLL_CTL_DEL, cort_fd, &event);
}

#ifdef TIMEOUT_TEST
int main(void)
{
    struct stdio_echo_test : public cort_fd_waiter{
        int last_time_out;
        struct epoll_event ev;
        CO_DECL(stdio_echo_test)
        cort_proto* start(){
        CO_BEGIN
            set_poll_request(0, EPOLLIN|EPOLLHUP);
            last_time_out = 5000;
            set_timeout(last_time_out);
            CO_YIELD();
            if(is_timeout()){
                printf("timeout! %dms cost\n", (int)get_time_past());
                set_timeout((++last_time_out)%256); //We will check the timer accuracy.
                CO_AGAIN;   
            }
            if(is_stopped()){
                puts("current coroutine is canceled!");
                clear_timeout();
                CO_RETURN;
            }
            if(get_poll_result() != EPOLLIN){
                remove_poll_request();
                clear_timeout();
                puts("exception happened?");
                CO_RETURN;
            }
            
            char buf[1024];
            int result = read(0, buf, 1023);
            if(result < 0){
                remove_poll_request();
                clear_timeout();
                puts("read exception?");
            }
            else{
                write(1,buf,result);
                last_time_out = 4095;
                cort_timer_refresh_clock();
                remove_poll_request();
                clear_timeout();
                //CO_AGAIN; It(CO_AGAIN) will be never resumed after remove_poll_request and clear_timeout.
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
