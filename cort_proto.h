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

    void set_run_function(run_type arg){
        data0.run_function = arg;
    }
    
    void set_parent(cort_proto *arg){
        cort_parent = arg;
    }
    
    cort_proto* start() {
        return (*(this->data0.run_function))(this);
    }
    
    void resume() {
        if((*(this->data0.run_function))(this) == 0 && cort_parent != 0){
             cort_parent->resume();
        }
    }
    
    void clear(){
        cort_parent = 0;
    }
    
protected:
    union{
        run_type run_function;

        void* result_ptr;                   //Useful to save coroutine result
        int result_int;                     //Useful to save coroutine result
    }data0;
    
    cort_proto* cort_parent;
    
    cort_proto():cort_parent(0){}   
    
    ~cort_proto(){}                 //only used as weak reference so public virtual destructor is not needed.
    
    inline cort_proto* on_finish(){
        return 0;
    }
};                                                                 

struct cort_multi_awaitable : public cort_proto {
public: 

    void clear(){
        data1.next_cort = (0);
        data2.prev_cort = (0);
        cort_proto::clear();
    }

    
    template <typename T>
    friend cort_proto* wait_range(cort_proto* this_ptr, T begin_forward_iterator, T end_forward_iterator);
    
protected:
    cort_multi_awaitable(){
        data1.next_cort = 0;
        data2.prev_cort = 0;
    }

    union{
        cort_multi_awaitable* next_cort;                //Input
        void* extend_info;                  //Useful for subclass(transmit some result to parent cort after on_finish)
    }data1;
    
    union{
        cort_multi_awaitable* prev_cort;                //Input
        void* extend_info;                  //Useful for subclass(transmit some result to parent cort after on_finish)
    }data2;

    void push_back(cort_multi_awaitable* rhs){
        this->data1.next_cort = rhs;
        rhs->data2.prev_cort = this;
    }
    void pop_back(){
        this->data1.next_cort = 0;
    }
    
    cort_proto* on_finish(){
        int all_zero = 0;           //XOR is quick
        if(data2.prev_cort != 0){
            data2.prev_cort->data1.next_cort = data1.next_cort;
            all_zero += 1;          //INCR is quick
        }
        if(data1.next_cort != 0){
            data1.next_cort->data2.prev_cort = data2.prev_cort;
            all_zero += 1;          //INCR is quick

        }
        if(all_zero == 0){          //TEST is quick
            return 0;
        }
        return this;
    }
};    

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

#define CO_FOR_EACH(x, ...) \
    CO_GET_NTH_ARG(__VA_ARGS__, CO_FE_9, CO_FE_8, CO_FE_7, CO_FE_6, CO_FE_5, CO_FE_4, CO_FE_3, CO_FE_2, CO_FE_1, CO_FE_0)(x, __VA_ARGS__)
    
    
// Now let us show an example.
struct cort_example : public cort_proto{
//If we use "struct cort_example : public cort_multi_awaitable", it means the cort_example can be waited parallely with other cort_multi_awaitable.

//First put all your context here as class member.
    //int run_times;

//Or you want to overload some member function
    //void clear(){}
    //~cort_example(){}

//The default constructor(that without any argument), or constructor_impl(if your compiler does not support delegate constructor), 
// must be called in your self-defined constructor function.
    //cort_example(int){;;;constructor_impl();}

//Second using CO_BEGIN to define some useful property
#define CO_BEGIN(cort_example) \
public: \
    typedef cort_example cort_type;\
    typedef cort_proto proto_type;\
    typedef unsigned char count_type;\
    template<typename TTT, unsigned int M>\
    struct cort_state_struct;\
    const static count_type nstate = (count_type)(-1);\
    void constructor_impl(){set_run_function((run_type)&cort_state_struct<cort_example, 0>::do_exec_static);} \
    cort_example(){\
        constructor_impl();                                                                     \
    }                                                                                           \
    template<count_type N, int M = 0>                                                           \
    struct struct_int : struct_int<N - 1, 0> {};                                                \
    template<int M>                                                                             \
    struct struct_int<0, M> {};                                                                 \
    static count_type (*first_counter(...))[1];                                                 \
    struct dummy{       void f(){                                                               \
    CORT_NEXT_STATE(cort_begin)                                                                 
    
#define CORT_NEXT_STATE(cort_state_name) \
    }};const static count_type cort_state_name = CO_STATE_EVAL_COUNTER(first_counter) ;         \
    CO_STATE_INCREASE_COUNTER(first_counter, 1);                                                \
    template<typename CORT_BASE>                                                                        \
    struct cort_state_struct<CORT_BASE, cort_state_name > : public CORT_BASE {                          \
        typedef cort_state_struct<CORT_BASE, cort_state_name > this_type;                               \
        const static count_type state_value = cort_state_name;                                          \
        static proto_type* do_exec_static(proto_type* this_ptr){return ((this_type*)(this_ptr))->do_exec();}\
        inline proto_type* do_exec() { goto ____action_begin; ____action_begin:
        
//Now you can define the coroutine function codes, using the class member as the local variable.

//You can use CO_AWAIT to wait a sub-coroutine. It can not be used in any branch or loop.
#define CO_AWAIT(sub_cort) CO_AWAIT_IMPL(sub_cort, CO_JOIN(CO_STATE_NAME, __LINE__), state_value + 1)

#define CO_AWAIT_ALL(...) do{ \
        cort_multi_awaitable* __tmp_cort = 0; \
        CO_FOR_EACH(CO_AWAIT_MULTI_IMPL, __VA_ARGS__) \
        if(__tmp_cort != 0){ \
            this->set_run_function(cort_state_struct<CORT_BASE, state_value + 1>::do_exec_static); \
            return this; \
        } \
    }while(false);\
    CO_GOTO_NEXT_STATE \
    CORT_NEXT_STATE(CO_JOIN(CO_STATE_NAME, __LINE__))
    
#define CO_AWAIT_RANGE(sub_cort_begin, sub_cort_end) do{ \
        if(wait_range(this, sub_cort_begin, sub_cort_end) != 0){ \
            this->set_run_function(cort_state_struct<CORT_BASE, state_value + 1>::do_exec_static); \
            return this; \
        } \
    }while(false); \
    CO_GOTO_NEXT_STATE \
    CORT_NEXT_STATE(CO_JOIN(CO_STATE_NAME, __LINE__))


//After wait finished, it will not turn to next state but current. It behaves like a loop. It can not be used in any branch or loop.
#define CO_AWAIT_BACK(sub_cort) do{ \
        proto_type* the_sub_cort = (sub_cort)->run();\
        if(the_sub_cort != 0){\
            the_sub_cort->set_parent(this); \
            this->set_run_function(cort_state_struct<CORT_BASE, state_value>::do_exec_static); \
            return this; \
        }\
    }while(false); \
    goto ____action_begin; \
    CORT_NEXT_STATE(CO_JOIN(CO_STATE_NAME, __LINE__))

#define CO_AWAIT_MULTI_IMPL(sub_cort) {\
    cort_multi_awaitable * __tmp_cort_new = sub_cort; \
    CO_AWAIT_MULTI_IMPL_IMPL(this, __tmp_cort, __tmp_cort_new) \
}
    
#define CO_AWAIT_MULTI_IMPL_IMPL(this_ptr, __tmp_cort, __tmp_cort_new) {\
    __tmp_cort_new->set_parent(this_ptr); \
    if(__tmp_cort != 0){ \
        __tmp_cort->push_back(__tmp_cort_new); \
        if(__tmp_cort_new->start() != 0){ \
            __tmp_cort = __tmp_cort_new; \
        }\
        else{ \
            __tmp_cort->pop_back(); \
        } \
    } \
    else { \
        if(__tmp_cort_new->start() != 0){ \
            __tmp_cort = __tmp_cort_new; \
        }\
    } \
}

#define CO_AWAIT_IMPL(sub_cort, cort_state_name, next_state) \
    do{ \
        proto_type* the_sub_cort = (sub_cort)->run();\
        if(the_sub_cort != 0){\
            the_sub_cort->set_parent(this); \
            this->set_run_function(cort_state_struct<CORT_BASE, next_state>::do_exec_static); \
            return this; \
        }\
    }while(false); \
    CO_GOTO_NEXT_STATE \
    CORT_NEXT_STATE(cort_state_name)

#define CO_GOTO_NEXT_STATE return ((cort_state_struct<CORT_BASE, state_value + 1>*)(this))->do_exec();

#define CO_AWAIT_AGAIN() do{ \
    this->set_run_function(cort_state_struct<CORT_BASE, state_value>::do_exec_static);  \
    return this; \
}while(false)

#define CO_AWAIT_IF(bool_exp, sub_cort) \
    if(!(bool_exp)){CO_GOTO_NEXT_STATE } \
    CO_AWAIT(sub_cort)  

#define CO_AWAIT_BACK_IF(bool_exp, sub_cort) \
    if(!(bool_exp)){CO_GOTO_NEXT_STATE } \
    CO_AWAIT_BACK(sub_cort)


//Sometimes you want to exit from the couroutine. Using CO_RETURN.
#define CO_RETURN() \
    return this->on_finish(); \

//Sometimes you want to stop the couroutine after a sub_coroutine is finished. Using CO_AWAIT_RETURN.
//It must be used in a branch or loop, or else it must be followed by CO_END.
#define CO_AWAIT_RETURN(sub_cort) \
    do{ \
        proto_type* the_sub_cort = (sub_cort)->run();\
        if(the_sub_cort != 0){\
            the_sub_cort->set_parent(this->cort_parent); \
            return the_sub_cort; \
        }\
        CO_RETURN(); \
    }while(false); 
                                                                                        
#define CO_END(cort_example) CO_RETURN(); }}; \
    const static count_type state_total_count = CO_STATE_EVAL_COUNTER(first_counter) ; 
}; 

template <typename T>
cort_proto* wait_range(cort_proto* this_ptr, T begin_forward_iterator, T end_forward_iterator){
    cort_multi_awaitable* tmp_cort = 0;
    while(begin_forward_iterator != end_forward_iterator){
        cort_multi_awaitable *tmp_cort_new = (*begin_forward_iterator); 
        CO_AWAIT_MULTI_IMPL_IMPL(this_ptr, tmp_cort, tmp_cort_new) 
        ++begin_forward_iterator;
    }
    return tmp_cort;
}


#endif