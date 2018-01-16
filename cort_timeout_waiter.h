#ifndef CORT_TIME_LIMITED_H_
#define CORT_TIME_LIMITED_H_

#include <stdint.h>
#include "cort_proto.h"

struct cort_timeout_waiter_data;

//cort_timeout_waiter A must be leaf coroutine. Because if you await other coroutine, time out limit may be disobeyed.
struct cort_timeout_waiter : public cort_proto{
    typedef uint64_t time_ms_t;
    typedef int64_t signed_time_ms_t;
    
    void resume_on_timeout();
    
    //resume_on_stop should be called by the scheduler when
    //you want to stop the thread, so you need to stop all the leaf coroutines(then they will resume their parents).
    //Then cort_timer_destroy can lead all the cort_timeout_waiter stop one bye one.
    void resume_on_stop();
    
private:
    union{
        time_ms_t start_time_ms;
        signed_time_ms_t time_cost_ms;
    }data_time;
    cort_timeout_waiter_data* that;
    cort_timeout_waiter(const cort_timeout_waiter&);
    const static time_ms_t timeout_masker = (1ull<<(sizeof(data_time.time_cost_ms)*8 - 1));
    const static time_ms_t stopped_masker = (1ull<<(sizeof(data_time.time_cost_ms)*8 - 2));

public:
    time_ms_t get_time_cost() const {
        return data_time.start_time_ms & ((~(timeout_masker - 1))&(~(stopped_masker - 1)));
    }
    
    inline bool is_timeout() const{
        return data_time.time_cost_ms < 0;
    }
    
    inline bool is_stopped() const{
        return (data_time.start_time_ms & stopped_masker) != 0;
    }
    
    cort_proto* on_finish();
    void set_timeout(time_ms_t timeout_ms);
    void clear_timeout();
    void clear();
    cort_timeout_waiter();
protected:
    time_ms_t get_time_past() const; 
    ~cort_timeout_waiter();
};

//This should be a leaf coroutine. It can only resumed by outer controler
//Because when your fd is ready, you have to react before next poll.
struct cort_fd_waiter : public cort_timeout_waiter{
    int cort_fd;
    uint32_t poll_result;
public:
    cort_fd_waiter(){
        cort_fd = -1;
        poll_result = 0;
    }
    int set_poll_request(int arg_fd, uint32_t arg_poller_request);
    int reset_poll_request(uint32_t arg_poller_request);
    int remove_poll_request();
    
    void set_cort_fd(int fd){
        cort_fd = fd;
    }
    int get_cort_fd() const{
        return cort_fd;
    }
    uint32_t get_poll_result() const{
        return poll_result;
    }
    void resume_on_poll(uint32_t poll_event);
};

void cort_timer_init();
void cort_timer_destroy();

cort_timeout_waiter::time_ms_t cort_timer_refresh_clock();
cort_timeout_waiter::time_ms_t cort_timer_now_ms();

int cort_get_poll_fd();
int cort_timer_poll(cort_timeout_waiter::time_ms_t until_ms);
void cort_timer_loop();

#endif
