#ifndef CORT_TIME_LIMITED_H_
#define CORT_TIME_LIMITED_H_

#include <stdint.h>
#include "cort_proto.h"

struct cort_time_limited_data;

//This should be a leaf coroutine
struct cort_time_limited : public cort_proto{
    typedef uint64_t time_ms_t;
private:
    union{
        time_ms_t start_time_ms;
        struct{
            uint32_t time_cost_ms;
            uint32_t poll_event;
        }_;
    }data2;
    
    cort_time_limited_data* that;   
public:
    time_ms_t get_time_cost() const {
        return data2._.time_cost_ms;
    }
    
    //0: time out.
    bool is_timeout() const{
        return (data2._.poll_event == 0);
    }
    
    //stopped by signal or other controller
    bool is_stopped() const{
        return (data2._.poll_event == (uint32_t)-1);
    }
    
    void set_timeout(time_ms_t timeout_ms);
    void clear_timeout();
    void clear();
    void resume_on_timeout(time_ms_t now_time_ms);
    void resume_on_stop(time_ms_t now_time_ms);
    void resume_normal(time_ms_t now_time_ms, uint32_t poll_event);
    cort_time_limited();
    
protected:
    uint32_t get_poll_event() const{
        return data2._.poll_event;
    }
    ~cort_time_limited();
};


void cort_timer_init();
void cort_timer_destroy();

cort_time_limited::time_ms_t cort_timer_refresh_clock();
cort_time_limited::time_ms_t cort_timer_now_ms();

int cort_get_poll_fd();
int cort_timer_poll(cort_time_limited::time_ms_t until_ms);
void cort_timer_loop();

#endif
