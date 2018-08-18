#ifndef CORT_STACKFUL_H_
#define CORT_STACKFUL_H_
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../cort_proto.h"
#include "../cort_util.h"

//If you want to change a stackless coroutine to stackful, you need to:
//1. Using CO_STACKFUL_DECL instead of CO_DECL in your coroutine class;
//2. Let cort_stackful or its subclass be the parent class of your coroutine class C(In another word, multi-heritage is needed.).
//3. You'd better use a stackless coroutine to await your stackful coroutine S(do not make it to be "no-root"),
//   or else it is difficult to recycle the stack itself.
//   This is because in the coroutine code of the S, it can not recycle the stack(delete or free also requires stack!!!).
//   Another alternative method is use stack pool to store its stack and free later(use set_stack to mention the stack is only weak referenced by S).
//   Remember, both methods require other to recycle its stack.
    
#define CO_STACKFUL_DECL(...) \
    CO_DECL_STACKFUL_PROTO(__VA_ARGS__) \
    static cort_proto* cort_start_static(cort_proto* that){ \
        return ((cort_type*)that)->CO_EXPAND(CO_GET_2ND_ARG(__VA_ARGS__,start))(); \
    }   \
    inline cort_proto* cort_start() {before_stackful_start(); \
        set_callback_function(&cort_resume_static); /*callback_function must be cort_resume_static*/ \
        return stackful_start(this, &cort_start_static);} \
        //stackful_start may return from cort_stackful_switch, so here we can not guarantee the return value must be zero.
    
#define CO_DECL_STACKFUL_PROTO(...) \
    CO_DECL_PROTO(__VA_ARGS__) \
    typedef CO_EXPAND(CO_GET_1ST_ARG(__VA_ARGS__)) cort_stackful_type;  \
    static cort_proto* cort_resume_static(cort_proto* that){ \
        that->set_callback_function(&cort_resume_static); \
        return ((cort_stackful_type*)that)->stackful_resume();  \
        /*stackful_resume may return from stackless_resume, or stackful entry function returns*/                           \
    }

extern "C"{
    cort_proto* cort_stackful_start(cort_proto* cort, void* src_sp_addr, size_t dest_sp_value, cort_proto::run_type func_address);
    cort_proto* cort_stackful_switch(void* src_sp_addr, void* dest_sp_addr, cort_proto* return_result);
}

//When a stackful coroutine x(and it is also a stackless coroutine, like cort_stackful_fds_waiter) wants to wait a stackless coroutine y:
//if(x->await(y) != 0) cort_stackful_yield(x);

//Or it only wants to yield then 
//cort_stackful_yield(x);
//If stackful_and_stackless_cort == 0 then the coroutine will be finished and the sources of the caller will be leaked.
//So we disable the behaviour.
template <typename T>
void cort_stackful_yield(T* stackful_and_stackless_cort){  
    if(stackful_and_stackless_cort){
        stackful_and_stackless_cort->before_stackless_resume(); 
        stackful_and_stackless_cort->stackless_resume(stackful_and_stackless_cort); //It must return from stackful_resume.
        stackful_and_stackless_cort->after_stackful_resume();
    }
}

struct cort_stackful_local_storage_meta{
    int32_t offset;
    int32_t version; //Maybe the dynamic library load or unload repeatedly so that offset need to be reused.

    cort_stackful_local_storage_meta(){
        cort_stackful_local_storage_meta::cort_stackful_local_storage_register();
    }
    ~cort_stackful_local_storage_meta(){
        cort_stackful_local_storage_meta::cort_stackful_local_storage_unregister();
    }
    void cort_stackful_local_storage_register();
    void cort_stackful_local_storage_unregister();
    
private:
    cort_stackful_local_storage_meta(const cort_stackful_local_storage_meta&);
    cort_stackful_local_storage_meta& operator = (const cort_stackful_local_storage_meta&);
};

struct cort_stackful_local_storage_data;
    
struct cort_stackful{ 
    static const uint32_t default_stack_size = 24*1024; 
    void* cort_stackless_sp_addr;
    void* cort_stackful_sp_addr;
    char* stack_base;
    uint32_t stack_size;
    uint16_t reserved_count;
    bool is_strong_ref;
    
    cort_stackful_local_storage_data* cort_stackful_local_storage;
    
    cort_proto* stackful_start(cort_proto* cort, cort_proto::run_type func_address);
    
    cort_proto* stackless_resume(cort_proto* return_value){ //When the coroutines yields, it always switches to the stackless one.
        return cort_stackful_switch(&cort_stackful_sp_addr, &cort_stackless_sp_addr, return_value);
        //It returns from stackful_resume.
    }
    
    cort_proto* stackful_resume(){ //When the coroutine resumes, it always switches to the stackful one.
        return cort_stackful_switch(&cort_stackless_sp_addr, &cort_stackful_sp_addr, 0); 
        //It may return from stackless_resume or never returns.
    }
    
    //即将启动有栈协程
    inline void before_stackful_start(){}   //You can overload the function in your subclass. It runs in usual stack.
    
    //已经切换到回一个有栈协程（第一次启动时不会调用这个）
    inline void after_stackful_resume(){}   //You can overload the function in your subclass. It runs in stackful-coroutine's own stack.
    
    //即将切换回一个无栈协程（注意此时有栈协程可能是结束了，也可能只是暂时想yield）
    inline void before_stackless_resume(){} //You can overload the function in your subclass. It runs in stackful-coroutine's own stack.
    
    //weak reference
    void set_stack(char* stack_ptr, uint32_t stack_total_size){
        if(is_strong_ref){
            free(stack_base);
            is_strong_ref = false;
        }
        stack_base = stack_ptr;
        stack_size = stack_total_size;
    }
    
    //strong reference
    void alloc_stack(uint32_t stack_total_size = default_stack_size){
        stack_size = stack_total_size; 
        if(is_strong_ref){
            stack_base = (char*)realloc(stack_base, stack_total_size);
        }else{
            stack_base = (char*)malloc(stack_total_size);
            is_strong_ref = true;
        }
    }

    void* get_local_storage(const cort_stackful_local_storage_meta& meta_data, const void* init_value_address);
    
    static cort_stackful* get_current_thread_cort();
    
    static void set_current_thread_cort(cort_stackful*);
    
    cort_stackful(uint32_t init_stack_size = default_stack_size){
        stack_base = 0;
        stack_size = 0;
        reserved_count = 0;
        is_strong_ref = false;
        cort_stackful_local_storage = 0;
    }
    
    ~cort_stackful();
private:
    cort_stackful(const cort_stackful&);
};                                                                  

template <typename T, bool size_test = (sizeof(T) <= sizeof(void*)), 
    bool class_test = cort_is_class_or_union<T>::result > //It must be small size(not long double) and not class.
struct co_local;

template <typename T>
struct co_local<T, true, false> : public cort_stackful_local_storage_meta{
    union{
        T init_value;
        void* padding;
    }init_value;
    
    co_local() : cort_stackful_local_storage_meta(){
        init_value.init_value = 0;
    }
    
    explicit co_local(T init_value_arg) : cort_stackful_local_storage_meta(){
        init_value.init_value = init_value_arg;
    }
    
    operator T&() const {
        return *(T*)(cort_stackful::get_current_thread_cort()->get_local_storage(*this, &init_value.init_value));
    }
    
    T& operator = (T arg) const{
        T& result = *(T*)(cort_stackful::get_current_thread_cort()->get_local_storage(*this, &init_value.init_value));
        result = arg;
        return result;
    }
    
    T& operator()() const {
        return *(T*)(cort_stackful::get_current_thread_cort()->get_local_storage(*this, &init_value.init_value));
    }
};

#endif