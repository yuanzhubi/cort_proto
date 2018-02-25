#ifndef CORT_STACKFUL_H_
#define CORT_STACKFUL_H_
#include <stdlib.h>
#include <stdint.h>
#include "../cort_proto.h"

//If you want to change a stackless coroutine to stackful, you need to:
//1. Using CO_STACKFUL_DECL instead of CO_DECL in your coroutine class;
//2. Let cort_stackful or its subclass be the parent class of your coroutine class.
//3. You'd better use a stackless coroutine to await your stackful coroutine S(do not make it to be "no-root"),
//   or else it is difficult to recycle the stack itself.
//   This is because in the coroutine code of the S, it can not recycle the stack(delete or free also requires stack!!!).
//   Another alternative method is use stack pool to store its stack and free later(use set_stack to mention the stack is only weak referenced by S).
//   Remember, both methods require other to recycle its stack.
    
#define CO_STACKFUL_DECL(...) \
    CO_DECL_PROTO(__VA_ARGS__) \
    CO_DECL_STACKFUL_PROTO(__VA_ARGS__) \
    static cort_proto* cort_start_static(cort_proto* that){ \
        return ((cort_type*)that)->CO_EXPAND(CO_GET_2ND_ARG(__VA_ARGS__,start))(); \
    }   \
    inline cort_proto* cort_start() {before_stackful_start(); return stackful_start(this, &cort_start_static);} \

#define CO_DECL_STACKFUL_PROTO(...) \
    typedef CO_EXPAND(CO_GET_1ST_ARG(__VA_ARGS__)) cort_stackful_type;  \
    static cort_proto* cort_resume_static(cort_proto* that){ \
        return ((cort_stackful_type*)that)->stackful_resume();  \
    }   \
    cort_proto* stackless_resume(cort_proto* return_value) { \
        set_run_function(&cort_resume_static); \
        return cort_stackful::stackless_resume(return_value); \
    }   

extern "C"{
    cort_proto* cort_stackful_start(cort_proto* func_argument, void* src_sp_addr, size_t dest_sp_value, cort_proto::run_type func_address);
    cort_proto* cort_stackful_switch(void* src_sp_addr, void* dest_sp_addr, cort_proto* return_result);
}

template <typename T>
void cort_stackful_await(T* stackful_and_stackless_cort){
    stackful_and_stackless_cort->before_stackless_resume(); 
    stackful_and_stackless_cort->stackless_resume(stackful_and_stackless_cort);
    stackful_and_stackless_cort->after_stackful_resume();
}

template <typename T, typename D>
void cort_stackful_await(T* stackful_cort, D* stackless_cort){
    stackful_cort->before_stackless_resume(); 
    stackful_cort->stackless_resume(stackless_cort);
    stackful_cort->after_stackful_resume();
}


struct cort_stackful{ 
    static const uint32_t default_stack_size = 24*1024; 
    void* cort_stackless_sp_addr;
    void* cort_stackful_sp_addr;
    char* stack_base;
    uint32_t stack_size;
    uint16_t reserved_count;
    bool is_strong_ref;
    
    cort_proto* stackful_start(cort_proto* func_argument, cort_proto::run_type func_address);
    
    cort_proto* stackless_resume(cort_proto* return_value){
        return cort_stackful_switch(&cort_stackful_sp_addr, &cort_stackless_sp_addr, return_value);
    }
    cort_proto* stackful_resume(){
        return cort_stackful_switch(&cort_stackless_sp_addr, &cort_stackful_sp_addr, 0);
    }
    
    inline void before_stackful_start(){}   //You can overload the function in your subclass.
    inline void after_stackful_resume(){}   //You can overload the function in your subclass.
    inline void before_stackless_resume(){} //You can overload the function in your subclass.
    
    cort_stackful(uint32_t init_stack_size = default_stack_size){
        stack_base = 0;
        stack_size = 0;
        is_strong_ref = false;
    }
    
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
        }
        else{
            stack_base = (char*)malloc(stack_total_size);
            is_strong_ref = true;
        }
    }
    
    ~cort_stackful(){
        if(is_strong_ref){
            free(stack_base);
        }
    }
private:
    cort_stackful(const cort_stackful&);
};                                                                  

#endif