#ifndef CORT_PROTO_H_
#define CORT_PROTO_H_
#include <stdlib.h>

#define CO_JOIN2(x,y) x##y 
#define CO_JOIN(x,y) CO_JOIN2(x,y)

#if defined(__GNUC__)
#define CO_TYPE_OF __typeof__
#else
#define CO_TYPE_OF decltype
#endif

struct cort_proto{ 
    typedef cort_proto* (*run_type)(cort_proto*);
    typedef cort_proto base_type;

    void set_run_function(run_type arg){
        data0.run_function = arg;
    }
    
    void set_self_await_function(run_type arg){
        data1.leaf_cort_run_function = arg;
    }
    
    cort_proto* resume_self_await(){
        run_type arg = data1.leaf_cort_run_function;
        data1.leaf_cort_run_function = 0;
        return arg(this);
    }
    
    void set_parent(cort_proto *arg){
        cort_parent = arg;
    }
    cort_proto *get_parent() const{
        return cort_parent;
    }
    
    void set_wait_count(size_t wait_count){
        this->data1.wait_count = wait_count;
    }
    
    void incr_wait_count(size_t wait_count){
        this->data1.wait_count += wait_count;
    }
        
    //Sometimes outer codes want "this" coroutine to wait other coroutines that "this" does not know. Using the following function
    //Await can be only called after "this" coroutine is awaiting at least one coroutine now or CO_AWAIT_UNKNOWN.
    //It must be guranteed that "this" coroutine is alive when subcoroutine is finished.
    template<typename T>
    cort_proto* await(T* sub_cort){
        cort_proto* __the_sub_cort = sub_cort->cort_start();
        if(__the_sub_cort != 0){
            __the_sub_cort->set_parent(this); 
            this->incr_wait_count(1); 
            return this; 
        }
    }
    
    void resume() {
        //We save the cort_parent before run_function. So you can "delete this" in your subclass overloaded on_finish function after parent class on_finish function is called and return 0.
        cort_proto* cort_parent_save = cort_parent;
        if((*(this->data0.run_function))(this) == 0 && cort_parent_save != 0 && (--(cort_parent_save->data1.wait_count)) == 0){
                cort_parent_save->resume();
        }
    }
    
    void clear(){
    }

protected:
    union{
        run_type run_function;
        void* result_ptr;                   //Useful to save coroutine result
        size_t result_int;                  //Useful to save coroutine result
    }data0;
    union{
        size_t wait_count;
        run_type leaf_cort_run_function;
    }data1;
    cort_proto* cort_parent;

public: //Following members should be protected, however the subclass defined in a function can not access them @vc2008
    inline void on_finish(){
        cort_parent = 0;
        data0.run_function = 0;
    }
    
    bool is_finished() const{
        return data0.run_function == 0;
    }

protected:
    cort_proto(){
        //We do not use initialize list because we want to remove the initialize order limit.
        cort_parent = 0;
        data0.run_function = 0;
        data1.leaf_cort_run_function = 0;
    }   
    ~cort_proto(){}                 //Only used as weak reference so public virtual destructor is not needed.
};                                                                  

struct cort_virtual : public cort_proto{
    virtual cort_proto* cort_start(){return 0;};
    virtual ~cort_virtual(){}
};

struct cort_auto_delete : public cort_virtual{
    void on_finish(){
        delete this;
    }
};

#define CO_GET_1ST_ARG(x,...) x
#define CO_GET_2ND_ARG(x,y,...) y
#define CO_GET_3RD_ARG(x,y,z,...) z
 
#define CO_GET_NTH_ARG(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, N, ...) N

#define CO_FE_0(co_call, x, ...) co_call(x)
#define CO_FE_1(co_call, x, ...) co_call(x) CO_FE_0(co_call, __VA_ARGS__)
#define CO_FE_2(co_call, x, ...) co_call(x) CO_FE_1(co_call, __VA_ARGS__)
#define CO_FE_3(co_call, x, ...) co_call(x) CO_FE_2(co_call, __VA_ARGS__)
#define CO_FE_4(co_call, x, ...) co_call(x) CO_FE_3(co_call, __VA_ARGS__)
#define CO_FE_5(co_call, x, ...) co_call(x) CO_FE_4(co_call, __VA_ARGS__)
#define CO_FE_6(co_call, x, ...) co_call(x) CO_FE_5(co_call, __VA_ARGS__)
#define CO_FE_7(co_call, x, ...) co_call(x) CO_FE_6(co_call, __VA_ARGS__)
#define CO_FE_8(co_call, x, ...) co_call(x) CO_FE_7(co_call, __VA_ARGS__)
#define CO_FE_9(co_call, x, ...) co_call(x) CO_FE_8(co_call, __VA_ARGS__)

#define CO_ARG_0(co_call, x, ...) co_call(x)
#define CO_ARG_1(co_call, x, ...) CO_ARG_0(co_call, __VA_ARGS__)
#define CO_ARG_2(co_call, x, ...) CO_ARG_1(co_call, __VA_ARGS__)
#define CO_ARG_3(co_call, x, ...) CO_ARG_2(co_call, __VA_ARGS__)
#define CO_ARG_4(co_call, x, ...) CO_ARG_3(co_call, __VA_ARGS__)
#define CO_ARG_5(co_call, x, ...) CO_ARG_4(co_call, __VA_ARGS__)
#define CO_ARG_6(co_call, x, ...) CO_ARG_5(co_call, __VA_ARGS__)
#define CO_ARG_7(co_call, x, ...) CO_ARG_6(co_call, __VA_ARGS__)
#define CO_ARG_8(co_call, x, ...) CO_ARG_7(co_call, __VA_ARGS__)
#define CO_ARG_9(co_call, x, ...) CO_ARG_8(co_call, __VA_ARGS__)

#define CO_EXPAND( x ) x

#define CO_ECHO(x) x

#define CO_FOR_EACH(x, ...) \
    CO_EXPAND(CO_GET_NTH_ARG(__VA_ARGS__, CO_FE_9, CO_FE_8, CO_FE_7, CO_FE_6, CO_FE_5, CO_FE_4, CO_FE_3, CO_FE_2, CO_FE_1, CO_FE_0)(x, __VA_ARGS__))
    
#define CO_GET_LAST_ARG(...)  \
    CO_GET_NTH_ARG(__VA_ARGS__, CO_ARG_9, CO_ARG_8, CO_ARG_7, CO_ARG_6, CO_ARG_5, CO_ARG_4, CO_ARG_3, CO_ARG_2, CO_ARG_1, CO_ARG_0)(CO_ECHO, __VA_ARGS__)
    
// Now let us show an example.
struct cort_example : public cort_proto{

//First put all your context here as class member.
    //int run_times;

//Or you want to overload some member function
    //void clear(){}
    //~cort_example(){}
    //cort_example(){}

    //CO_DECL(cort_example) to declare this is a coroutine class
    //or CO_DECL(cort_example, new_start) to use "new_start"  instead default "start" as the coroutine entrance function
#define CO_DECL(...) \
public: \
    CO_DECL_PROTO(__VA_ARGS__) \
    inline base_type* cort_start() { return this->CO_EXPAND(CO_GET_2ND_ARG(__VA_ARGS__,start))();/*coroutine function name is start for default*/}                      

//If you this class is not defined as the coroutine entrance, you can CO_DECL_PROTO instead to avoid defines a enter function.
#define CO_DECL_PROTO(...)  typedef CO_EXPAND(CO_GET_1ST_ARG(__VA_ARGS__)) cort_type;  

    //Now define you enter function. 
    //Anyway, you can define it in another file or out of class, leaving only declaration but not implement here.
    //cort_proto* start(){

#define CO_BEGIN \
    struct cort_start_impl{\
        struct __cort_begin; \
        static __cort_begin* get_begin_ptr(cort_type* ptr){ return (__cort_begin*)ptr;} \
        struct __cort_state_struct{ void dummy(){ \
        CORT_NEXT_STATE(__cort_begin)         
//Now you can define the coroutine function codes, using the class member as the local variable.

//In your codes, you may await with other sub-coroutines. Using following API.
//You can await a coroutine x only if x->is_finished() is true.

//You can use CO_AWAIT to wait a sub-coroutine. It can not be used in any branch or loop.
#define CO_AWAIT(...) CO_AWAIT_IMPL(CO_EXPAND(CO_GET_1ST_ARG(__VA_ARGS__)), CO_EXPAND(CO_GET_2ND_ARG(__VA_ARGS__, cort_start)), CO_JOIN(CO_STATE_NAME, __LINE__))

//You can use CO_AWAIT_ALL to wait no more than 10 sub-coroutine. It can not be used in any branch or loop.
#define CO_AWAIT_ALL(...) do{ \
        size_t current_wait_count = 0; \
        CO_FOR_EACH(CO_AWAIT_MULTI_IMPL, __VA_ARGS__) \
        if(current_wait_count != 0){ \
            this->set_wait_count(current_wait_count); \
            this->set_run_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::do_exec_static)); \
            return this; \
        } \
    }while(false);\
    CO_NEXT_STATE
    
//You can use CO_AWAIT_RANGE to wait variate count of sub-coroutine between the forward iterators.
//It can not be used in any branch or loop.
#define CO_AWAIT_RANGE(sub_cort_begin, sub_cort_end) do{ \
        if(cort_wait_range(this, sub_cort_begin, sub_cort_end) != 0){ \
            this->set_run_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::do_exec_static)); \
            return this; \
        } \
    }while(false); \
   CO_NEXT_STATE

//After wait in CO_AWAIT_AGAIN finished, it will not turn to next action but current action begin. It behaves like a loop. It can not be used in any branch or loop.
#define CO_AWAIT_AGAIN(sub_cort) do{ \
        cort_proto* __the_sub_cort = (sub_cort)->cort_start();\
        if(__the_sub_cort != 0){\
            __the_sub_cort->set_parent(this); \
            this->set_wait_count(1); \
            this->set_run_function((run_type)(&this_type::do_exec_static)); \
            return this; \
        }\
        goto ____action_begin; \
    }while(false); \
    CO_NEXT_STATE;
    
#define CO_NEXT_STATE \
    CO_GOTO_NEXT_STATE; \
    CO_ENTER_NEXT_STATE; \
    CORT_NEXT_STATE(CO_JOIN(CO_STATE_NAME, __LINE__)) 
    
//Sometimes you know you have to pause but you do not know who will resume you, or the scheduler will resume you. 
//Using CO_AWAIT_UNKNOWN(), other coroutines can use cort_proto::await to tell you what you should wait,
//or the schedulmer will resume you.
//This is a useful interface for "Dependency Inversion": it enable the coroutine set 
//the resume condition after pause.
#define CO_AWAIT_UNKNOWN() do{ \
        this->set_run_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::do_exec_static)); \
        return this;   \
    }while(false); \
    CORT_NEXT_STATE(CO_JOIN(CO_STATE_NAME, __LINE__))
    
#define CO_AWAIT_UNKNOWN_AGAIN() do{ \
        this->set_run_function((run_type)(&do_exec_static)); \
    }while(false); \
    return this;   \
    CORT_NEXT_STATE(CO_JOIN(CO_STATE_NAME, __LINE__))
    
#define CO_YIELD() CO_AWAIT_UNKNOWN()

#define CO_YIELD_AGAIN() CO_AWAIT_UNKNOWN_AGAIN()

//Sometimes you want to stop the couroutine after a sub_coroutine is finished. Using CO_AWAIT_RETURN.
//It must be used in a branch or loop, or else it must be followed by CO_END.
#define CO_AWAIT_RETURN(sub_cort) \
    do{ \
        base_type* __the_sub_cort = (sub_cort)->cort_start();\
        if(__the_sub_cort != 0){\
            /*__the_sub_cort->set_parent(this->cort_parent); \
            Above codes is not needed */ \
            return __the_sub_cort; \
        }\
    }while(false); \
    CO_RETURN   
    
//Sometimes you want to exit from the couroutine. Using CO_RETURN. It can be used anywhere in a coroutine non-static member function.
//If you want to skip on_finish(for example, you delete this in your coroutine function), using CO_EXIT
#define CO_RETURN do{ \
    this->on_finish(); \
    CO_EXIT;  \
}while(false)

//CO_EXIT will end the coroutine function except it does not call on_finish. So you should only use it for the coroutine without parent.
#define CO_EXIT return 0

//Maybe you have called another coroutine member function then you want to deliver the coroutine control to that function.
//Using CO_SWITCH
#define CO_SWITCH return this

//Only available for leaf coroutine return using CO_SELF_RETURN!!!! 
//More accurately,  in "member_func_name",
//1. you can not wait any coroutine (including CO_AWAIT(this) or CO_SELF_AWAIT other member function). 
//2. CO_YIELD, CO_AGAIN, CO_RETURN is enabled. CO_SLEEP and other API that "await" any coroutine is disabled.
//3. cort_proto::await is also disabled
#define CO_SELF_AWAIT(member_func_name) \
    do{ \
        set_self_await_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::do_exec_static)); \
        return this->member_func_name();\
    }while(false); \
    CORT_NEXT_STATE(CO_JOIN(CO_STATE_NAME, __LINE__))

//If current function is waited by CO_SELF_AWAIT, using CO_SELF_RETURN instead of CO_RETURN to avoid on_finish called twice.
#define CO_SELF_RETURN \
    do{ \
        if(data1.leaf_cort_run_function == 0){ \
            CO_RETURN; \
        } \
        return resume_self_await(); \
    }while(false); 
    
//Sometimes you want to execute current action again later. Using CO_AGAIN(). It can be used anywhere in a coroutine non-static member function.
#define CO_AGAIN do{ \
        this->set_run_function((run_type)(&this_type::do_exec_static));  \
        return this; \
    }while(false)

//You can skip next await action using CO_SKIP_AWAIT
//It can not skip CO_AWAIT_RETURN.
#define CO_SKIP_AWAIT CO_GOTO_NEXT_STATE

//Following are conditional form API
#define CO_AWAIT_IF(bool_exp, sub_cort) \
    if(!(bool_exp)){CO_GOTO_NEXT_STATE; } \
    CO_AWAIT(sub_cort)  
    
#define CO_AWAIT_ALL_IF(bool_exp, ...) \
    if(!(bool_exp)){CO_GOTO_NEXT_STATE; } \
    CO_AWAIT_ALL(__VA_ARGS__)  
    
#define CO_AWAIT_RANGE_IF(bool_exp, sub_cort_begin, sub_cort_end) \
    if(!(bool_exp)){CO_GOTO_NEXT_STATE; } \
    CO_AWAIT_RANGE(sub_cort_begin, sub_cort_end)  

#define CO_AWAIT_AGAIN_IF(bool_exp, sub_cort) \
    if(!(bool_exp)){CO_GOTO_NEXT_STATE; } \
    CO_AWAIT_AGAIN(sub_cort)

#define CO_AWAIT_UNKNOWN_IF(bool_exp) \
    if(!(bool_exp)){CO_GOTO_NEXT_STATE; } \
    CO_AWAIT_UNKNOWN()
    
#define CO_YIELD_IF(bool_exp) CO_AWAIT_UNKNOWN_IF(bool_exp)

#define CO_AWAIT_UNKNOWN_AGAIN_IF(bool_exp) \
    if(!(bool_exp)){CO_GOTO_NEXT_STATE; } \
    CO_AWAIT_UNKNOWN_AGAIN()

#define CO_YIELD_AGAIN_IF(bool_exp) CO_AWAIT_UNKNOWN_AGAIN_IF(bool_exp)

//Implement
#define CO_AWAIT_MULTI_IMPL(sub_cort) {\
    CO_AWAIT_MULTI_IMPL_IMPL(this, sub_cort) \
}

#define CO_AWAIT_MULTI_IMPL_IMPL(this_ptr, sub_cort) {\
    cort_proto *__the_sub_cort = (sub_cort)->cort_start(); \
    if(__the_sub_cort != 0){ \
        __the_sub_cort->set_parent(this_ptr); \
        ++current_wait_count; \
    }\
}

#define CO_AWAIT_IMPL(sub_cort, func_name, cort_state_name) \
    do{ \
        base_type* __the_sub_cort = (sub_cort)->func_name();\
        if(__the_sub_cort != 0){\
            __the_sub_cort->set_parent(this); \
            this->set_wait_count(1); \
            this->set_run_function((run_type)(&cort_state_name::do_exec_static)); \
            return this; \
        }\
        goto ____action_end; \
    }while(false); \
    CO_ENTER_NEXT_STATE; \
    CORT_NEXT_STATE(cort_state_name)

#define CO_GOTO_NEXT_STATE goto ____action_end;

#define CO_ENTER_NEXT_STATE ____action_end: return  ((CO_JOIN(CO_STATE_NAME, __LINE__)*)(this))->do_exec();

#define CORT_NEXT_STATE(cort_state_name) \
    }};struct cort_state_name : public cort_type {                       \
        typedef cort_state_name this_type;                               \
        static base_type* do_exec_static(cort_proto* this_ptr){return ((this_type*)(this_ptr))->do_exec();}\
        inline base_type* do_exec() { goto ____action_begin; ____action_begin:
                                                                                        
#define CO_END  CO_RETURN; }}; }; \
    return cort_start_impl::get_begin_ptr(this)->do_exec(); 
};

#include <iterator>
template <typename T>
size_t cort_wait_range(cort_proto* this_ptr, T begin_forward_iterator, T end_forward_iterator){
    size_t current_wait_count = 0;
    while(begin_forward_iterator != end_forward_iterator){
        typename std::iterator_traits<T>::value_type tmp_cort_new = (*begin_forward_iterator); 
        CO_AWAIT_MULTI_IMPL_IMPL(this_ptr, tmp_cort_new) 
        ++begin_forward_iterator;
    }
    this_ptr->set_wait_count(current_wait_count);
    return current_wait_count;
}

template<typename T>
T* cort_set_parent(T* son, cort_proto* parent = 0){
    son->set_parent(parent);
    return son;
}

//CO_ASYNC will async call a coroutine.
//First argument is the coroutine, the second is the enter function name(or your coroutine enter function name, if there exist only one argument).
//The called coroutine should maintain its lifetime itself.
//Usually you can CO_ASYNC a coroutine x only if x->is_finished() is true.
//CO_ASYNC can be used anywhere. If the coroutine is not awaited before, it is equal to direct call "cort->cort_start()".
#define CO_ASYNC(...) \
    cort_set_parent(CO_EXPAND(CO_GET_1ST_ARG(__VA_ARGS__)))->CO_EXPAND(CO_GET_2ND_ARG(__VA_ARGS__, cort_start))()

#define CO_GET_2ND_DEFAULT(x, ...) CO_GET_NTH_ARG(__VA_ARGS__, CO_FE_9, CO_FE_8, CO_FE_7, CO_FE_6, CO_FE_5, CO_FE_4, CO_FE_3, CO_FE_2, CO_GET_1ST_ARG, CO_GET_3RD_ARG)(x, __VA_ARGS__)

//CO_ASYNC_THEN will async call a coroutine x, then call its member function x->func() after it is finished.
//First argument is the coroutine, the last is the "then" function name(func, for example). 
//If there are three arguments, the enter function name is the second; else is your defined coroutine enter function.
#define CO_ASYNC_THEN(...) do{\
    typedef CO_TYPEOF((CO_EXPAND(CO_GET_1ST_ARG(__VA_ARGS__)))) __the_sub_cort_type; \
    struct CO_JOIN(CO_LOCAL_CORT_NAME, __LINE__) : public cort_proto{ \
            __the_sub_cort_type *__the_sub_cort; \
            CO_DECL(CO_JOIN(CO_LOCAL_CORT_NAME, __LINE__)) \
            cort_proto* start(){ \
                CO_BEGIN \
                    CO_AWAIT(__the_sub_cort, CO_EXPAND(CO_GET_2ND_DEFAULT(cort_start, __VA_ARGS__))); \
                    __the_sub_cort->CO_EXPAND(CO_GET_LAST_ARG(__VA_ARGS__)); \
                    delete this; \
                    CO_EXIT; /*avoid on_finish calling*/\
                CO_END \
            } \
        }; CO_JOIN(CO_LOCAL_CORT_NAME, __LINE__) *__the_sub_cort_waiter = new CO_JOIN(CO_LOCAL_CORT_NAME, __LINE__)(); \
    __the_sub_cort_waiter->__the_sub_cort = CO_EXPAND(CO_GET_1ST_ARG(__VA_ARGS__)); \
    __the_sub_cort_waiter->start();  \
}while(false)

#endif
