#ifndef CORT_TIME_LIMITED_H_
#define CORT_TIME_LIMITED_H_

#include <stdint.h>
#include "../cort_proto.h"

#if (defined(__x86_64__) || defined(__i386__)) && defined(__linux__)
#define CO_USE_RDTSC //using rdtsc to get clock

#endif

struct cort_timeout_waiter_data;

//cort_timeout_waiter 是一个可以设置超时的协程。
//假设他子类的协程函数里this->set_timeout(5); 然后CO_YIELD; 5ms后他就能从CO_YIELD中resume回来。
//然而通常他必须是一个叶子协程，在协程函数里并不能等待别的协程，只能调用CO_YIELD， CO_AGAIN这类函数来进行暂停操作。
//因为如果他等待别的协程X，在X完成之前，他就可能超时了。此时X是否还应继续执行呢？
//如果是，那么超时设置就没有意义了；如果否，那么X还必须提供一个取消执行的接口（类似于异常处理）。这产生了极强的耦合和开发难度。
//综上，我们认为cort_timeout_waiter必须是一个叶子协程。
//同理，一个普通协程想要睡眠5ms的方法即是建立一个cort_timeout_waiter并且设置它超时时间为5ms，并CO_AWAIT等待它。
//cort_timeout_waiter背后是一个时间堆进行管理，并对相同的超时时间进行了聚合优化。这个时间堆每个线程各自拥有，而且使用各自的epoll fd来做调度等待。
//当你销毁时间堆时，里面注册的所有cort_timeout_waiter都会停止等待并resume。
//cort_timeout_waiter 管理精度是毫秒。但是epoll本身的精度在小于4ms的时候并不太可靠。
//cort_timeout_waiter 提供了一个32位的引用计数来进行管理他的生命周期。

//cort_timeout_waiter must be leaf coroutine. Because if you await other coroutine, time out limit may be disobeyed.
//But you can await "yourself" via CO_SELF_AWAIT.
struct cort_timeout_waiter : public cort_proto{
    typedef uint64_t time_ms_t;
    typedef uint32_t time_cost_ms_t;
    
    //已经超时，resume当前协程
    void resume_on_timeout();
    
    // cort_timer_destroy can lead all the cort_timeout_waiter with timeout greater than zero stopped one bye one.
    //时间堆已经被销毁，resume当前协程。或者被手工结束
    void resume_on_stop();
    
private:    
    cort_timeout_waiter(const cort_timeout_waiter&);
 
protected: 
    const static time_cost_ms_t timeout_masker = (((time_cost_ms_t)1)<<(sizeof(time_cost_ms_t)*8 - 1));
    const static time_cost_ms_t stopped_masker = (((time_cost_ms_t)1)<<(sizeof(time_cost_ms_t)*8 - 2));
    const static time_cost_ms_t normal_masker = timeout_masker | stopped_masker;
    

public:
    const static time_ms_t no_time_out = (time_ms_t)(-1);
    //pimpl mode
    cort_timeout_waiter_data* that;
    cort_timeout_waiter **ref_data;
    
    //start timestamp
    time_ms_t start_time_ms;
    //time cost when on_finished is called.
    time_cost_ms_t time_cost_ms;
    
    uint32_t ref_count;
    uint32_t get_time_cost() const {
        return (time_cost_ms & (~normal_masker));
    }
    
    bool is_timeout_or_stopped() const{
        return (time_cost_ms & normal_masker) != 0;
    }
    
    bool is_timeout() const{
        return (time_cost_ms & timeout_masker) != 0;
    }
    
    bool is_stopped() const{
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
    
    //获取最终超时的毫秒时间戳
    //return no_time_out if timeout is not set.
    time_ms_t get_timeout_time() const;
    
    //获取当前协程已经自设置超时时间后运行了多久了
    uint32_t get_time_past() const;     

    
    //设置timeout_ms后超时
    void set_timeout(time_ms_t timeout_ms);
    
    //清除超时设置。on_finish函数中会自动调用。所以通常你不需要手工调用。
    void clear_timeout();
    
    cort_timeout_waiter();
    cort_timeout_waiter(time_ms_t timeout_ms);
    cort_proto* on_finish();
    virtual void clear(); 
    virtual ~cort_timeout_waiter();
};

//我们还针对可被epoll的文件句柄封装了协程cort_fd_waiter。这个句柄 IO操作暂时不可用时，可以对这个句柄所属的cort_fd_waiter协程set_poll_request去监视他的可用性。
//通常，你还应该同时设置这个监视的超时时间。
//综上，cort_fd_waiter可能因为以下3个原因resume.
//1. 超时了（resume_on_timeout）
//2. 文件句柄的io可用了（resume_on_poll）
//3. 时间堆被销毁了（resume_on_stop）
//如同cort_timeout_waiter一样，他也应该只能是一个叶子协程。
//This should be a leaf coroutine. It can only resumed by outer controler.
//Because when your fd is ready, it must resume before next poll.
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
    
    //设置poll请求，参数是epoll中定义的事件
    int set_poll_request(uint32_t arg_poll_request);
    //移除poll请求
    int remove_poll_request();
    //获取监听的事件
    uint32_t get_poll_request() const {
        return poll_request;
    }
    
    //关闭被监听的fd
    void close_cort_fd();
    //移除被监听的fd，和他的poll请求
    void remove_cort_fd();
    
    //设置监听fd
    void set_cort_fd(int fd){
        cort_fd = fd;
    }
    //获取被监听的fd
    int get_cort_fd() const{
        return cort_fd;
    }
    
    
    //获取监听的结果
    uint32_t get_poll_result() const{
        return poll_result;
    }
    //清除监听的结果
    void clear_poll_result() {
        poll_result = 0;
    }
    //设置监听的结果
    void set_poll_result(uint32_t new_poll_result) {
        poll_result = new_poll_result;
    }
    
    //fd发生了关注的事件并resume
    void resume_on_poll(uint32_t poll_event);
    
    void resume_on_stop();
    
    //获取当前线程有多少个fd在被监视
    static uint32_t cort_waited_fd_count_thread();
};

//Usual program stages:
//初始化时间堆
//1. Call cort_timer_init to prepare for the timers.
int cort_timer_init();

//启动epoll时间循环。需要所有的cort_timeout_waiter停止，所有的监视fd被移除或者关闭后才能返回。
//2. After your cort_timeout_waiter start, call cort_timer_loop.
void cort_timer_loop();

//销毁时间堆
//3. The above cort_timer_loop returns when every cort_timeout_waiter is finished, then call cort_timer_destroy for recycle. 
void cort_timer_destroy();

//1,2,3 steps can be called in every thread.


//当前的毫秒时间戳
cort_timeout_waiter::time_ms_t cort_timer_now_ms();

//毫秒时钟默认只在每个epoll之后才刷新，你可以用cort_timer_now_ms_refresh手工重置
cort_timeout_waiter::time_ms_t cort_timer_now_ms_refresh();

//获取当前线程epoll fd
int cort_get_poll_fd();


//CO_SLEEP(timeout_ms) 可以让你当前协程睡timeout_ms毫秒
#define CO_SLEEP(timeout_ms) CO_AWAIT(new cort_sleeper(timeout_ms))
#define CO_SLEEP_IF(bool_exp, timeout_ms) CO_AWAIT_IF(bool_exp, new cort_sleeper(timeout_ms))
#define CO_SLEEP_AGAIN(timeout_ms) CO_AWAIT_AGAIN(new cort_sleeper(timeout_ms))
#define CO_SLEEP_AGAIN_IF(bool_exp, timeout_ms) CO_AWAIT_AGAIN_IF(bool_exp, new cort_sleeper(timeout_ms))

struct cort_sleeper : public cort_timeout_waiter{
    CO_DECL(cort_sleeper)
    cort_sleeper(time_ms_t timeout_ms = 0){set_timeout(timeout_ms);}
    cort_proto* start(){
        CO_BEGIN
            CO_YIELD();
            //Now it must be timeout
        CO_END
    }
protected:
    cort_proto* on_finish(){
        delete this;
        return 0;
    }
};

//cort_repeater 用来每秒重复创建N个T类型的协程并执行之。适合执行定时任务，压测等。
template<typename T>
struct cort_repeater : public cort_timeout_waiter{
    CO_DECL(cort_repeater)
    cort_repeater(double count = 1.0){
        set_repeat_per_second(count);
    }
    void set_repeat_per_second(double count){
        req_count = count;
        if(req_count <= 10.0){
            interval = (cort_timeout_waiter::time_ms_t)(1000/req_count);
            req_per_interval = 1;
        }else if(req_count <= 100.0){
            interval = 100;
            req_per_interval = (req_count/10);
        }else{
            interval = 10;
            req_per_interval = (req_count/100);
        }
        counter = 0;
    }
    void stop(){
        clear_timeout();
    }
    
    static const unsigned int reset_times = 9;
    cort_proto* start(){
        counter = 0;
        start_time = 0;
        CO_BEGIN
            if(!this->is_stopped()){
                this->set_timeout(interval);               
                if(req_count > 1.0){    
                    unsigned int this_time_count;
                    if(counter == 0){                       
                        cort_timeout_waiter::time_ms_t current_time = cort_timer_now_ms_refresh();
                        if(start_time != 0){
                            this_time_count = (unsigned int)((current_time - start_time ) * req_count / 1000.0 - ((unsigned int)req_per_interval) * (reset_times));
                        }else{
                            this_time_count = 0;
                        }
                        counter = 1;
                        start_time = current_time;
                    }else{ 
                        this_time_count = (unsigned int)req_per_interval;
                        if(counter != reset_times){                           
                            ++counter;
                        }else{
                            counter = 0;
                        }
                    }
                    while(this_time_count != 0){
                        --this_time_count;
                        (new T())->start();
                    }
                }else{
                    (new T())->start();
                }
                CO_AGAIN;
            }
            this->stop();
        CO_END
    }
    double req_count;
    
    cort_timeout_waiter::time_ms_t start_time; 
    cort_timeout_waiter::time_ms_t interval;
    
    unsigned int counter; 
    double req_per_interval;
};

//我们对cort_timeout_waiter或他们的子类封装一个COM式的引用计数智能指针
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

#endif
