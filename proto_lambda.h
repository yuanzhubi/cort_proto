#if !defined(PT_LAMBDA_H_)
#define PT_LAMBDA_H_

#define PL_JOIN3(x,y) x##y 
#define PL_JOIN2(x,y) PL_JOIN3(x,y)
#define PL_JOIN(x,y) PL_JOIN2(x,y)

#if defined(__GNUC__)
#define PL_TYPE_OF(x) __typeof__(x)
#else
#include <type_traits>
#define PL_TYPE_OF(x) std::remove_reference<decltype(x)>::type
#endif

#define PL_GET_EMPTY(...)
#define PL_FE_0(co_call, x, ...) co_call(x)
#define PL_FE_1(co_call, x, ...) co_call(x) PL_FE_0(co_call, __VA_ARGS__)
#define PL_FE_2(co_call, x, ...) co_call(x) PL_FE_1(co_call, __VA_ARGS__)
#define PL_FE_3(co_call, x, ...) co_call(x) PL_FE_2(co_call, __VA_ARGS__)
#define PL_FE_4(co_call, x, ...) co_call(x) PL_FE_3(co_call, __VA_ARGS__)
#define PL_FE_5(co_call, x, ...) co_call(x) PL_FE_4(co_call, __VA_ARGS__)
#define PL_FE_6(co_call, x, ...) co_call(x) PL_FE_5(co_call, __VA_ARGS__)
#define PL_FE_7(co_call, x, ...) co_call(x) PL_FE_6(co_call, __VA_ARGS__)
#define PL_FE_8(co_call, x, ...) co_call(x) PL_FE_7(co_call, __VA_ARGS__)
#define PL_FE_9(co_call, x, ...) co_call(x) PL_FE_8(co_call, __VA_ARGS__)

#define PL_GET_NTH_ARG_E(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N

#define PL_PARAMS(...) __VA_ARGS__
#define PL_EXPAND(x) x
#define PL_REMOVE_PARENTHESIS(x) PL_EXPAND(PL_PARAMS x)


#define PL_FOR_EACH_E(x, ...) \
    PL_EXPAND(PL_GET_NTH_ARG_E(YUANZHU_NEED_DONATE, ##__VA_ARGS__, PL_FE_9, PL_FE_8, PL_FE_7, PL_FE_6, PL_FE_5, PL_FE_4, PL_FE_3, PL_FE_2, PL_FE_1, PL_FE_0, PL_GET_EMPTY)(x, ##__VA_ARGS__))
    
#define PL_TYPEDEF(x) typedef PL_TYPE_OF(x) PL_JOIN(PL_TYPE_, x);
#define PL_DEF_MEMBER(x) PL_JOIN(PL_TYPE_, x) x;
#define PL_DEF_MEMBER_ARG(x) proto_lambda::co_arg_type<PL_JOIN(PL_TYPE_, x)>::type PL_JOIN(arg_, x),
#define PL_ASSIGN(x) ,x(PL_JOIN(arg_, x))
#define PL_COMMA_BACK(x) x,

namespace proto_lambda{
    template <typename T, bool is_too_large = (sizeof(T)>16) >
    struct co_arg_type{
        typedef T type;
    };

    template <typename T>
    struct co_arg_type<T, true>{ //大于16字节的，用引用拷贝传参。
        typedef const T& type;
    };

    struct co_empty{};
}

#define PL_DEF_LAMBDA_STRUCT(base_type, return_type, co_lambda,  ...) \
PL_FOR_EACH_E(PL_TYPEDEF, ##__VA_ARGS__) \
struct PL_JOIN(PL_LAMBDA_TYPE_, __LINE__):public base_type{\
    PL_FOR_EACH_E(PL_DEF_MEMBER, ##__VA_ARGS__) \
    PL_JOIN(PL_LAMBDA_TYPE_, __LINE__) (PL_FOR_EACH_E(PL_DEF_MEMBER_ARG, ##__VA_ARGS__) ...): /*... is used for the possible last comma*/\
        base_type() PL_FOR_EACH_E(PL_ASSIGN, ##__VA_ARGS__){} \
    return_type operator() PL_REMOVE_PARENTHESIS(co_lambda) \
}

#define PT_LAMBDA_BASE(base_type, name, return_type, co_lambda, ...) PL_DEF_LAMBDA_STRUCT(base_type, return_type, PL_EXPAND(co_lambda), ##__VA_ARGS__)  name( PL_FOR_EACH_E(PL_COMMA_BACK, ##__VA_ARGS__) 0);
#define PT_LAMBDA_BASE_NEW(base_type, name, return_type, co_lambda, ...) PL_DEF_LAMBDA_STRUCT(base_type, return_type, PL_EXPAND(co_lambda), ##__VA_ARGS__) *name = new PL_JOIN(PL_LAMBDA_TYPE_, __LINE__)(__VA_ARGS__);

#define PT_LAMBDA(name, return_type, co_lambda, ...) PT_LAMBDA_BASE(proto_lambda::co_empty, name, return_type, co_lambda, ##__VA_ARGS__)
#define PT_LAMBDA_NEW(name, return_type, co_lambda, ...) PT_LAMBDA_BASE_NEW(proto_lambda::co_empty, name, return_type, co_lambda, ##__VA_ARGS__)

#endif