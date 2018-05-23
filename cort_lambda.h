#include "proto_lambda.h"

#define CO_PL_DEF_LAMBDA_STRUCT(base_type, return_type, co_lambda,  ...) \
PL_FOR_EACH_E(PL_TYPEDEF, ##__VA_ARGS__) \
struct PL_JOIN(PL_LAMBDA_TYPE_, __LINE__):public base_type{\
    PL_FOR_EACH_E(PL_DEF_MEMBER, ##__VA_ARGS__) \
    PL_JOIN(PL_LAMBDA_TYPE_, __LINE__) (PL_FOR_EACH_E(PL_DEF_MEMBER_ARG, ##__VA_ARGS__) ...): /*... is used for the possible last comma*/\
        base_type() PL_FOR_EACH_E(PL_ASSIGN, ##__VA_ARGS__){} \
    CO_DECL(PL_JOIN(PL_LAMBDA_TYPE_, __LINE__)) \
    enum{is_auto_delete = true}; \
    return_type on_finish(){return_type result = base_type::on_finish();if(result == 0 && !base_type::is_auto_delete){delete this;} return result;} \
    return_type start() { \
        CO_BEGIN \
            PL_REMOVE_PARENTHESIS(co_lambda) \
        CO_END \
    }\
}

#define CO_ASYNC_LB(base_type, co_lambda,...) CO_PL_DEF_LAMBDA_STRUCT(base_type, cort_proto*, PL_EXPAND(co_lambda), ##__VA_ARGS__);  (new PL_JOIN(PL_LAMBDA_TYPE_, __LINE__)(__VA_ARGS__))->start();
#define CO_AWAIT_LB(base_type, co_lambda,...) CO_PL_DEF_LAMBDA_STRUCT(base_type, cort_proto*, PL_EXPAND(co_lambda), ##__VA_ARGS__); CO_AWAIT(new PL_JOIN(PL_LAMBDA_TYPE_, __LINE__)(__VA_ARGS__));