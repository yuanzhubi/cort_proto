#ifdef CORT_CHANNEL_TEST
//Suppose we rent the cpu time in a strange old super computer and costing no more than 256 "clock" each time. 
//So computing is enabled only after allowed and we need to divide our computing task into several one.
//This is an example to simulate the IO operation that only enabled after select or (e)poll result.

//Now we want to get the nth fibonacci numbers: 0,1,1,2,3,5,8,13,21,34,55,89....
//But we implement a fool recursive algorithm to get the result.
//Try to divide the computing task to several simpler one to be finished in the old super computer. 

#include <stdio.h>
#include <stdlib.h>
#include <string>

#include <iostream>
#include "../cort_proto.h"
#include "../cort_channel.h"

cort_channel<int> channel(38);
cort_channel<std::string> string_channel;
cort_event_channel event_channel(8);
int count = 0;

struct random_echo_cort : public cort_proto{
    int n;
    CO_DECL(random_echo_cort)
    
    cort_proto* start(){
        CO_BEGIN 
            //printf("%p\n", this);
            ++count;
            n = rand();
            if(n % 2 == 0){
                (new random_echo_cort())->start();
            }
            CO_AWAIT_IF(n % 2 == 0, &channel, push, n); //wait push finished
            CO_AWAIT_IF(n % 2 == 1, &channel);          //wait get&pop is enabled.
            if(n % 2 == 1){
                int *p = channel.get();
                if(p != 0){                             //get zero means wait failed
                   //printf("%d\n", *p);
                    channel.pop();
                }
            }

            CO_AWAIT_IF(n % 2 == 0, &event_channel, push, 1);  //produce
            CO_AWAIT_IF(n % 2 == 1, &event_channel);           //consume, wait get&pop is enabled.
            if(n % 2 == 1 &&  event_channel.get() != 0){              //get zero means wait failed
                event_channel.pop();
            }
            CO_AWAIT(&string_channel, push, "12345678");
        CO_END
    }
    
    enum{is_auto_delete = true}; 
    cort_proto* on_finish(){
        cort_proto::on_finish();
        delete this;
        return 0;
    }
};

#include <iostream>
int main(int argc, char* argv[]){ 
    puts("./a.out channel_buffer_size max_coroutine_count");
    int channel_buffer_size = 7;
    int max_coroutine_count = 1e4;
    if(argc > 1){
        channel_buffer_size = atoi(argv[1]);
    }
    if(argc > 2){
        max_coroutine_count = atoi(argv[2]);
    }
    channel.set_buffer_size(channel_buffer_size);
    
    while(count < max_coroutine_count){
        (new random_echo_cort())->start();
        //channel.display(std::cout);
    }

    channel.close();    //This gurantees to triger all the waiting coroutines.
    event_channel.close();
    string_channel.close();
    count = 0;
    return 0;
}
#endif
