#ifndef CORT_PROTO_H_
#define CORT_PROTO_H_

#define CO_JOIN2(x,y) x##y 
#define CO_JOIN(x,y) CO_JOIN2(x,y)

#define CO_STATE_MAX_COUNT ((count_type)((1u<<(sizeof(count_type)*8)) - 1u)) // you can increase the number if your compiler affordable

#define CO_STATE_EVAL_COUNTER(counter) (sizeof(*counter((struct_int<CO_STATE_MAX_COUNT>*)0)) \
          - sizeof(*counter((void*)0)))
          
/*We can change the result of CO_STATE_EVAL_COUNTER if we use CO_STATE_INCREASE_COUNTER or CO_STATE_SET_COUNTER*/

#define CO_STATE_INCREASE_COUNTER(counter, delta)  static char (*counter(struct_int<CO_STATE_EVAL_COUNTER(counter) + 1>*))[CO_STATE_EVAL_COUNTER(counter) + sizeof(*counter((void*)0)) + (delta)] 

#define CO_STATE_SET_COUNTER(counter, value)  static char (*counter(struct_int<CO_STATE_EVAL_COUNTER(counter) + 1>*))[value + sizeof(*counter((void*)0))]

struct cort_proto{ 
    typedef cort_proto* (*run_type)(cort_proto*);
    typedef cort_proto base_type;

    void set_run_function(run_type arg){
        data0.run_function = arg;
    }
    
    void set_parent(cort_proto *arg){
        cort_parent = arg;
    }
    
    void set_wait_count(size_t wait_count){
        this->data1.wait_count = wait_count;
    }
    
    void incr_wait_count(size_t wait_count){
        this->data1.wait_count += wait_count;
    }
    
    //Only useful for some rare case. Its overload form is called ususally.
    //It behaves like a virtual function. So if you want to call cort_proto::start, 
    //you MUST call "init" function in subclass first to initialize the "virtual table pointer".
    cort_proto* start() {
        return (*(this->data0.run_function))(this);
    }
    
    //Sometimes outer codes want "this" coroutine to wait other coroutines that "this" does not know. Using the following function
    //Await can be only called after "this" coroutine is awaiting at least one coroutine now or CO_AWAIT_ANY.
    //It must be guranteed that "this" coroutine is alive when subcoroutine is finished.
    template<typename T>
    cort_proto* await(T* sub_cort){
        cort_proto* __the_sub_cort = sub_cort->start();
        if(__the_sub_cort != 0){
            __the_sub_cort->set_parent(this); 
            this->incr_wait_count(1); 
            return this; 
        }
    }
    
    void resume() {
        //We save the cort_parent before run_function. So you can "delete this" during run_function.
        cort_proto* cort_parent_save = cort_parent;
        if((*(this->data0.run_function))(this) == 0 && cort_parent_save != 0 && (--(cort_parent_save->data1.wait_count)) == 0){
            cort_parent_save->resume();
        }
    }
    
    void clear(){
        cort_parent = 0;
        data0.result_ptr = 0;
    }

private:
    union{
        run_type run_function;
        void* result_ptr;                   //Useful to save coroutine result
        size_t result_int;                  //Useful to save coroutine result
    }data0;
    union{
        size_t wait_count;
        void* result_ptr;                   //Useful to save coroutine result
        size_t result_int;                  //Useful to save coroutine result
    }data1;
    cort_proto* cort_parent;

public: //Following members should be protected, however the subclass defined in a function can not access them @vc2008
    inline cort_proto* on_finish(){
        return 0;
    }

    //Can be used before on_finish return
    void** get_data0(){
        return &data0.result_ptr;
    }

    //Can be used before on_finish return
    void** get_data1(){
        return &data1.result_ptr;
    }

protected:
    cort_proto(){
        //We do not use initialize list because we want to remove the initialize order limit.
        cort_parent = 0;
        data0.result_ptr = 0;
    }   
    ~cort_proto(){}                 //Only used as weak reference so public virtual destructor is not needed.
};                                                                  

#define CO_GET_1ST_ARG(x,...) x
#define CO_GET_2ND_ARG(x,y,...) y
 
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
#define CO_EXPAND( x ) x

#define CO_FOR_EACH(x, ...) \
    CO_EXPAND(CO_GET_NTH_ARG(__VA_ARGS__, CO_FE_9, CO_FE_8, CO_FE_7, CO_FE_6, CO_FE_5, CO_FE_4, CO_FE_3, CO_FE_2, CO_FE_1, CO_FE_0)(x, __VA_ARGS__))
    
// Now let us show an example.
struct cort_example : public cort_proto{

//First put all your context here as class member.
    //int run_times;

//Or you want to overload some member function
    //void clear(){}
    //~cort_example(){}
    //cort_example(){}

#define CO_DECL(cort_example) \
public: \
    typedef cort_example cort_type;\
    static base_type* start_static(cort_type *p) { return p->start();}                          \
    cort_type* init() {                                                                         \
        set_run_function((run_type)(&start_static));           \
        return  this;                                                                           \
    } 

    //Now define you enter function. 
    //Anyway, you can define it in another file or out of class, leaving only declaration but not implement here.
    cort_proto* start(){

#define CO_BEGIN \
    struct cort_start_impl{\
        struct __cort_state_struct{ void dummy(){ \
        CORT_NEXT_STATE(__cort_begin)         
//Now you can define the coroutine function codes, using the class member as the local variable.


//In your codes, you may await with other sub-coroutines. Using following API.

//You can use CO_AWAIT to wait a sub-coroutine. It can not be used in any branch or loop.
#define CO_AWAIT(sub_cort) CO_AWAIT_IMPL(sub_cort, CO_JOIN(CO_STATE_NAME, __LINE__))

//You can use CO_AWAIT_ALL to wait no more than 10 sub-coroutine. It can not be used in any branch or loop.
#define CO_AWAIT_ALL(...) do{ \
        size_t current_wait_count = 0; \
        CO_FOR_EACH(CO_AWAIT_MULTI_IMPL, __VA_ARGS__) \
        if(current_wait_count != 0){ \
            this->set_wait_count(current_wait_count); \
            this->set_run_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::do_exec_static)); \
            return this; \
        } \
        CO_GOTO_NEXT_STATE; \
    }while(false);\
    CO_ENTER_NEXT_STATE \
    CORT_NEXT_STATE(CO_JOIN(CO_STATE_NAME, __LINE__))
    
//You can use CO_AWAIT_RANGE to wait variate count of sub-coroutine between the forward iterators.
//It can not be used in any branch or loop.
#define CO_AWAIT_RANGE(sub_cort_begin, sub_cort_end) do{ \
        if(cort_wait_range(this, sub_cort_begin, sub_cort_end) != 0){ \
            this->set_run_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::do_exec_static)); \
            return this; \
        } \
        CO_GOTO_NEXT_STATE; \
    }while(false); \
    CO_ENTER_NEXT_STATE \
    CORT_NEXT_STATE(CO_JOIN(CO_STATE_NAME, __LINE__))

//After wait in CO_AWAIT_BACK finished, it will not turn to next action but current action begin. It behaves like a loop. It can not be used in any branch or loop.
#define CO_AWAIT_BACK(sub_cort) do{ \
        cort_proto* __the_sub_cort = (sub_cort)->start();\
        if(__the_sub_cort != 0){\
            __the_sub_cort->set_parent(this); \
            this->set_wait_count(1); \
            this->set_run_function((run_type)(&this_type::do_exec_static)); \
            return this; \
        }\
        goto ____action_begin; \
    }while(false); \
    CO_ENTER_NEXT_STATE \
    CORT_NEXT_STATE(CO_JOIN(CO_STATE_NAME, __LINE__))

//Sometimes you want to stop the couroutine after a sub_coroutine is finished. Using CO_AWAIT_RETURN.
//It must be used in a branch or loop, or else it must be followed by CO_END.
#define CO_AWAIT_RETURN(sub_cort) \
    do{ \
        base_type* __the_sub_cort = (sub_cort)->start();\
        if(__the_sub_cort != 0){\
            /*__the_sub_cort->set_parent(this->cort_parent); \
            Above codes is not needed */ \
            return __the_sub_cort; \
        }\
        CO_RETURN; \
    }while(false); 
    
//Sometimes you want to exit from the couroutine. Using CO_RETURN. It can be used anywhere.
#define CO_RETURN \
    return ((cort_type*)this)->on_finish() \
    
//Sometimes you want to execute current action again later. Using CO_AGAIN(). It can be used anywhere.
#define CO_AGAIN do{ \
        this->set_run_function((run_type)(&this_type::do_exec_static));  \
        return this; \
    }while(false)

//You can skip next await action using CO_GOTO_NEXT_STATE
//It can not skip CO_AWAIT_RETURN.
#define CO_GOTO_NEXT_STATE goto ____action_end


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

#define CO_AWAIT_BACK_IF(bool_exp, sub_cort) \
    if(!(bool_exp)){CO_GOTO_NEXT_STATE; } \
    CO_AWAIT_BACK(sub_cort)


//Implement
#define CO_AWAIT_MULTI_IMPL(sub_cort) {\
    CO_AWAIT_MULTI_IMPL_IMPL(this, sub_cort) \
}

#define CO_AWAIT_MULTI_IMPL_IMPL(this_ptr, sub_cort) {\
    cort_proto *__the_sub_cort = (sub_cort)->start(); \
    if(__the_sub_cort != 0){ \
        __the_sub_cort->set_parent(this_ptr); \
        ++current_wait_count; \
    }\
}

#define CO_AWAIT_IMPL(sub_cort, cort_state_name) \
    do{ \
        base_type* __the_sub_cort = (sub_cort)->start();\
        if(__the_sub_cort != 0){\
            __the_sub_cort->set_parent(this); \
            this->set_wait_count(1); \
            this->set_run_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::do_exec_static)); \
            return this; \
        }\
        goto ____action_end; \
    }while(false); \
    CO_ENTER_NEXT_STATE \
    CORT_NEXT_STATE(cort_state_name)

#define CO_ENTER_NEXT_STATE ____action_end: return  ((CO_JOIN(CO_STATE_NAME, __LINE__)*)(this))->do_exec();

#define CORT_NEXT_STATE(cort_state_name) \
    }};struct cort_state_name : public cort_type {                       \
        typedef cort_state_name this_type;                               \
        static base_type* do_exec_static(cort_proto* this_ptr){return ((this_type*)(this_ptr))->do_exec();}\
        inline base_type* do_exec() { goto ____action_begin; ____action_begin: 




//Sometimes you know you have to pause but you do not know why and when you can continue. 
//Using CO_AWAIT_ANY(), others will use cort_proto::await to tell you what you should wait.
//This is a useful interface for "Dependency Inversion": it enable the coroutine set 
//the resume condition after pause.
#define CO_AWAIT_ANY() do{ \
        this->set_wait_count(0); \
        this->set_run_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::do_exec_static)); \
    }while(false); \
    return this;   \
    CO_ENTER_NEXT_STATE \
    CORT_NEXT_STATE(CO_JOIN(CO_STATE_NAME, __LINE__))
                                                                                        
#define CO_END  CO_RETURN; }}; }; \
    return ((cort_start_impl::__cort_begin*)this)->do_exec();
    
    return 0;}
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

#endif