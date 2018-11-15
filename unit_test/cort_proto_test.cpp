#ifdef CORT_PROTO_TEST
//Suppose we rent the cpu time in a strange old super computer and costing no more than 256 "clock" each time. 
//So computing is enabled only after allowed and we need to divide our computing task into several one.
//This is an example to simulate the IO operation that only enabled after select or (e)poll result.

//Now we want to get the nth fibonacci numbers: 0,1,1,2,3,5,8,13,21,34,55,89....
//But we implement a fool recursive algorithm to get the result.
//Try to divide the computing task to several simpler one to be finished in the old super computer. 

#include <stdio.h>
#include <stdlib.h>
#include <stack>
#include "../cort_proto.h"

std::stack<cort_proto*> scheduler_list;
unsigned char the_clock = 1;

void push_work(cort_proto* arg){
    scheduler_list.push(arg);
}
void pop_work(){
    scheduler_list.pop();
}

bool pop_execute_work(){
    if(scheduler_list.empty()){
        return false;
    }
    cort_proto* p = scheduler_list.top();
    scheduler_list.pop();
    p->resume();
    if(scheduler_list.empty()){
        return false;
    }
    return true;
}

struct fibonacci_cort;
struct fibonacci_cort : public cort_proto{
    fibonacci_cort *corts[2];
    int n;
    int result;

    //You can define anything.
    ~fibonacci_cort(){
    }
    fibonacci_cort(int input ): n(input){
    }

    CO_DECL(fibonacci_cort)
    
    //This is the coroutine entrance.
    cort_proto* start();
    
    cort_proto* test(int){return 0;}
};

cort_proto* fibonacci_cort::start(){
    CO_BEGIN 
        struct cort_tester :public cort_proto {
            CO_DECL(cort_tester)
            cort_proto* start() {
                return 0;
            }
        }tester;
        if(++the_clock == 0){   //Oh you are not enabled to work now.
            push_work(this);
            CO_AGAIN;
        }
        if(n < 2){
            result = n;
            CO_RETURN;
        }
        corts[0] = new fibonacci_cort(n-1);
        corts[1] = new fibonacci_cort(n-2);

        //They may cost much time so we should wait their result.
        
        if(false){
            //You can skip next await using CO_SKIP_AWAIT
            CO_SKIP_AWAIT;
        }
        //CO_AWAIT_ALL(corts[0], corts[1]); //You can place no more than ten corts for CO_AWAIT_ALL.
        //CO_AWAIT_RANGE(corts, corts+2);   //Or using forward iterator of coroutine pointer for variate count.
        //CO_AWAIT(corts[0]); 
        //CO_AWAIT(corts[1]);     //Or await one bye one. They must be put in different lines!
        
        //CO_AWAIT_ALL_IF(true, corts[0], corts[1]);
        //CO_AWAIT_ALL_IF(cond, cort) means:
        //if(!cond){CO_SKIP_AWAIT;}
        //CO_AWAIT_ALL(cort);
        
        struct cort_then_output: public fibonacci_cort{
            CO_DECL(cort_then_output)
            cort_proto* start(){
                printf("intput:%d, output:%d! \n", n, result);
                return 0;
            }
        };
        
        CO_THEN(cort_then_output, corts[0]); //When corts[0] finished, run as cort_then_output.
        CO_THEN(cort_then_output, corts[1]);
        CO_THEN_LB(this->corts[1], (return 0;));
        CO_THEN_LB(this->corts[1], (printf("intput:%d, output:%d! \n", n, result);));
        //Or you can simply
        //CO_THEN(cort_then_output, corts[0], corts[1]);
        //Or if c++11 supported
        //corts[0]->then<cort_then_output>(); corts[1]->then<cort_then_output>();

        CO_AWAIT_ALL(corts[0], corts[1], &tester, &tester);


        //CO_AWAIT_RANGE_IF(true, corts, corts+2);
        //CO_AWAIT_IF(true, corts[0]); 
        //CO_AWAIT_IF(true, corts[1]);    

        if(++the_clock == 0){   //Oh you are not enabled to work now.
            push_work(this);
            CO_AGAIN;
        }
        result = corts[0]->result + corts[1]->result;
        delete corts[0];
        delete corts[1];
        
        //Pure test
        CO_IF(true)
        CO_ELSE
        CO_ELSE_END

        CO_IF(true)
        CO_IF_END

        CO_IF(false)
        CO_IF_END

        CO_IF(true)
            CO_ASYNC_LB(cort_proto, (printf("test:%d\n", 0);));
        CO_IF_END

        CO_IF(false)
        CO_ELSE_IF(true)
            CO_IF(true)
                CO_IF(the_clock == 1)
                    CO_AWAIT_LB(cort_proto, (printf("test1:%d\n", result);), result);
                CO_ELSE
                    CO_AWAIT_LB(cort_proto, (printf("test2:%d\n", result);), result);
                CO_ELSE_END
            CO_IF_END
        CO_ELSE_END

        CO_IF(false)
        CO_ELSE_IF(the_clock == 1)
            CO_AWAIT_LB(cort_proto, (printf("test double3:%d, %d\n", result, n);), result,n);
        CO_ELSE
            CO_AWAIT_LB(cort_proto, (printf("test double4:%d, %d\n", result, n);), result,n);
        CO_ELSE_END

        CO_IF(true)
            CO_IF(false)
            CO_ELSE
                //The second parameter is called before next loop condition test.
                CO_WHILE(the_clock%2 == 1, ++the_clock)
                    CO_AWAIT_LB(cort_proto, (printf("test double5:%d, %d\n", result, n);), result,n);
                    printf("");
                CO_WHILE_END
            CO_ELSE_END
        CO_IF_END

        CO_WHILE(false)
            //You can ignore the second parameter. But infinite "loop" may lead to stack over flow,
            //if and only if it never "yields" in the "loop body"
            //"Loop back" always leads to a function call.
            CO_AWAIT_LB(cort_proto, (printf("test double6:%d, %d\n", result, n);), result,n);
        CO_WHILE_END
    CO_END
}

int main(int argc, char* argv[]){ 
    if(argc <= 1){
        argc = 18;
    }
    else{
        argc = atoi(argv[1]);
    }
    fibonacci_cort main_task(argc);
    main_task.start();
    while(pop_execute_work()){ //Generator mode? Just a toy.
        //sleep(1);
    }
    printf("result is: %d\n", main_task.result);
    return 0;
}
#endif
