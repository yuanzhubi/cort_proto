



//Suppose we rent the cpu time in a strange old super computer and costing no more than 256 "clock" each time for example. 
//So computing is enabled only after allowed and we need to divide our computing task into several one.
//This is an example to simulate the IO operation that only enabled after select or (e)poll result.

//Now we want to get the nth fibonacci numbers: 0,1,1,2,3,5,8,13,21,34,55,89....
//But we implement a fool recursive algorithm to get the result.
//Try to divide the computing task to several simpler one to be finished in the old super computer. 

#include <stdio.h>
#include <stdlib.h>
#include <stack>
#include "cort_proto.h"
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
struct fibonacci_cort : public cort_multi_awaitable{
    
    fibonacci_cort *corts[2];
    int n;
    int result;
    ~fibonacci_cort(){

    }
    CO_BEGIN(fibonacci_cort)
        
        if(++the_clock == 0){   //Oh you are not enabled work now.
            push_work(this);
            CO_AWAIT_AGAIN();
        }
    
        if(n < 2){
            result = n;
            //When the coroutine ended you can use data0 to send_back some information.
            //Now we use it to tell the parent coroutine: it is finished without any further sub-coroutine waiting..
            data0.result_ptr = 0;   
            CO_RETURN();
        }
        corts[0] = new fibonacci_cort();
        corts[0]->n = n - 1;
        corts[1] = new fibonacci_cort();
        corts[1]->n = n - 2;

        //They may cost much time so we should wait their result.

        //CO_AWAIT_ALL(corts[0], corts[1]); //You can place no more than ten corts to await.
        CO_AWAIT_RANGE(corts, corts+2);     //Or using forward iterator of coroutine pointer for variate count.
        
        if(++the_clock == 0){   //Oh you are not enabled work now.
            push_work(this);
            CO_AWAIT_AGAIN();
        }
        result = corts[0]->result + corts[1]->result;
        delete corts[0];
        delete corts[1];
    CO_END(fibonacci_cort)
};


int main(int argc, char* argv[]){
    fibonacci_cort main_task;
    if(argc <= 1){
        main_task.n = 18;
    }
    else{
        main_task.n = atoi(argv[1]);
    }
    main_task.start();
    while(pop_execute_work()){
    }
    printf("%d\n", main_task.result);
    return 0;
}