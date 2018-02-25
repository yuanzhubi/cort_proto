#ifndef CORT_TIME_LIMITED_H_
#define CORT_TIME_LIMITED_H_

#include <stdint.h>
#include "cort_proto.h"

#if defined(__x86_64__) || defined(__i386__)
#define CO_USE_RDTSC //using rdtsc to get clock

#endif

struct cort_timeout_waiter_data;

//cort_timeout_waiter must be leaf coroutine. Because if you await other coroutine, time out limit may be disobeyed.
//But you can await "yourself" via CO_SELF_AWAIT.
struct cort_timeout_waiter : public cort_proto{
    typedef uint64_t time_ms_t;
    typedef uint32_t time_cost_ms_t;
    
    void resume_on_timeout();
    
    //resume_on_stop should be called by the scheduler when
    //you want to stop the thread, so you need to stop all the leaf coroutines(then they will resume their parents).
    //Then cort_timer_destroy can lead all the cort_timeout_waiter stop one bye one.
    void resume_on_stop();
    
protected:    
    cort_timeout_waiter(const cort_timeout_waiter&);
    const static time_cost_ms_t timeout_masker = (((time_cost_ms_t)1)<<(sizeof(time_cost_ms_t)*8 - 1));
    const static time_cost_ms_t stopped_masker = (((time_cost_ms_t)1)<<(sizeof(time_cost_ms_t)*8 - 2));
    const static time_cost_ms_t normal_masker = timeout_masker | stopped_masker;
    cort_timeout_waiter_data* that;
    time_ms_t start_time_ms;
    time_cost_ms_t time_cost_ms;
public:
    uint32_t ref_count;
    uint32_t get_time_cost() const {
        return (time_cost_ms & (~normal_masker));
    }
    
    inline bool is_timeout_or_stopped() const{
        return (time_cost_ms & normal_masker) != 0;
    }
    
    inline bool is_timeout() const{
        return (time_cost_ms & timeout_masker) != 0;
    }
    
    inline bool is_stopped() const{
        return (time_cost_ms & stopped_masker) != 0;
    }
    
    void add_ref(){
        ++this->ref_count;
    }

    uint32_t remove_ref(){
        return --this->ref_count;
    }

    uint32_t release(){
        switch(this->ref_count){
        case 0: //The object is not managed by ref_count. We think "this" is a strong reference in default.
        case 1:
            delete this;
            return 0;
        default:
            return --this->ref_count;
        }
    }
    bool is_set_timeout() const{
        return (that != 0) ;
    }
    time_ms_t get_timeout_time() const;
    uint32_t get_time_past() const;     
    void on_finish();
    void set_timeout(time_ms_t timeout_ms);
    void clear_timeout();
    virtual void clear();
    cort_timeout_waiter();
protected:    
    
    virtual ~cort_timeout_waiter();
};

//COM style reference count management. Every reference is strong reference.
template<typename T>
struct cort_shared_ptr{
    T* cort;
    T& operator *() const {return *cort;}
    T* operator ->() const {return cort;}
    
    ~cort_shared_ptr(){
        if(cort != 0){
            cort->release();
        }
    }
    
    operator bool() const {return cort != 0;}
    
    cort_shared_ptr(){
        cort = 0;
    }

    cort_shared_ptr(T* rhs){
        cort = rhs;
        if(cort != 0){
            cort->add_ref();
        }
    }

    template<typename G>
    explicit cort_shared_ptr(const cort_shared_ptr<G>& rhs){
        cort = rhs.cort;
        if(cort != 0){
            cort->add_ref();
        }
    }
    
    template<typename G>
    cort_shared_ptr& operator = (const cort_shared_ptr<G>& rhs){
        if(cort == rhs.cort){
            return *this;
        }
        if(cort != 0){
            cort->release();
        }
        cort = rhs.cort;
        if(cort != 0){
            cort->add_ref();
        }
        return *this;
    }
    
    template<typename G>
    cort_shared_ptr& operator = (G* ptr){
        if(cort == ptr){
            return *this;
        }
        if(cort != 0){
            cort->release();
        }
        cort = ptr;
        if(cort != 0){
            cort->add_ref();
        }
        return *this;
    }
    
    uint32_t clear(){
        if(cort != 0){
            uint32_t result = cort->release();
            cort = 0;
            return result;
        }
        return 0;
    }
    
    template<typename G>
    void init(){
        if(cort != 0){
            cort->release();
        }
        cort = new G();
        cort->add_ref();
    }
    
    T* get_ptr() const{
        return cort;
    }
};

//This should be a leaf coroutine. It can only resumed by outer controler.
//Because when your fd is ready, you have to react before next poll.
struct cort_fd_waiter : public cort_timeout_waiter{
    int cort_fd;
    uint32_t poll_request;
    uint32_t poll_result;
    uint32_t reserved_data;
public:
    cort_fd_waiter(){
        cort_fd = -1;
        poll_request = 0;
        poll_result = 0;
    }
    ~cort_fd_waiter();
    
    int set_poll_request(uint32_t arg_poll_request);
    int remove_poll_request();
    
    void close_cort_fd();
    void remove_cort_fd();
    
    void set_cort_fd(int fd){
        cort_fd = fd;
    }
    int get_cort_fd() const{
        return cort_fd;
    }
    uint32_t get_poll_request() const {
        return poll_request;
    }
    uint32_t get_poll_result() const{
        return poll_result;
    }
    void clear_poll_result() {
        poll_result = 0;
    }
    void set_poll_result(uint32_t new_poll_result) {
        poll_result = new_poll_result;
    }
    void resume_on_poll(uint32_t poll_event);
    static uint32_t cort_waited_fd_count_thread();
};

//Usual program stages:

//1. Call cort_timer_init to prepare for the timers.
int cort_timer_init();

//2. After your cort_timeout_waiter start, call cort_timer_loop.
void cort_timer_loop();

//3. The above cort_timer_loop returns when every cort_timeout_waiter is finished, then call cort_timer_destroy for recycle. 
void cort_timer_destroy();

//1,2,3 steps can be called in every thread.

cort_timeout_waiter::time_ms_t cort_timer_refresh_clock();
cort_timeout_waiter::time_ms_t cort_timer_now_ms();

#define CO_SLEEP(timeout_ms) CO_AWAIT(new cort_sleeper(timeout_ms))
#define CO_SLEEP_IF(bool_exp, timeout_ms) CO_AWAIT_IF(bool_exp, new cort_sleeper(timeout_ms))
#define CO_SLEEP_AGAIN(timeout_ms) CO_AWAIT_AGAIN(new cort_sleeper(timeout_ms))
#define CO_SLEEP_AGAIN_IF(bool_exp, timeout_ms) CO_AWAIT_AGAIN_IF(bool_exp, new cort_sleeper(timeout_ms))

int cort_get_poll_fd();
int cort_timer_poll(cort_timeout_waiter::time_ms_t until_ms);

struct cort_sleeper : public cort_timeout_waiter{
    CO_DECL(cort_sleeper)
    cort_sleeper(time_ms_t timeout_ms){set_timeout(timeout_ms);}
    cort_proto* start(){
        CO_BEGIN
            CO_YIELD();
            //Now it is must be timeout
        CO_END
    }
protected:
    void on_finish(){
        delete this;
    }
};

template<typename T>
struct cort_repeater : public cort_timeout_waiter{
    CO_DECL(cort_repeater)
    void set_repeat_per_second(double count){
        req_count = count;
        if(count > 100){
            unsigned int intcount = (unsigned int)count;
            interval_count = intcount / 100;
            first_interval_count = intcount % 100;
            type = 0;
        }
        else if(count > 1.0){
            unsigned int intcount = (unsigned int)count;
            interval =  1000 / intcount;
            first_interval = 1000 % intcount;
            interval_count = intcount;
            type = 1;
        }
        else  if(count > 1e-3){
            unsigned int intcount = (unsigned int)(count*1000);
            interval =  1000 * 1000 / intcount ;
            first_interval = 1000 * 1000 % intcount;
            interval_count = intcount;
            type = 1000;
        }
        index = 0;
        real_cort_count = 0;
    }
    void stop(){
        clear_timeout();
        real_cort_count = 0;
        interval_count = 0;
        first_interval_count = 0;
        
        interval = 0;
        first_interval = 0;
        
        index = 0;
        type = 65535;
    }
    cort_proto* start(){
        last_time = cort_timer_now_ms();
        start_time = 0;
        CO_BEGIN
            if(!this->is_stopped() && type != 65535){
                switch(type){
                    case 0:{
                        this->set_timeout(10);
                    }
                    break;
                    case 1:{
                        unsigned int real_interval = ((index < first_interval)?(interval+1):interval);
                        this->set_timeout(real_interval);
                    }
                    break;
                    case 1000:{
                        unsigned int real_interval = ((index < first_interval)?(interval+1000):interval);
                        this->set_timeout(real_interval);
                    }
                    default:
                    break;
                }
                        
                unsigned int now_time = (unsigned int)cort_timer_now_ms();
                if(index == 0 && type <= 1){
                    if(start_time != 0){ //We may be delayed and we need to fix.
                        now_time = (unsigned int)cort_timer_refresh_clock();
                        int fix_count = (int)(((now_time - start_time) / 1000.0) * req_count ) - real_cort_count; 
                        while(fix_count-- > 0){
                            (new T())->cort_start();
                        }
                    }
                    start_time = (unsigned int)cort_timer_refresh_clock(); 
                    real_cort_count = 0;
                }
                switch(type){
                    case 0:{                            
                        if(now_time - last_time > 200){
                            last_time = now_time;
                            index = 0;
                            break;//We faced some blocking operation. So we skip one time.
                        }
                        last_time = now_time;
                        unsigned int real_count = ((index < first_interval_count)?(interval_count+1):interval_count);
                        index = (index + 1)%100;
                        for(unsigned int i = 0; i <real_count; ++i){
                            (new T())->cort_start();
                            ++real_cort_count;
                        }
                    }
                    break;
                    case 1:{                            
                        index = (index + 1)%interval_count;
                        last_time = now_time;
                        (new T())->cort_start();
                        ++real_cort_count;
                    }
                    break;
                    case 1000:{                     
                        (new T())->cort_start();
                        ++real_cort_count;
                        index = (index + 1)%interval_count;
                    }
                    break;
                    default:
                    break;
                }
                CO_AGAIN;
            }
        CO_END
    }
    double req_count;
    unsigned int real_cort_count;
    unsigned int start_time; 
    unsigned int last_time; 
    unsigned int interval_count;
    unsigned int first_interval_count;
    
    unsigned int interval;
    unsigned int first_interval;

    unsigned short index;
    unsigned short type;
};


#endif