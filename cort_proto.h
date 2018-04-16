#ifndef CORT_PROTO_H_
#define CORT_PROTO_H_
#include <stdlib.h>

#define CO_JOIN2(x,y) x##y 
#define CO_JOIN(x,y) CO_JOIN2(x,y)

#if defined(__GNUC__)
#define CO_TYPE_OF __typeof__
#define co_likely_if(x)    if(__builtin_expect(!!(x), 1))
#define co_unlikely_if(x)  if(__builtin_expect(!!(x), 0))
#else
#define CO_TYPE_OF decltype
#define co_likely_if(x)    if(x)
#define co_unlikely_if(x)  if(x)
#endif

#if defined(CORT_SINGLE_THREAD)
#define __thread
#endif

struct cort_proto{ 
    typedef cort_proto* (*run_type)(cort_proto*);
    typedef cort_proto base_type;
    
    //获取协程的下次运行函数，为空则表示协程未启动或者已经结束
    run_type get_run_function() const {
        return data0.run_function;
    }
    //设置协程的下次运行函数
    void set_run_function(run_type arg){
        data0.run_function = arg;
    }
    
    //当前协程是否已经结束或者未启动
    bool is_finished() const{
        return data0.run_function == 0;
    }
    
    //协程内如果自己想等待自己（简称自等待），使用这个函数来设置下一次执行的函数。
    //这是一个罕有使用的功能。
    void set_self_await_function(run_type arg){
        data1.leaf_cort_run_function = arg;
    }
    
    //从自等待中恢复
    cort_proto* resume_self_await(){
        run_type arg = data1.leaf_cort_run_function;
        data1.leaf_cort_run_function = 0;
        return arg(this);
    }
    
    //移除当前协程的等待者
    void remove_parent(){
        cort_parent = 0;
    }
    //设置当前协程的等待者
    void set_parent(cort_proto *arg){
        cort_parent = arg;
    }
    //获取当前协程的等待者
    cort_proto *get_parent() const{
        return cort_parent;
    }
    
    //获取当前协程等待了多少个子协程的完成
    size_t get_wait_count() const{
        return this->data1.wait_count;
    }
    //设置当前协程等待的子协程的个数
    void set_wait_count(size_t wait_count){
        this->data1.wait_count = wait_count;
    }
    //增加当前协程等待的子协程的个数
    void incr_wait_count(size_t wait_count){
        this->data1.wait_count += wait_count;
    }
    //减少当前协程等待的子协程的个数，如果减到0了，会让当前协程从等待状态中resume.
    void decr_wait_count(size_t wait_count){
        this->data1.wait_count -= wait_count;
        if(this->data1.wait_count == 0){
            this->resume();
        }
    }
        
    //我们让this协程等待另外一个协程sub_cort
    template<typename T>
    cort_proto* await(T* sub_cort){
        cort_proto* __the_sub_cort = sub_cort->cort_start();
        if(__the_sub_cort != 0){
            __the_sub_cort->set_parent(this); 
            this->incr_wait_count(1); 
            return __the_sub_cort; 
        }
        return 0;
    }
    
    //当前协程等待的所有子协程完毕，就会执行run_function来resume。如果执行完毕则会检查父协程是否可以resume。
    void resume() {
        //We save the cort_parent before run_function. So you can "delete this" in your subclass overloaded on_finish function after parent class on_finish function is called and return 0.
        cort_proto* cort_parent_save = cort_parent;
        if((*(this->data0.run_function))(this) == 0 && 
            cort_parent_save != 0 && 
            (--(cort_parent_save->data1.wait_count)) == 0){
            cort_parent_save->resume();
        }
    }
    
    //如果你打算让你的协程支持“协程池”功能，你可以重载clear函数来标明一个协程在进入协程池以备未来复用之前，应该如何清除状态。
    virtual void clear(){
    }
    
    //If T is a local coroutine class, c++11 needed because c++03 disabled local class to be the template argument.
    //corts[0].then<T>;
    template<typename T>
    void then(){
        data10.cort_then_function = T::cort_start_static;
    }
    
    void then(run_type then_function){
        data10.cort_then_function = then_function;
    }
    run_type then()const {
        return data10.cort_then_function;
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
        cort_proto* real_cort_parent;
    }data1;
    cort_proto* cort_parent;
    
    union{
        size_t rest_reference_count;
        run_type cort_then_function;
    }data10;

public: //Following members should be protected, however the subclass defined in a function can not access them @vc2008
    //on_finish 函数是一个非虚函数，每个协程在自己结束时都会（通过宏，所以不会损失类型信息而无需使用虚函数）调用它
    //你也可以在自己的协程子类中重载它。记得调用父类的on_finish函数否则会丧失诸如then这些功能。
    inline cort_proto* on_finish(){
        if(data10.cort_then_function != 0 ){
            cort_proto* result = data10.cort_then_function(this);
            data10.cort_then_function = 0;
            if(result != 0){
                return result;
            }
        }
        cort_parent = 0;
        data0.run_function = 0;
        return 0;
    }
    
    cort_proto(){
        //We do not use initialize list because we want to remove the initialize order limit.
        cort_parent = 0;
        data0.run_function = 0;
        data1.leaf_cort_run_function = 0;
        data10.cort_then_function = 0;
    }
    
protected:
    virtual ~cort_proto(){}
};                                                                  

template<typename T>
struct cort_auto_delete : public T{
    cort_proto* on_finish(){
        delete this;
        return 0;
    }
};

typedef cort_auto_delete<cort_proto> cort_auto;

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



#define CO_FE_BINARY_0(co_call, x, y,...) co_call(x,y)
#define CO_FE_BINARY_1(co_call, x, y,...) co_call(x,y) CO_FE_BINARY_0(co_call, x, __VA_ARGS__)
#define CO_FE_BINARY_2(co_call, x, y,...) co_call(x,y) CO_FE_BINARY_1(co_call, x, __VA_ARGS__)
#define CO_FE_BINARY_3(co_call, x, y,...) co_call(x,y) CO_FE_BINARY_2(co_call, x, __VA_ARGS__)
#define CO_FE_BINARY_4(co_call, x, y,...) co_call(x,y) CO_FE_BINARY_3(co_call, x, __VA_ARGS__)
#define CO_FE_BINARY_5(co_call, x, y,...) co_call(x,y) CO_FE_BINARY_4(co_call, x, __VA_ARGS__)
#define CO_FE_BINARY_6(co_call, x, y,...) co_call(x,y) CO_FE_BINARY_5(co_call, x, __VA_ARGS__)
#define CO_FE_BINARY_7(co_call, x, y,...) co_call(x,y) CO_FE_BINARY_6(co_call, x, __VA_ARGS__)
#define CO_FE_BINARY_8(co_call, x, y,...) co_call(x,y) CO_FE_BINARY_7(co_call, x, __VA_ARGS__)
#define CO_FE_BINARY_9(co_call, x, y,...) co_call(x,y) CO_FE_BINARY_8(co_call, x, __VA_ARGS__)

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

#define CO_EXPAND(x) x

#define CO_ECHO(x) x

#define CO_FOR_EACH(x, ...) \
    CO_EXPAND(CO_GET_NTH_ARG(__VA_ARGS__, CO_FE_9, CO_FE_8, CO_FE_7, CO_FE_6, CO_FE_5, CO_FE_4, CO_FE_3, CO_FE_2, CO_FE_1, CO_FE_0)(x, __VA_ARGS__))

#define CO_COUNT_INC(x) +1
#define CO_ARG_COUNT(...) (0+CO_FOR_EACH(CO_COUNT_INC, __VA_ARGS__))
    
#define CO_FOR_EACH_BINARY(x, y, ...) \
    CO_EXPAND(CO_GET_NTH_ARG(__VA_ARGS__, CO_FE_BINARY_9, CO_FE_BINARY_8, CO_FE_BINARY_7, CO_FE_BINARY_6, CO_FE_BINARY_5,\
        CO_FE_BINARY_4, CO_FE_BINARY_3, CO_FE_BINARY_2, CO_FE_BINARY_1, CO_FE_BINARY_0)(x, y, __VA_ARGS__))
    
#define CO_GET_LAST_ARG(...)  \
    CO_GET_NTH_ARG(__VA_ARGS__, CO_ARG_9, CO_ARG_8, CO_ARG_7, CO_ARG_6, CO_ARG_5, CO_ARG_4, CO_ARG_3, CO_ARG_2, CO_ARG_1, CO_ARG_0)(CO_ECHO, __VA_ARGS__)
    
//如果你想让你的协程结束但并不调用on_finish函数，请使用以下宏结束你的协程
//CO_EXIT will end the coroutine function but it does not call on_finish. 
#define CO_EXIT do{ \
        data0.run_function = 0; \
        return 0; \
    }while(false)

#define CO_SWITCH return this
    
// Now let us show an example.
//我们通过cort_example来介绍如何定义和子类化使用一个协程
//首先定义你协程类名cort_example， 并让他继承一个cort_proto子类作为他的父类。这里我们就选择cort_proto了
struct cort_example : public cort_proto{

//如果你的协程函数中有局部变量，需要在经过等待操作后还能访问到，那请把他们定义在类这里成为成员变量（如同C语言把局部变量集中定义一样）。
//如果你的协程入口函数是有参数和返回值的，也请把他们定义为成员变量来进行传递。
//First put all your context here as class member.
    //int run_times;

//在类里，你也可以定义一些自己需要的成员函数，例如构造函数，析构函数。也可能像上面的介绍那样，重写如clear函数, on_finish函数
//Or you want to overload some member function
    //void clear(){}
    //~cort_example(){}
    //cort_example(){}

//接下来用CO_DECL定义协程入口函数。
//第一个参数是当前的类名字（必填）
//第二个参数是协程入口函数名字（选填），如果不填则会认为start函数是协程入口函数的名字。
    //CO_DECL(cort_example) to declare this is a coroutine class
    //or CO_DECL(cort_example, new_start) to use "new_start"  instead default "start" as the coroutine entrance function
#define CO_DECL(...) \
public: \
    CO_DECL_PROTO(__VA_ARGS__) \
    static inline base_type* cort_start_static(cort_proto* this_ptr){return ((cort_type*)this_ptr)->CO_EXPAND(CO_GET_2ND_ARG(__VA_ARGS__,start))();}\
    inline base_type* cort_start() { return this->CO_EXPAND(CO_GET_2ND_ARG(__VA_ARGS__,start))();/*coroutine function name is start for default*/}                      

//有可能你想说明当前类是一个协程，但是暂时还不想定义协程入口函数，想留待子类来定义协程入口函数，
//那么请使用CO_DECL_PROTO来代替CO_DECL
//If this class is not defined as the coroutine entrance, you can CO_DECL_PROTO instead to avoid defines a enter function.
#define CO_DECL_PROTO(...)  typedef CO_EXPAND(CO_GET_1ST_ARG(__VA_ARGS__)) cort_type;  


//接下来我们就可以来定义我们的协程入口函数了。这个函数的函数签名是固定的：返回一个cort_proto*的无参非静态成员函数。
//注意他和普通的成员函数一样，不一定需要定义在类定义里，可以在类里给一个声明，在别的文件里分离定义亦可。
//他返回0表示已经执行完成，非0表示暂时还没有完成。
//假设我们的协程入口函数名字是默认值start
    //Now define you enter function. 
    //Anyway, you can define it in another file or out of class, leaving only declaration but not implement here.
    //cort_proto* start(){

//start作为协程入口函数，需要以CO_BEGIN开头，CO_END结尾。写在CO_BEGIN之前的代码会在协程执行时执行一次，但是在协程resume过程中不会再次执行。
#define CO_BEGIN \
    struct cort_start_impl{\
        struct __cort_begin; \
        static __cort_begin* get_begin_ptr(cort_type* ptr){ return (__cort_begin*)ptr;} \
        struct __cort_state_struct{ void dummy(){ \
        CORT_NEXT_STATE(__cort_begin)      

//写下CO_BEGIN8个字符之后，你就可以写你的协程函数代码直到CO_END。注意把成员变量当局部变量用。
//Now you can define the coroutine function codes, using the class member as the local variable.
//注意你可以在cort_example里面定义其他成员函数，里面的代码也是用CO_BEGIN CO_END所包围：
//是的, 我们允许我们的一个协程有多个入口函数，在CO_DECL中声明的名字只是个默认入口函数而已，
//我们接下来会介绍如何在CO_AWAIT等API中使用非默认的入口函数。


//在协程函数里，你可以使用以下的等待API，这些API只能在协程函数内部使用。
//In your codes, you may await with other sub-coroutines. Using following API.

//你可以使用CO_AWAIT来等待一个子协程。第一个参数是子协程的地址（必填），第二个是协程入口函数名字（选填），不填则使用CO_DECL中声明的名字或者start.
//注意CO_AWAIT不能使用在协程函数的条件分支或者循环体内部，只能使用在第一级大括号内。
//You can use CO_AWAIT to wait a sub-coroutine. It can not be used in any branch or loop.
#define CO_AWAIT(...) CO_AWAIT_IMPL(CO_EXPAND(CO_GET_1ST_ARG(__VA_ARGS__)), CO_EXPAND(CO_GET_2ND_ARG(__VA_ARGS__, cort_start)), CO_JOIN(CO_STATE_NAME, __LINE__))

//你可以使用CO_AWAIT_ALL来等待多个但不超过10个子协程。这个API不能指定协程入口函数，所以只能使用默认入口函数。
//注意CO_AWAIT_ALL不能使用在协程函数的条件分支或者循环体内部，只能使用在第一级大括号内。
//You can use CO_AWAIT_ALL to wait no more than 10 sub-coroutine. It can not be used in any branch or loop.
#define CO_AWAIT_ALL(...) do{ \
        size_t __current_wait_count = 0; \
        CO_FOR_EACH(CO_AWAIT_MULTI_IMPL, __VA_ARGS__) \
        if(__current_wait_count != 0){ \
            this->set_wait_count(__current_wait_count); \
            this->set_run_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::do_exec_static)); \
            return this; \
        } \
    }while(false);\
    CO_NEXT_STATE
   
//你可以使用CO_AWAIT_RANGE来等待在2个协程指针迭代器之间的所有协程。这个API不能指定协程入口函数，所以只能使用默认入口函数。
//注意CO_AWAIT_RANGE不能使用在协程函数的条件分支或者循环体内部，只能使用在第一级大括号内。    
//You can use CO_AWAIT_RANGE to wait variate count of sub-coroutine between the forward iterators.
//It can not be used in any branch or loop.
#define CO_AWAIT_RANGE(sub_cort_begin, sub_cort_end) do{ \
        if(cort_wait_range(this, sub_cort_begin, sub_cort_end) != 0){ \
            this->set_run_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::do_exec_static)); \
            return this; \
        } \
    }while(false); \
    CO_NEXT_STATE
        
//上面我们已经介绍了很多等待API，我们把每一个等待API调用处的下一行语句称为“恢复点”。
//我们注意到他们不能使用在循环体，条件分支等第二层或者更深层大括号内的限制（这些限制并非由于我们的设计，而是C++文法导致的），
//所以我们提供一些专有的辅助API来模拟循环和条件分支以及跳转。

//CO_AWAIT_AGAIN类似于CO_AWAIT但是他从等待中恢复会跳转到上一个“恢复点” 而不是下一个去执行。行为类似一个循环。
//After wait in CO_AWAIT_AGAIN finished, it will not turn to next action but current action begin. It behaves like a loop. It can not be used in any branch or loop.
#define CO_AWAIT_AGAIN(...) do{ \
        cort_proto* __the_sub_cort = (CO_EXPAND(CO_GET_1ST_ARG(__VA_ARGS__)))->CO_EXPAND(CO_GET_2ND_ARG(__VA_ARGS__, cort_start))();\
        if(__the_sub_cort != 0){\
            __the_sub_cort->set_parent(this); \
            this->set_wait_count(1); \
            this->set_run_function((run_type)(&this_type::do_exec_static)); \
            return __the_sub_cort; \
        }\
        goto ____action_begin; \
    }while(false); \
    CO_NEXT_STATE;
    
#define CO_NEXT_STATE \
    CO_GOTO_NEXT_STATE; \
    CO_ENTER_NEXT_STATE; \
    CORT_NEXT_STATE(CO_JOIN(CO_STATE_NAME, __LINE__)) 
 
//有时候我们知道当前协程需要等待但是我们（暂时）不知道它需要等待什么东西。可以使用CO_AWAIT_UNKNOWN/CO_AWAIT_UNKNOWN_AGAIN。
//当你知道它需要等待什么的时候，请使用cort_proto::await 函数(从协程外部)来告诉它需要等待什么。
//如果他是个叶子协程，那就等待调度器来resume他了（所以这以下几个API在叶子协程中很常见）。
//这是一种“依赖反转”的设计模式。
//Sometimes you know you have to pause but you do not know who will resume you, or the scheduler will resume you. 
//Using CO_AWAIT_UNKNOWN(), other coroutines can use cort_proto::await to tell you what you should wait,
//or the schedulmer will resume you.
//This is a useful interface for "Dependency Inversion": it enable the coroutine set 
//the resume condition after pause.
#define CO_AWAIT_UNKNOWN() do{ \
        this->set_run_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::do_exec_static)); \
        return this;   \
    }while(false); \
    CO_NEXT_STATE
    
#define CO_AWAIT_UNKNOWN_AGAIN() do{ \
        this->set_run_function((run_type)(&do_exec_static)); \
    }while(false); \
    return this;   \
   CO_NEXT_STATE
    
//在lua等语言中，拥有CO_AWAIT_UNKNOWN职责的关键字是yield，所以我们特意取了类似的别名。
#define CO_YIELD() CO_AWAIT_UNKNOWN()
#define CO_YIELD_AGAIN() CO_AWAIT_UNKNOWN_AGAIN()

//当协程A等待B，B又等待C的时候。假设B等待C完成之后其实没有任何后续代码需要执行了，我们可以在B等待C的时候，使用CO_AWAIT_RETURN：
//直接让B结束，并且让A直接等待C而不是B。这是一种类似尾递归的优化。
//B结束时on_finish 和then依然会执行，注意不要在then中等待别的协程了，否则就不叫“B等待C完成之后其实没有任何后续代码需要执行了”。
//Sometimes you want to stop the couroutine after a sub_coroutine is finished. Using CO_AWAIT_RETURN.
//It must be used in a branch or loop, or else it must be followed by CO_END.
#define CO_AWAIT_RETURN(sub_cort) do{ \
        base_type* __the_sub_cort = (sub_cort)->cort_start();\
        if(__the_sub_cort != 0){\
            __the_sub_cort->set_parent(this->cort_parent); \
            this->on_finish(); \
            return __the_sub_cort; \
        }\
    }while(false); \
    CO_RETURN   

    

//CO_RETURN是用来结束当前协程代码和返回的，类似return 标识了函数的结束和返回。
//可以使用在协程代码中的任何地方.
//Sometimes you want to exit from the couroutine. Using CO_RETURN. It can be used anywhere in a coroutine non-static member function.
//If you want to skip on_finish(for example, you delete this in your coroutine function), using CO_EXIT
#define CO_RETURN return this->on_finish(); 
    
//当你发现当前协程执行不下去，期待等会儿从上一个恢复点重新执行时，
//请使用CO_AGAIN（这种模式在IO代码里很常见，例如recv产生了一次EAGAIN，并且你知道数据并未接收完整，那显然需要等epoll可用后重新recv）。
//可以使用在协程代码中的任何地方。
//Sometimes you want to execute current action again later. Using CO_AGAIN. It can be used anywhere in a coroutine non-static member function.
#define CO_AGAIN do{ \
        this->set_run_function((run_type)(&this_type::do_exec_static));  \
        return this; \
    }while(false)


//前面介绍了很多前向跳转的适配，我们来介绍一些后向跳转的适配API。
//你可以使用CO_SKIP_AWAIT或者CO_GOTO_NEXT_STATE来直接跳转到下一个恢复点执行
//他们可以在协程代码中的任何地方使用，除非没有下一个恢复点了。
//You can skip next await action using CO_SKIP_AWAIT
//It can not skip CO_AWAIT_RETURN.

#define CO_BREAK CO_SKIP_AWAIT

#define CO_SKIP_AWAIT CO_GOTO_NEXT_STATE

#define CO_GOTO_NEXT_STATE goto ____action_end;

//下面是一些API的条件模式。只会在第一个参数为true的情况下才会执行AWAIT否则会直接跳转到AWAIT后的恢复点继续执行。
//注意CO_AWAIT_ALL不能使用在协程函数的条件分支或者循环体内部，只能使用在第一级大括号内。
//Following are conditional form APIs
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


    
//比较厌烦上面这种挨个加if的形式对吧，给一个通用的条件行为式。
#define CO_OP_IF(bool_exp, op, ...) \
    if(!(bool_exp)){CO_GOTO_NEXT_STATE; } \
    op(__VA_ARGS__)
    
#define CO_AWAIT_UNKNOWN_AGAIN_IF(bool_exp) CO_OP_IF(bool_exp, CO_AWAIT_UNKNOWN_AGAIN)

#define CO_YIELD_AGAIN_IF(bool_exp) CO_OP_IF(bool_exp, CO_YIELD_AGAIN)
    
//Implement
#define CO_AWAIT_MULTI_IMPL(sub_cort) {\
    CO_AWAIT_MULTI_IMPL_IMPL(this, sub_cort) \
}

#define CO_AWAIT_MULTI_IMPL_IMPL(this_ptr, sub_cort) {\
    cort_proto *__the_sub_cort = (sub_cort)->cort_start(); \
    if(__the_sub_cort != 0){ \
        __the_sub_cort->set_parent(this_ptr); \
        ++__current_wait_count; \
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

#define CO_ENTER_NEXT_STATE ____action_end: return  ((CO_JOIN(CO_STATE_NAME, __LINE__)*)(this))->do_exec();

#define CORT_NEXT_STATE(cort_state_name) \
    }};struct cort_state_name : public cort_type {                       \
        typedef cort_state_name this_type;                               \
        static base_type* do_exec_static(cort_proto* this_ptr){return ((this_type*)(this_ptr))->do_exec();}\
        inline base_type* do_exec() { goto ____action_begin; ____action_begin:
 
//当你写下CO_END和反大括号的时候，一个协程入口函数的定义就算完成了。
#define CO_END  CO_RETURN; }}; }; \
    return cort_start_impl::get_begin_ptr(this)->do_exec(); 
   //}
};//end of cort_example definition

//Following is a full example.

struct cort_wait_n : public cort_proto{
    void init_resume_any(size_t wait_count, size_t n){
        data10.rest_reference_count = wait_count - n;
        data1.wait_count = n;
    }
    cort_proto* on_finish(){
        delete this;
        return this; //get_parent()->resume() is called mannually so we should not return 0.
    }
    
    CO_DECL(cort_wait_n)
    cort_proto* start(){
        CO_BEGIN
            CO_YIELD();
            //now n waited coroutines finished so we resume our parent.
            cort_proto* parent = get_parent();
            if(parent != 0){
                parent->resume();  
            }
            else{
                CO_RETURN;
            }
            data1.wait_count = data10.rest_reference_count;
            CO_YIELD_IF(data1.wait_count != 0);
            //now rest waited coroutines finished so we can delete this.
        CO_END
    }
};

#define CO_AWAIT_ANY_IMPL_IMPL(sub_cort) {\
    cort_proto *__the_sub_cort = (sub_cort)->cort_start(); \
    if(__the_sub_cort != 0){ \
        __the_sub_cort->set_parent(__wait_any_cort); \
        ++__current_waited_count; \
    }\
    else if(++__current_finished_count == __max_count){ \
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

//等待许多子协程中的任意n个的返回
#define CO_AWAIT_ANY_N(n, ...) do{ \
        size_t __current_finished_count = 0; \
        size_t __current_waited_count = 0; \
        const size_t __max_count = (size_t)(n); \
        cort_wait_n *__wait_any_cort = new cort_wait_n(); \
        CO_FOR_EACH(CO_AWAIT_ANY_IMPL_IMPL, __VA_ARGS__) \
        __wait_any_cort->init_resume_any(__current_waited_count,  __max_count - __current_finished_count); \
        __wait_any_cort->start(); \
        __wait_any_cort->set_parent(this); \
        this->set_wait_count(1); \
        this->set_run_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::do_exec_static)); \
        return this; \
    }while(false);\
    CO_NEXT_STATE
    
//等待协程区间中的任意n个的返回
#define CO_AWAIT_RANGE_ANY_N(n, sub_cort_begin, sub_cort_end) do{ \
        cort_proto *__wait_result_cort = cort_wait_range_any(this, sub_cort_begin, sub_cort_end, n); \
        if(__wait_result_cort != 0){ \
            __wait_result_cort->set_run_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::do_exec_static)); \
            return __wait_result_cort; \
        } \
    }while(false);\
    CO_NEXT_STATE
    
//等待许多子协程中的任意1个的返回
#define CO_AWAIT_ANY(...) CO_AWAIT_ANY_N(1, __VA_ARGS__)

//等待协程区间中的任意1个的返回
#define CO_AWAIT_RANGE_ANY(sub_cort_begin, sub_cort_end) CO_AWAIT_RANGE_ANY_N(1, sub_cort_begin, sub_cort_end) 

#include <iterator>
template <typename T>
size_t cort_wait_range(cort_proto* this_ptr, T begin_forward_iterator, T end_forward_iterator){
    size_t __current_wait_count = 0;
    while(begin_forward_iterator != end_forward_iterator){
        typename std::iterator_traits<T>::value_type tmp_cort_new = (*begin_forward_iterator); 
        CO_AWAIT_MULTI_IMPL_IMPL(this_ptr, tmp_cort_new) 
        ++begin_forward_iterator;
    }
    this_ptr->set_wait_count(__current_wait_count);
    return __current_wait_count;
}

template<typename T>
cort_proto* cort_wait_range_any(cort_proto* this_ptr, T begin_forward_iterator, T end_forward_iterator, size_t __max_count){
    size_t current_finished_count = 0; 
    size_t waited_count = 0; 
    cort_wait_n *wait_any_cort = new cort_wait_n();
    for(;begin_forward_iterator != end_forward_iterator;++begin_forward_iterator){
        typename std::iterator_traits<T>::value_type tmp_cort_new = (*begin_forward_iterator);       
        cort_proto *__the_sub_cort = tmp_cort_new->cort_start(); 
        if(__the_sub_cort != 0){ 
            __the_sub_cort->set_parent(wait_any_cort); 
            ++waited_count;
        }
        else if(++current_finished_count == __max_count){ 
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
    wait_any_cort->set_wait_count(waited_count);
    wait_any_cort->start();
    wait_any_cort->set_parent(this_ptr);
    this_ptr->set_wait_count(1);
    return this_ptr;
}

template<typename T>
T* cort_set_parent(T* son, cort_proto* parent = 0){
    son->set_parent(parent);
    return son;
}

//接下来介绍一些比较冷门的功能。

//如果你直接调用了协程的一个协程入口成员函数cort->start()，那么本质上就是发起了一次异步调用加上可能的then回调而没有任何等待。
//事实上cort->start() 是很常见的根协程启动方式。
//但是如果cort已经被使用过一次了，注意他是否被合适的清理过以备第二次复用。可能某些协程子类使用了CO_EXIT来结束协程
//导致on_finish函数并未被调用所以cort并未完成清理。
//你也可以使用CO_ASYNC(cort, start)或者CO_ASYNC(cort) 这样的语法来在cort->start()执行前强制发起最基本的清理以防万一。
//他可以在任何地方使用，甚至在协程函数体外部。 
//CO_ASYNC will async call a coroutine.
//First argument is the coroutine, the second is the enter function name(or your coroutine enter function name, if there exist only one argument).
//The called coroutine should maintain its lifetime itself.
//Usually you can CO_ASYNC a coroutine x only if x->is_finished() is true.
//CO_ASYNC can be used anywhere. If the coroutine is not awaited before, it is equal to direct call "cort->cort_start()".
#define CO_ASYNC(...) \
    cort_set_parent(CO_EXPAND(CO_GET_1ST_ARG(__VA_ARGS__)))->CO_EXPAND(CO_GET_2ND_ARG(__VA_ARGS__, cort_start))()

#define CO_GET_2ND_DEFAULT(x, ...) CO_GET_NTH_ARG(__VA_ARGS__, CO_FE_9, CO_FE_8, CO_FE_7, CO_FE_6, CO_FE_5, CO_FE_4, CO_FE_3, CO_FE_2, CO_GET_1ST_ARG, CO_GET_3RD_ARG)(x, __VA_ARGS__)


//协程的自等待：如上所述，协程可以拥有多个入口协程函数，可以在函数A里等待this的B的完成（CO_SELF_AWAIT(this, B)）。
//我们把这叫做自等待。抱歉为了性能只有满足以下条件的场景才允许使用自等待并且需要使用特殊的API。
//0. 使用CO_SELF_AWAIT发起等待
//1. B内不得发起其他任何等待或者自等待。
//2. 不过B可以使用CO_YIELD, CO_AGAIN.
//3. 当前协程如果已经在B中使用了CO_YIELD, CO_AGAIN，也不可以使用cort_proto::await 从外部增加等待
//4. 被等待函数需要使用下面的CO_SELF_RETURN函数返回，避免on_finish函数在A和B中重复被调用。
//总体来说，因为有太多的限制，自等待并不是作者积极推荐给大家使用的一个功能，但是他确实能在某些极限情况下节省一些空间和优化封装。
//CO_SELF_AWAIT is only available for leaf coroutines that return using CO_SELF_RETURN!!!! 
//More accurately, in "member_func_name",
//1. you can not wait any coroutine (including CO_AWAIT or CO_SELF_AWAIT other member function). 
//2. CO_YIELD, CO_AGAIN is enabled. CO_SLEEP and other API that "await" any coroutine is disabled.
//3. cort_proto::await is also disabled 
#define CO_SELF_AWAIT(member_func_name) do{ \
    cort_proto * result = this->member_func_name(); \
    if(result != 0){ \
        set_self_await_function((run_type)(&CO_JOIN(CO_STATE_NAME, __LINE__)::do_exec_static)); \
        return result; \
    }  \
}while(false); \
CO_NEXT_STATE

//If current function is waited by CO_SELF_AWAIT, using CO_SELF_RETURN instead of CO_RETURN to avoid on_finish called twice.
#define CO_SELF_RETURN  do{ \
    if(data1.leaf_cort_run_function != 0){ \
        return resume_self_await(); \
    } \
    return 0; \
}while(false)
    
#define cort_then_impl(new_type, cort) cort->data10.cort_then_function = new_type::cort_start_static;
    
    //有时候你依然喜欢传统的 then或者when 这种完成回调功能。你可以使用cort_then来实现类似的语法。
    //https://github.com/yuanzhubi/cort_proto/blob/e3c0aba65299331cdaa04f0dc500ecc8b2781b23/unit_test/cort_proto_test.cpp#L93
    //struct cort_then_output: public fibonacci_cort{
    //        CO_DECL(cort_then_output)
    //        cort_proto* start(){
    //            printf("intput:%d, output:%d! \n", n, result);
    //            return 0;
    //        }
    //    };
    //cort_then(cort_then_output, corts[0]); 
    //CO_AWAIT(corts[0]);
    //这里使用子类化的方法来实现完成级联，CO_AWAIT里面 fibonacci_cort类型的corts[0]执行完毕后会以cort_then_output 的方式来继续执行。
    //注意他依然只能访问fibonacci_cort，而且不能增加成员变量或者定义或重写虚函数。
    //cort_then的第一个参数是子类名字，第二个是需要执行then级联的协程地址，你也可以加上更多的协程地址作为cort_then的参数
#define cort_then(new_type,...) CO_FOR_EACH_BINARY(cort_then_impl, new_type, __VA_ARGS__)
    
#define cort_then_range(new_type, cort_begin, cort_end) do{ \
    for(cort_proto* it = cort_begin; it != cort_end; ++it){ \
        cort_then_impl(new_type, it); \
    } \
}while(false)
        
#endif
