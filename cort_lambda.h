#ifndef CORT_LAMBDA_H
#define CORT_LAMBDA_H

//You can create an annoymous coroutine lambda class via CO_AWAIT_LB and await it.
//The first argument is the base type of the new coroutine.
//The second argument is the lambda function body. Please wrap it with parenthesis, because your function codes may contain commas. CO_BEGIN and CO_END are not needed, you do not need to write it.
//The rest arguments(optional) are the caputered variables. They are captured "by value". No more than 10 arguments are accepted.
#define CO_AWAIT_LB(base_type, co_lambda,...) cort_proto *CO_JOIN(cort_wait_result, __LINE__); \
    {CO_DEF_LAMBDA_STRUCT(base_type, CO_EXPAND(co_lambda), ##__VA_ARGS__); \
    CO_JOIN(cort_wait_result, __LINE__) = (new CO_JOIN(CL_LAMBDA_TYPE_, __LINE__)(__VA_ARGS__))->start(); \
    }CO_UNTIL(CO_JOIN(cort_wait_result, __LINE__))

//CO_ASYNC_LB has the same argument as CO_AWAIT_LB, but the lambda function is just executed but not waited. 
#define CO_ASYNC_LB(base_type, co_lambda,...) do{ \
    CO_DEF_LAMBDA_STRUCT(base_type, CO_EXPAND(co_lambda), ##__VA_ARGS__); \
    (new CO_JOIN(CL_LAMBDA_TYPE_, __LINE__)(__VA_ARGS__))->start(); \
}while(false)

//You can set the "then" function for the corotuine using lambda function via CO_THEN_LB.
//The first argument is the coroutine.
//The second argument is the lambda function body.
//CO_THEN_LB can not capture other arguments.
#define CO_THEN_LB(sub_cort, co_lambda) do{ \
    typedef cort_lambda::co_remove_pointer<CO_TYPE_OF(sub_cort)>::type CO_JOIN(CL_LAMBDA_BASE_TYPE_, __LINE__); \
    CO_DEF_LAMBDA_STRUCT_THEN(CO_JOIN(CL_LAMBDA_BASE_TYPE_, __LINE__), CO_EXPAND(co_lambda)); \
    (sub_cort)->then(CO_JOIN(CL_LAMBDA_TYPE_, __LINE__)::cort_start_static); \
}while(false)

namespace cort_lambda{
    template <typename T, bool is_too_large = (sizeof(T)>16) >
    struct co_arg_type{ //For argument larger than 16 bytes, we pass it by value.
        typedef T type;
    };

    template <typename T>
    struct co_arg_type<T, true>{
        typedef T& type;
    };

    template<typename T>
    struct co_remove_ref{
        typedef T type;
    };
    
    template<typename T>
    struct co_remove_ref<T&>{
        typedef T type;
    };
    
    template <typename T>
    struct co_remove_pointer{
        typedef T type;
    };
    
    template <typename T>
    struct co_remove_pointer<T*>{
        typedef T type;
    };
    
    template <typename T>
    struct co_remove_pointer<T* &>{
        typedef T type;
    };
    
    template <typename T>
    struct co_remove_pointer<const T*>{
        typedef T type;
    };
    
    template <typename T>
    struct co_remove_pointer<const T* &>{
        typedef T type;
    };
}
   
#define CL_TYPEDEF(x) typedef CO_TYPE_OF(x) CO_JOIN(CL_TYPE_, x);
#define CL_DEF_MEMBER(x) CO_JOIN(CL_TYPE_, x) x;
#define CL_DEF_MEMBER_ARG(x) cort_lambda::co_arg_type<CO_JOIN(CL_TYPE_, x)>::type CO_JOIN(arg_, x)
#define CL_ASSIGN(x) ,x(CO_JOIN(arg_, x))
#define CL_COMMA_BACK(x) x,

#define CO_DEF_LAMBDA_STRUCT(base_type, co_lambda, ...) \
    CO_FOR_EACH(CL_TYPEDEF,  ##__VA_ARGS__) \
    struct CO_JOIN(CL_LAMBDA_TYPE_, __LINE__) : public base_type{\
        CO_FOR_EACH(CL_DEF_MEMBER,  ##__VA_ARGS__) \
        inline CO_JOIN(CL_LAMBDA_TYPE_, __LINE__) (CO_FOR_EACH_COMMA(CL_DEF_MEMBER_ARG, ##__VA_ARGS__)): /*... is used for the possible last comma*/\
            base_type() CO_FOR_EACH(CL_ASSIGN,  ##__VA_ARGS__){} \
        \
        /*CO_DECL(CO_JOIN(CL_LAMBDA_TYPE_, __LINE__))*/ \
        /*Some functions are not needed so we manually implement our version of CO_DECL.*/ \
        \
        CO_DECL_PROTO(CO_JOIN(CL_LAMBDA_TYPE_, __LINE__)) \
        static cort_proto* cort_start_static(cort_proto* this_ptr){return ((cort_type*)this_ptr)->start();}\
        \
        /*We will delete this in on_finish. */\
        enum{is_auto_delete = true}; \
        \
        cort_proto* on_finish(){ \
            cort_proto* result = base_type::on_finish(); \
            if(result == 0 && (!base_type::is_auto_delete)){ \
                delete this; \
            } \
            return result;\
        } \
        \
        cort_proto* start() { \
            CO_BEGIN \
                CO_REMOVE_PARENTHESIS(co_lambda) \
            CO_END \
        }\
    }
    
#define CO_DEF_LAMBDA_STRUCT_THEN(base_type, co_lambda) \
    struct CO_JOIN(CL_LAMBDA_TYPE_, __LINE__) : public base_type{\
        CO_DECL_PROTO(CO_JOIN(CL_LAMBDA_TYPE_, __LINE__)) \
        static cort_proto* cort_start_static(cort_proto* this_ptr){return ((cort_type*)this_ptr)->start();}\
        \
        cort_proto* start() { \
            CO_BEGIN \
                CO_REMOVE_PARENTHESIS(co_lambda) \
            CO_END \
        }\
    }

#endif
