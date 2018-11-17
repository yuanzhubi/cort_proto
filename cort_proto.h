#ifndef CORT_PROTO_H_
#define CORT_PROTO_H_
#include <stdlib.h>
#include <iterator>

#define CO_JOIN2(x,y) x##y 
#define CO_JOIN(x,y) CO_JOIN2(x,y)

#if defined(__GNUC__)
#define CO_TYPE_OF __typeof__
#else //C++ 0x or above is required
#define CO_TYPE_OF decltype
#endif

#define CO_AWAITABLE                        //Declare a function result is awaitable.
#define CO_THIS_AWAITABLE CO_AWAITABLE      //Declare a member function result is awaitable in another coroutine member function of current class or its subclass.

struct cort_proto;

struct cort_base{
    //trivial but virtual destructor
    virtual ~cort_base(){}
    
    cort_proto* cort_parent;                //The parent coroutine that waits this coroutine.
    void remove_parent(){
        cort_parent = 0;
    }
    void set_parent(cort_proto *arg){
        cort_parent = arg;
    }
    cort_proto *get_parent() const{
        return cort_parent;
    }   
};

struct cort_proto : public cort_base{
public:
    typedef cort_proto* (*run_type)(cort_proto*);

protected:    
    size_t cort_wait_count;             //Count of the coroutines that waited by this coroutine.
     
    union{                              
        run_type callback_function;     //The callback function executed when the waited subcoroutines are all finished.
        size_t object_count;            //Used to save count of the resumed waiters of cort_channel.
        cort_proto* last_resumer_cort;  //Used to save resumer.
    }data0;     
    
    union{
        run_type cort_then_function;    //The callback function when this coroutine finished. It maybe zero.
        size_t rest_wait_count;         //Used to save wait count of cort_wait_n after it resumed its parent
        size_t pinned_object_count;     //Used to save count of produced and locked objects of cort_channel_proto.
        size_t ref_count;               //reference count.
        void* pdata;                    //Used for p-impl pattern.
    }data10; 

public:  
    enum{is_auto_delete = false};       //When the coroutine finished, it will not "delete this".
    
    cort_proto(){
        //We do not use initialize list because we want to remove the initialize order limit for the c++ compiler.
        cort_parent = 0;
        data0.callback_function = 0;
        cort_wait_count = 0;
        data10.cort_then_function = 0;
    }

    //So the cort_proto coroutine will cost (4+1)*sizeof(void*) bytes, 4 data meber, 1 virtual function table pointer.    

    run_type get_callback_function() const {
        return data0.callback_function;
    }
    void set_callback_function(run_type arg){
        data0.callback_function = arg;
    }

    cort_proto* get_last_resumer() const{
        return data0.last_resumer_cort;
    }
    void set_last_resumer(cort_proto* arg){
        data0.last_resumer_cort = arg;
    }
    
    size_t get_wait_count() const{
        return this->cort_wait_count;
    }
    void set_wait_count(size_t wait_count){
        this->cort_wait_count = wait_count;
    }
    void incr_wait_count(size_t wait_count){
        this->cort_wait_count += wait_count;
    }
    size_t decr_wait_count(size_t wait_count){
        return this->cort_wait_count -= wait_count;
    }
        
    //await another coroutine not started.
    template<typename T>
    cort_proto* await(T* sub_cort){
        cort_proto* __wait_result_cort = sub_cort->cort_start();
        if(__wait_result_cort != 0){
            __wait_result_cort->set_parent(this); 
            this->incr_wait_count(1); 
            return this; 
        }
        return 0;
    }
    
    //await another coroutine started yet.
    cort_proto* until(cort_proto* sub_cort){
        if(sub_cort != 0){
            sub_cort->set_parent(this); 
            this->incr_wait_count(1);
        }
        return sub_cort;
    }
    
    //on_finish will be called when this coroutine ended. 
    //As a coroutine is most ended in the subclass member function, it is designed not to be a virtual function.
    cort_proto* on_finish(){
        run_type func = then();
        if(func != 0 ){
            then(0);
            return func(this);
        }
        return 0;
    }
    
    //Sometimes you need to reuse this coroutine, or add it to a "coroutine pool".
    //You can call the "clear" function to clear or reset the coroutine states.
    //You can rewrite the function for your subclass.
    virtual void clear(){}
       
    //If T is a local coroutine class, c++11 needed because c++03 disabled local class to be the template argument.
    //corts[0].then<T>;
    template<typename T>
    void then(){
        data10.cort_then_function = T::cort_start_static;
    }
    
    void then(run_type then_function){
        data10.cort_then_function = then_function;
    }
    run_type then() const {
        return data10.cort_then_function;
    }

    //When all the coroutines awaited by this coroutine finished, this coroutine will be resumed by member function "resume".
    //last_resumer_cort is the coroutine that generates the calling.
    void resume(cort_proto* last_resumer_cort = 0) {
        //We save the cort_parent before callback_function. So you can "delete this" in your callback_function.
        cort_proto* that = this;
        cort_proto* cort_parent_save;
        run_type callback_function;
        do {
            cort_parent_save = that->get_parent();       //we save it because that may be deleted in callback_function
            callback_function = that->get_callback_function();
            that->set_last_resumer(last_resumer_cort);
        }while((callback_function == 0 || callback_function(that) == 0) && 
            ((last_resumer_cort = that, that = cort_parent_save) != 0) && 
            (cort_parent_save->decr_wait_count(1) == 0));
    }
    
    void try_resume(cort_proto* resumer){
        if(this->decr_wait_count(1) == 0){
            this->resume(resumer);
        }
    }
};                                                                  

template<typename T>
struct cort_auto_delete : public T{
    enum{is_auto_delete = true}; 
    cort_proto* on_finish(){
        cort_proto* result = T::on_finish();
        if(result == 0 && !(T::is_auto_delete)){
            delete this;
        }
        return result;
    }
};

typedef cort_auto_delete<cort_proto> cort_auto;

#define CO_GET_1ST_ARG(x,...) x
#define CO_GET_2ND_ARG(x,y,...) y
#define CO_PARAMS(...) __VA_ARGS__
#define CO_REMOVE_PARENTHESIS(x) CO_EXPAND(CO_PARAMS x)

#define CO_EXPAND(x) x
#define CO_EMPTY_EXPAND(...)
#define CO_COMMA_GEN(...) ,

#define CO_REMOVE_TWO_IMPL(x,y,...) __VA_ARGS__
#define CO_REMOVE_TWO(...)  CO_EXPAND(CO_GET_NTH_ARG(__VA_ARGS__, \
    CO_REMOVE_TWO_IMPL, CO_REMOVE_TWO_IMPL, CO_REMOVE_TWO_IMPL, CO_REMOVE_TWO_IMPL, CO_REMOVE_TWO_IMPL,\
    CO_REMOVE_TWO_IMPL, CO_REMOVE_TWO_IMPL, CO_REMOVE_TWO_IMPL, CO_REMOVE_TWO_IMPL,  \
    CO_EMPTY_EXPAND, CO_EMPTY_EXPAND, CO_EMPTY_EXPAND))(__VA_ARGS__)



//We add a "ignored_data" because visual C++ compiler can not "perfect forward" empty __VA_ARGS__.
#define CO_FE_0(...) 
#define CO_FE_1(op_sep, sep, op_data, ignored_data, x, ...) op_data(x) 
#define CO_FE_2(op_sep, sep, op_data, ignored_data, x, ...) op_data(x) op_sep(sep) CO_EXPAND(CO_FE_1(op_sep, sep, op_data, ignored_data, ##__VA_ARGS__))
#define CO_FE_3(op_sep, sep, op_data, ignored_data, x, ...) op_data(x) op_sep(sep) CO_EXPAND(CO_FE_2(op_sep, sep, op_data, ignored_data, ##__VA_ARGS__))
#define CO_FE_4(op_sep, sep, op_data, ignored_data, x, ...) op_data(x) op_sep(sep) CO_EXPAND(CO_FE_3(op_sep, sep, op_data, ignored_data, ##__VA_ARGS__))
#define CO_FE_5(op_sep, sep, op_data, ignored_data, x, ...) op_data(x) op_sep(sep) CO_EXPAND(CO_FE_4(op_sep, sep, op_data, ignored_data, ##__VA_ARGS__))
#define CO_FE_6(op_sep, sep, op_data, ignored_data, x, ...) op_data(x) op_sep(sep) CO_EXPAND(CO_FE_5(op_sep, sep, op_data, ignored_data, ##__VA_ARGS__))
#define CO_FE_7(op_sep, sep, op_data, ignored_data, x, ...) op_data(x) op_sep(sep) CO_EXPAND(CO_FE_6(op_sep, sep, op_data, ignored_data, ##__VA_ARGS__))
#define CO_FE_8(op_sep, sep, op_data, ignored_data, x, ...) op_data(x) op_sep(sep) CO_EXPAND(CO_FE_7(op_sep, sep, op_data, ignored_data, ##__VA_ARGS__))
#define CO_FE_9(op_sep, sep, op_data, ignored_data, x, ...) op_data(x) op_sep(sep) CO_EXPAND(CO_FE_8(op_sep, sep, op_data, ignored_data, ##__VA_ARGS__))
#define CO_FE_10(op_sep, sep, op_data, ignored_data, x, ...) op_data(x) op_sep(sep) CO_EXPAND(CO_FE_9(op_sep, sep, op_data, ignored_data, ##__VA_ARGS__))

#define CO_GET_NTH_ARG( _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10,  N, ...) N

#define CO_FOR_EACH_IMPL(op_sep, op_data, sep, ...) CO_EXPAND(CO_GET_NTH_ARG(__VA_ARGS__, CO_FE_10, CO_FE_9, CO_FE_8, CO_FE_7, \
    CO_FE_6, CO_FE_5, CO_FE_4, CO_FE_3, CO_FE_2, CO_FE_1, CO_FE_0)(op_sep, sep, op_data, ##__VA_ARGS__))

#define CO_FOR_EACH(op_data, ...) CO_FOR_EACH_IMPL(CO_EMPTY_EXPAND, op_data, EMPTY_SEPERATOR, IGNORED_ARG, ##__VA_ARGS__)

#define CO_FOR_EACH_COMMA(op_data, ...) CO_FOR_EACH_IMPL(CO_COMMA_GEN, op_data, EMPTY_SEPERATOR, IGNORED_ARG, ##__VA_ARGS__)

#define CO_FOR_EACH_SEP(op_data, sep, ...) CO_FOR_EACH_IMPL(CO_EXPAND, op_data, sep, IGNORED_ARG, ##__VA_ARGS__)
        
#define CO_COUNT_INC(x) +1
#define CO_ARG_COUNT(...) (0 CO_FOR_EACH(CO_COUNT_INC, __VA_ARGS__))
    
// Now let us show an example according to following class cort_example.
// Coroutie class should be public subclass of cort_proto
//struct cort_example : public cort_proto{

//First put all your context variable(with life cycle acrossing a callback proccess) here as class members. Like temporary local virable definition in C language.
    //int run_times;

//Then you can overload or define some member functions. The definition can be put outside of the class
    //void clear(){}
    //~cort_example(){}
    //cort_example();

    //Next you can use CO_DECL to declare your class is a coroutine class and the default coroutine entry function name.
    //Attention your coroutine class can have some more coroutine entry functions.

    //CO_DECL(cort_example) to declare this is a coroutine class with "start" as the default coroutine entry function.
    //or CO_DECL(cort_example, new_start) to use "new_start"  instead default "start" as the default coroutine entry function.
#define CO_DECL(...) \
public: \
    CO_DECL_PROTO(__VA_ARGS__) \
    /*coroutine function name is start for default*/ \
    static cort_proto* cort_start_static(cort_proto* this_ptr){ \
        return ((cort_type*)this_ptr)->CO_EXPAND(CO_GET_2ND_ARG(__VA_ARGS__,start))();}\
    cort_proto* cort_start() { return this->CO_EXPAND(CO_GET_2ND_ARG(__VA_ARGS__,start))();} \
    cort_proto* cort_start(cort_proto* &echo_ptr) {echo_ptr = this;  \
        return this->CO_EXPAND(CO_GET_2ND_ARG(__VA_ARGS__,start))();}

//If you do not want to declare the defualt entry function name, using CO_DECL_PROTO instead.
#define CO_DECL_PROTO(...)  typedef CO_EXPAND(CO_GET_1ST_ARG(__VA_ARGS__)) cort_type;

    //Now declare or define your defualt entry function. 
    //Anyway, you can define it in another file or out of class, leaving only declaration here.
    //You can define more than 1 entry functions(not only the default entry).
    //It must return cort_proto* and with no arguments.

//Suppose the function has name "start".
    //cort_proto* start(){
//The function should begin with "CO_BEGIN", end with "CO_END". The return type should be cort_proto*.
#define CO_BEGIN \
    typedef cort_type cort_local_type; \
    struct cort_start_impl{\
        typedef struct cort_state_struct{ void dummy(){ \
        CORT_NEXT_STATE(cort_begin_type)

//The codes between CO_BEGIN and CO_END are your coroutine function codes. You can use the class member variable as the function local variable.
//We will introduce some macro interfaces for the coroutine function codes.


//In your coroutine codes, you may await with other coroutines. Using following macros. 
//Do not use them outside the coroutine function. 

//You can use CO_AWAIT to wait the finish of another coroutine X.
//First argument is the address of X, or a smart pointer! We only need support of "->" operation of X.
//Second argument is the entry function name of X. It is optional and will use the name declared in CO_DECL if you do not provide the argument.
//If you provide the entry function name, then you can provide the arguments of the function as the rest argument of CO_AWAIT. 
//It can not be used in any branch or loop.
#define CO_AWAIT(...) CO_AWAIT_IMPL(CO_EXPAND(CO_GET_1ST_ARG(__VA_ARGS__)), \
    CO_EXPAND(CO_GET_2ND_ARG(__VA_ARGS__, cort_start)), (CO_REMOVE_TWO(__VA_ARGS__)))


//You can use CO_AWAIT_ALL to wait no more than 10 sub-coroutine. 
//The arguments are the address or smart pointer of X. 
//Only their default entry function declared in CO_DECL can be used. You can not specify other entry function name.
//It can not be used in any branch or loop.
#define CO_AWAIT_ALL(...) do{ \
        size_t current_wait_count = 0; \
        CO_FOR_EACH(CO_AWAIT_MULTI_IMPL, __VA_ARGS__) \
        if(current_wait_count != 0){ \
            this->set_wait_count(current_wait_count); \
            this->set_callback_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::start_static)); \
            return this; \
        } \
    }while(false);\
    CO_NEXT_STATE

//You can use CO_AWAIT_RANGE to wait variate number of coroutines between two forward iterators.
//The arguments are the two iterators of the range and its value_type should be the address or smart pointers.
//Only their default entry function declared in CO_DECL can be used. You can not specify other entry function name.
//It can not be used in any branch or loop.
#define CO_AWAIT_RANGE(sub_cort_begin, sub_cort_end) do{ \
        if(cort_wait_range(this, sub_cort_begin, sub_cort_end) != 0){ \
            this->set_callback_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::start_static)); \
            return this; \
        } \
    }while(false); \
    CO_NEXT_STATE

//Await finish of any n coroutines from the coroutine arguments.
//Only their default entry function declared in CO_DECL can be used. You can not specify other entry function name.
//It can not be used in any branch or loop.
#define CO_AWAIT_ANY_N(n, ...) do{ \
        size_t __current_finished_count = 0; \
        size_t __current_waited_count = 0; \
        const size_t __max_count = (size_t)(n); \
        cort_wait_n *__wait_any_cort = new cort_wait_n(); \
        CO_FOR_EACH(CO_AWAIT_ANY_IMPL, __VA_ARGS__) \
        /*(__current_waited_count + __current_finished_count) is the total count */ \
        __wait_any_cort->init_resume_any(__current_waited_count,  __max_count - __current_finished_count); \
        __wait_any_cort->start(); \
        __wait_any_cort->set_parent(this); \
        this->set_callback_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::start_static)); \
        return this; \
    }while(false);\
    CO_NEXT_STATE

//Await finish of any n coroutines from the coroutine in the range between sub_cort_begin and sub_cort_end.
//Only their default entry function declared in CO_DECL can be used.  You can not specify other entry function name.
//It can not be used in any branch or loop.
#define CO_AWAIT_RANGE_ANY_N(n, sub_cort_begin, sub_cort_end) do{ \
        if(cort_wait_range_any(this, sub_cort_begin, sub_cort_end, n) != 0){ \
            this->set_callback_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::start_static)); \
            return this; \
        } \
    }while(false);\
    CO_NEXT_STATE

//Await finish of any coroutine from the coroutine arguments.
//Only their default entry function declared in CO_DECL can be used. You can not specify other entry function name.
//It can not be used in any branch or loop.
#define CO_AWAIT_ANY(...) CO_AWAIT_ANY_N(1, __VA_ARGS__)

//Await finish of any coroutine from the coroutine in the range between sub_cort_begin and sub_cort_end.
//Only their default entry function declared in CO_DECL can be used.
//It can not be used in any branch or loop.
#define CO_AWAIT_RANGE_ANY(sub_cort_begin, sub_cort_end) CO_AWAIT_RANGE_ANY_N(1, sub_cort_begin, sub_cort_end)


//Above macro interfaces can not be used in a loop body or a conditional branch, due to the C++ limit.
//So we provides some adapter iterfaces to simulate the loop(backward jump) or branch jump(forward jump).

//Definition: we name the first code line after a CO_BEGIN or CO_AWAIT or CO_AWAIT_X, "resume point".
//The resume points divide the function body into different "states".

//After wait in CO_AWAIT_AGAIN finished, it will not turn to next resume point as CO_AWAIT but previous one. It behaves like a loop.
//The argument of CO_AWAIT_AGAIN is same as CO_AWAIT.

#define CO_AWAIT_AGAIN(...) do{ \
        cort_proto* __wait_result_cort = (CO_EXPAND(CO_GET_1ST_ARG(__VA_ARGS__)))->CO_EXPAND(CO_GET_2ND_ARG(__VA_ARGS__, cort_start))();\
        if(__wait_result_cort != 0){\
            __wait_result_cort->set_parent(this); \
            this->set_wait_count(1); \
            this->set_callback_function((run_type)(&cort_this_type::start_static)); \
            return this; \
        }\
    }while(false); \
    return start_static(this);

#define CO_AWAIT_ALL_AGAIN(...) do{ \
        size_t current_wait_count = 0; \
        CO_FOR_EACH(CO_AWAIT_MULTI_IMPL, __VA_ARGS__) \
        if(current_wait_count != 0){ \
            this->set_wait_count(current_wait_count); \
            this->set_callback_function((run_type)(&cort_this_type::start_static)); \
            return this; \
        } \
    }while(false);\
    return start_static(this);

//Sometimes you know you will await some coroutine but you do not know who it is. Or current coroutine is a leaf coroutine and should be resumed manually.
//Using CO_AWAIT_UNKNOWN(), other coroutines can later use cort_proto::await to tell current one what it should wait.
//This is a useful interface for "Dependency Inversion": it enables setting the resume condition of the coroutine after its pause.
//It still can not be used in any branch or loop.
#define CO_AWAIT_UNKNOWN() do{ \
        this->set_callback_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::start_static)); \
        return this;   \
    }while(false); \
    CO_NEXT_STATE

#define CO_AWAIT_UNKNOWN_AGAIN() do{ \
        this->set_callback_function((run_type)(&cort_this_type::start_static)); \
        return this; \
    }while(false);

//Some other language use "yield" keyword to implement the "CO_AWAIT_UNKNOWN". So we provide a similiar interface name.
//So CO_YIELD or CO_YIELD_X also generates new resume point.
#define CO_YIELD() CO_AWAIT_UNKNOWN()
#define CO_YIELD_AGAIN() CO_AWAIT_UNKNOWN_AGAIN()


//Following are conditional form await interfaces, they will await only if the bool_exp is true.
#define CO_AWAIT_IF(bool_exp, ...) \
    if(!(bool_exp)){CO_SKIP_AWAIT; } \
    CO_AWAIT(__VA_ARGS__)

#define CO_AWAIT_ALL_IF(bool_exp, ...) \
    if(!(bool_exp)){CO_SKIP_AWAIT; } \
    CO_AWAIT_ALL(__VA_ARGS__)

#define CO_AWAIT_RANGE_IF(bool_exp, sub_cort_begin, sub_cort_end) \
    if(!(bool_exp)){CO_SKIP_AWAIT; } \
    CO_AWAIT_RANGE(sub_cort_begin, sub_cort_end)

#define CO_AWAIT_AGAIN_IF(bool_exp, ...) \
    if(bool_exp){CO_AWAIT_AGAIN(__VA_ARGS__); } \

#define CO_AWAIT_UNKNOWN_IF(bool_exp) \
    if(!(bool_exp)){CO_SKIP_AWAIT;} \
    CO_AWAIT_UNKNOWN()

#define CO_AWAIT_UNKNOWN_AGAIN_IF(bool_exp) \
    if(bool_exp){CO_AWAIT_UNKNOWN_AGAIN(); } \

#define CO_YIELD_IF(bool_exp) CO_AWAIT_UNKNOWN_IF(bool_exp)
#define CO_YIELD_AGAIN_IF(bool_exp)  CO_AWAIT_UNKNOWN_AGAIN_IF(bool_exp)


//CO_AWAIT(sub_cort) means: start sub_cort and current coroutine will await its finish.
//CO_UNTIL(sub_cort) means: sub_cort has started(before CO_UNTIL), current coroutine will await its finish.

#define CO_UNTIL(sub_cort) CO_YIELD_IF(this->until(sub_cort) != 0)

#define CO_UNTIL_IMPL(sub_cort) ((this->until(sub_cort) == 0) ? 1 : 0)
#define CO_UNTIL_ALL(...) CO_YIELD_IF((CO_FOR_EACH_SEP(CO_UNTIL_IMPL, +, __VA_ARGS__)) != (CO_ARG_COUNT(__VA_ARGS__)))
#define CO_UNTIL_ANY(...) CO_UNTIL_ANY_N(1, __VA_ARGS__)

#define CO_UNTIL_ANY_IMPL(sub_cort){   \
    cort_proto *__wait_result_cort = __wait_any_cort->until(sub_cort); \
    if(__wait_result_cort != 0){ \
        __wait_result_cort->set_parent(__wait_any_cort); \
        ++__current_waited_count; \
    }\
    else if(++__current_finished_count == __max_count){ \
        if(__current_waited_count != 0){ \
            __wait_any_cort->start(); \
        } \
        else{ \
            delete __wait_any_cort; \
        } \
        break; \
    } \
} \

#define CO_UNTIL_ANY_N(n, ...) do{ \
        size_t __current_finished_count = 0; \
        size_t __current_waited_count = 0; \
        const size_t __max_count = (size_t)(n); \
        cort_wait_n *__wait_any_cort = new cort_wait_n(); \
        CO_FOR_EACH(CO_UNTIL_ANY_IMPL, __VA_ARGS__) \
        /*(__current_waited_count + __current_finished_count) is the total count */ \
        __wait_any_cort->init_resume_any(__current_waited_count,  __max_count - __current_finished_count); \
        __wait_any_cort->start(); \
        __wait_any_cort->set_parent(this); \
        this->set_callback_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::start_static)); \
        return this; \
    }while(false);\
    CO_NEXT_STATE



//The control interfaces above can generate a "resume point" and following not.
//The control interfaces above should be used with brackets and following not.
//The control interfaces above can not used in loop/branch body or other "{}" in CO_BEGIN and CO_END and following can.



//Sometimes you want to exit from the current coroutine. Using CO_RETURN. 
//This is "normal" return, like codes running to CO_END and finished.
#define CO_RETURN return this->on_finish();

//CO_EXIT will exit like CO_RETURN but it does not call on_finish and then_function. 
#define CO_EXIT do{ \
        this->remove_parent(); \
        set_callback_function(0); \
        return 0; \
    }while(false)

//CO_AGAIN will behave like CO_YIELD but it does not generate a new resume point. Current coroutine will be resumed at current resume point.
//This is useful for delayed retry, like dealing errno "EAGAIN".
#define CO_AGAIN do{ \
        this->set_callback_function((run_type)(&cort_this_type::start_static));  \
        return this; \
    }while(false)

//CO_RESTART will restart the codes between CO_BEGIN and CO_END without calling on_finish and then_function
#define CO_RESTART return cort_start_impl::local_start(this);

//You can directly jump to next resume point by CO_SKIP_AWAIT or CO_NEXT
#define CO_SKIP_AWAIT goto ____action_end;
#define CO_NEXT CO_SKIP_AWAIT

//You can directly jump to previous resume point by CO_PREV
#define CO_PREV return ((cort_prev_type*)this)->local_start();




//Implement
#define CO_AWAIT_MULTI_IMPL(sub_cort) \
    CO_AWAIT_MULTI_IMPL_BINARY(this, sub_cort)

#define CO_AWAIT_MULTI_IMPL_BINARY(this_ptr, sub_cort) {\
    cort_proto* __wait_result_cort = (sub_cort)->cort_start(); \
    if(__wait_result_cort != 0){ \
        __wait_result_cort->set_parent(this_ptr); \
        ++current_wait_count; \
    }\
}

template <typename T>
size_t cort_wait_range(cort_proto* this_ptr, T begin_forward_iterator, T end_forward_iterator){
    size_t current_wait_count = 0;
    while(begin_forward_iterator != end_forward_iterator){
        typename std::iterator_traits<T>::value_type tmp_cort_new = (*begin_forward_iterator);
        CO_AWAIT_MULTI_IMPL_BINARY(this_ptr, tmp_cort_new)
        ++begin_forward_iterator;
    }
    this_ptr->set_wait_count(current_wait_count);
    return current_wait_count;
}

#define CO_AWAIT_IMPL(sub_cort, func_name, argument_list) \
    do{ \
        cort_proto* __wait_result_cort = (sub_cort)->func_name(CO_REMOVE_PARENTHESIS(argument_list));\
        if(__wait_result_cort != 0){\
            __wait_result_cort->set_parent(this); \
            this->set_wait_count(1); \
            this->set_callback_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::start_static)); \
            return this; \
        }\
    }while(false); \
    CO_NEXT_STATE

#define CO_NEXT_STATE \
    CO_ENTER_NEXT_STATE; \
    CORT_NEXT_STATE(CO_JOIN(CO_STATE_NAME, __LINE__))

#define CO_ENTER_NEXT_STATE  \
    goto ____action_end; ____action_end: return  ((CO_JOIN(CO_STATE_NAME, __LINE__)*)(this))->local_start();

#define CORT_NEXT_STATE(cort_state_name) \
    }} CO_JOIN(cort_state_name, _prev_type); \
    /* Function and class definition of previous state end!*/\
    \
    typedef struct cort_state_name : public cort_local_type {                 \
        typedef cort_state_name cort_this_type;                               \
        typedef CO_JOIN(cort_state_name, _prev_type) cort_prev_type;                            \
        static cort_proto* start_static(cort_proto* this_ptr){ \
            return ((cort_this_type*)(cort_prev_type*)(this_ptr))->local_start();} \
        cort_proto* local_start() { goto ____action_begin; ____action_begin:

#define CO_IF(co_bool_condition) \
            goto ____action_end; ____action_end:  \
            typedef CO_JOIN(cort_state_name_skip, __LINE__)::CO_JOIN(CO_STATE_NAME, __LINE__) skip_type; \
            if(co_bool_condition){ \
                return  ((skip_type*)(this))->inner_start(); \
            } \
            else{ \
                return  ((skip_type*)(this))->local_next_start(); \
            } \
        }}CO_JOIN(cort_state_name_prev_prev, __LINE__);  \
        /* Function and class definition of previous state end! */\
        /* We will use class CO_JOIN(cort_state_name_skip, __LINE__) to wrap the whole if else body!*/\
        typedef struct CO_JOIN(cort_state_name_skip, __LINE__){ \
            typedef struct CO_JOIN(CO_STATE_NAME, __LINE__) : public cort_local_type { \
                CO_DECL(CO_JOIN(CO_STATE_NAME, __LINE__), inner_start) \
                cort_proto* inner_start(){ \
                    CO_BEGIN \


#define CO_BRANCH_BEGIN_IMPL\
    }}CO_JOIN(cort_state_name_prev, __LINE__);  \
    \
    typedef struct CO_JOIN(CO_STATE_NAME, __LINE__) : public cort_local_type { \
        CO_DECL(CO_JOIN(CO_STATE_NAME, __LINE__), inner_start) \
        cort_proto* inner_start(){ \
            CO_BEGIN

#define CO_BRANCH_END_IMPL \
        goto ____action_end; ____action_end:  \
        return co_if_end(this); \
    }}cort_end_type; \
    /* Function and class definition of previous "if else" body end! */\
    static cort_proto* local_start(cort_type* ptr){ \
        return ((cort_begin_type*)(cort_end_type*)ptr)->local_start(); \
    } }; \
    return cort_start_impl::local_start(this); \
    } \

#define CO_IF_END  \
            CO_BRANCH_END_IMPL \
            cort_proto* local_next_start(){ \
                return co_if_end(this); \
            } \
        } CO_JOIN(cort_state_name_prev, __LINE__); \
        /* Any body in "if/else" will call co_if_end to avoid further judge. */ \
        static cort_proto* co_if_end(cort_local_type *cort){ \
            return  ((CO_JOIN(CO_STATE_NAME, __LINE__)*)(cort))->local_start() ;\
        CORT_NEXT_STATE(CO_JOIN(CO_STATE_NAME, __LINE__))

#define CO_ELSE_IF_END CO_IF_END

#define CO_ELSE_IF(co_bool_condition) \
    CO_BRANCH_END_IMPL \
    cort_proto* local_next_start(){  \
        goto ____action_end; ____action_end:  \
        if(co_bool_condition){ return  ((CO_JOIN(CO_STATE_NAME, __LINE__)*)(this))->inner_start();} \
        else{ return  ((CO_JOIN(CO_STATE_NAME, __LINE__)*)(this))->local_next_start();} \
        CO_BRANCH_BEGIN_IMPL

#define CO_ELSE  \
    CO_BRANCH_END_IMPL \
    cort_proto* local_next_start(){  \
        return ((CO_JOIN(CO_STATE_NAME, __LINE__)*)(this))->inner_start(); \
        CO_BRANCH_BEGIN_IMPL

#define CO_ELSE_END CO_IF_END

#define CO_WHILE(co_bool_condition, ...) \
        goto ____action_end; ____action_end:  \
        if(co_bool_condition){ \
            return ((CO_JOIN(CO_STATE_NAME, __LINE__)*)(this))->inner_start(); \
        }\
        return  ((CO_JOIN(CO_STATE_NAME, __LINE__)*)(this))->local_next_skip(); \
    }}CO_JOIN(cort_state_name, __LINE__);  \
    CO_WHILE_IMPL(co_bool_condition, ##__VA_ARGS__)

#define CO_DO_WHILE(co_bool_condition, ...) \
        goto ____action_end; ____action_end:  \
        return ((CO_JOIN(CO_STATE_NAME, __LINE__)*)(this))->inner_start(); \
    }}CO_JOIN(cort_state_name, __LINE__);  \
    CO_WHILE_IMPL(co_bool_condition, ##__VA_ARGS__)

#define CO_WHILE_IMPL(co_bool_condition, ...) \
    typedef struct CO_JOIN(CO_STATE_NAME, __LINE__) : public cort_local_type { \
        bool co_while_test(){ \
            return (co_bool_condition); \
        } \
        void co_on_continue(){ \
            __VA_ARGS__; \
        } \
        CO_DECL(CO_JOIN(CO_STATE_NAME, __LINE__), inner_start) \
        typedef cort_type break_type; \
        cort_proto* inner_start(){ \
            CO_BEGIN

#define CO_BREAK return break_type::local_next_skip()

#define CO_CONTINUE return break_type::local_next_start()

#define CO_WHILE_END \
        goto ____action_end; ____action_end:  \
        return ((cort_begin_type*)(this))->local_next_start(); }}cort_end_type; \
        static cort_proto* local_start(cort_type* ptr){ return ((cort_begin_type*)(cort_end_type*)ptr)->local_start();} }; \
        return cort_start_impl::local_start(this); \
        } \
        cort_proto* local_next_start(){  \
            co_on_continue(); \
            if(co_while_test()){ \
                return this->inner_start();\
            }\
            return local_next_skip(); \
        }\
        cort_proto* local_next_skip(){  \
            CO_NEXT_STATE

//When you write "CO_END" and "}", a typical coroutine entry function is defined finished.
#define CO_END  \
    CO_RETURN; }}cort_end_type; \
    /*Why not direct ((cort_start_impl::cort_begin_type*)this)->local_start()? Because type of (*this) may be a template class and we need to add a "typename" before cort_start_impl::cort_begin_type*/ \
    static cort_proto* local_start(cort_type* ptr){ return ((cort_begin_type*)(cort_end_type*)ptr)->local_start();} }; \
    return cort_start_impl::local_start(this);
   //}
//};//end of cort_example definition

//Following is a full example, used for CO_WAIT_ANY.
struct cort_wait_n : public cort_proto{
    void init_resume_any(size_t total_wait_count, size_t first_wait_count){
        data10.rest_wait_count = total_wait_count - first_wait_count;
        set_wait_count(first_wait_count);
    }
    cort_proto* on_finish(){
        delete this;
        return this;            //parent->resume(this->get_last_resumer());  is called mannually so we should not return 0.
    }
    enum{is_auto_delete = true};
    CO_DECL(cort_wait_n)
    cort_proto* start(){
        CO_BEGIN
            CO_YIELD();            
            cort_proto* parent = get_parent();
            if(parent != 0){    //Now n waited coroutines finished so we resume our parent.
                parent->resume(this->get_last_resumer());  
            }else{              //All the coroutines are finished.
                CO_RETURN;
            }           
            set_wait_count(data10.rest_wait_count);           
            CO_YIELD_IF(data10.rest_wait_count != 0);
            //Now rest waited coroutines finished so we can delete this according to on_finish.
        CO_END
    }
};

#define CO_AWAIT_ANY_IMPL(sub_cort) {\
    cort_proto *__echo_cort; \
    cort_proto *__wait_result_cort = (sub_cort)->cort_start(__echo_cort); \
    if(__wait_result_cort != 0){ \
        __wait_result_cort->set_parent(__wait_any_cort); \
        ++__current_waited_count; \
    }\
    else if(++__current_finished_count == __max_count){ \
        this->set_last_resumer(__echo_cort); \
        if(__current_waited_count != 0){ \
            __wait_any_cort->set_wait_count(__current_waited_count); \
            __wait_any_cort->start(); \
        } \
        else{ \
            delete __wait_any_cort; \
        } \
        break; \
    } \
}

template<typename T>
cort_proto* cort_wait_range_any(cort_proto* this_ptr, T begin_forward_iterator, T end_forward_iterator, size_t max_count){
    if(begin_forward_iterator == end_forward_iterator){
        return 0;
    }
    size_t current_finished_count = 0; 
    size_t waited_count = 0; 
    cort_wait_n *wait_any_cort = new cort_wait_n();
    for(;begin_forward_iterator != end_forward_iterator;++begin_forward_iterator){
        typename std::iterator_traits<T>::value_type tmp_cort_new = (*begin_forward_iterator);       
        cort_proto *__wait_result_cort = tmp_cort_new->cort_start(); 
        if(__wait_result_cort != 0){ 
            __wait_result_cort->set_parent(wait_any_cort); 
            ++waited_count;
        }
        else if(++current_finished_count == max_count){ 
            this_ptr->set_last_resumer(tmp_cort_new);
            if(waited_count != 0){
                wait_any_cort->set_wait_count(waited_count);
                wait_any_cort->start();
            }
            else{
                delete wait_any_cort;
            }
            return 0;
        }
    }
    wait_any_cort->set_parent(this_ptr);
    wait_any_cort->set_wait_count(waited_count);
    wait_any_cort->start();
    return this_ptr;
}


//Coroutine self wait: 
//As mentioned above, coroutine can have multiple entry function.
//Entry function A can call CO_AWAIT_THIS(B) to await member function B of this.
//
//However, there is a problem: function on_finish will be called twice at finish of both A and B.
//You can use CO_BEGIN_THIS instead of CO_BEGIN to avoid calling on_finish in function B.

#define CO_AWAIT_THIS(member_func_name) do{ \
        CO_DECL(cort_parent_save) \
        cort_proto *result = this->member_func_name(); \
        if(result != 0){ \
           new cort_parent_save(this, (run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::start_static)); \
        }  \
    }while(false); \
    CO_NEXT_STATE
    

#define CO_BEGIN_THIS \
    struct cort_local_type: public cort_type{ \
        cort_proto* on_finish(){return 0;} \
    }; \
    struct cort_start_impl{\
        struct cort_begin_type; \
        typedef struct cort_state_struct{ void dummy(){ \
        CORT_NEXT_STATE(cort_begin_type)  

struct cort_parent_save : public cort_proto{  
    static cort_proto* cort_start_static(cort_proto* arg){
        cort_proto* p = arg->get_last_resumer();
        p->set_parent(arg->get_parent());
        p->set_callback_function(arg->then());
        p->resume(p);
        delete arg;
        return arg; //Because arg->parent() != p, so we should mannually "p->resume(p)"
    }
    cort_parent_save(cort_proto* arg, run_type func){
        set_wait_count(1);
        set_callback_function(&cort_start_static);
        then(func); 
        set_parent(arg->get_parent()); 
        arg->set_parent(this);     
    }
};

//CO_AWAIT can wait another coroutine.
//cort_proto::then can concate another function to a coroutine. The function can be called when the coroutine function is finished and function on_finish is called.
//But ususally cort_proto::then is not called directly with the function. 
//First argumet of CO_THEN is the new sub-class type and the rest is the coroutine address.
//CO_THEN will accept a sub-class like following.

//struct cort_then_example: public cort_proto{
//        CO_DECL(cort_then_example)
//        cort_proto* on_finish(){
//          cort_proto* result;
//          if(result = cort_proto::on_finish() ){
//                return result;
//          }
//          printf("cort finished!\n");
//          return 0;
//        }
//        cort_proto* start(){
//          CO_BEGIN
//            printf("cort_then example!\n" );
//            return 0;
//          CO_END
//        }
//    }x, y;
//struct cort_then : public cort_then_example{
//       CO_DECL(cort_then)
//       cort_proto* start(){
//          CO_BEGIN
//            printf("cort_then function is called!\n" );
//          CO_END
//        }
//};
//CO_THEN(cort_then, &x);
//CO_THEN(cort_then, &x);
//x.start(); 
//
//Output is:
//
//cort_then example!
//cort_then function is called!
//cort finished!
//cort finished!
//
//Through x is set twice, only last setting before start is accepted. 
//So cort_then::start is called only once.
//cort_then_example::on_finish is called twice, at end of cort_then_example::start and cort_then::start. 
//If you want to skip the last calling, using CO_EXIT before CO_END or instead of CO_RETURN;
//cort_then::start is called at first execution of "if(result = cort_proto::on_finish() )". 
//Before the calling, the setting is cleared in cort_proto::on_finish. 
//So at second calling of cort_proto::on_finish(), cort_then::start will not be called.


#define CO_THEN(new_type, sub_cort) sub_cort->then(new_type::cort_start_static);
    
//Supporing proto lambda grammar
#include "cort_lambda.h"
    
#endif
