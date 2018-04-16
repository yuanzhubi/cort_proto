#ifndef CORT_CHANNLE_H_
#define CORT_CHANNLE_H_
#include "cort_proto.h"
#include "cort_util.h"
#include <assert.h>

template<typename T>
struct cort_channel_proto: public cort_proto{ 
private:
    cort_channel_proto(const cort_channel_proto&);
    cort_channel_proto& operator=(const cort_channel_proto&);
protected:
    cort_pod_pool<T> objects;    
    cort_ptr_pool recvers;
public:
    CO_DECL(cort_channel_proto, wait_popable);
    
    void push(const T& arg){
        objects.push_back(arg);
        cort_proto* waiter = this->get_parent();
        if(waiter != 0){
            this->set_parent(0);
            ++data0.result_int;
            waiter->decr_wait_count(1);
        }
        else if(!recvers.empty()){
            waiter = (cort_proto*)recvers.pop_back();
            ++data0.result_int;
            waiter->decr_wait_count(1);
        }
    }

    T* pop(){
        assert(!objects.empty() 
            && data0.result_int != 0); // Some coroutines pop before wait_popable? This is a check.
        T* last = &objects.pop_back();
        --data0.result_int;
        return last;
    }
    
    cort_proto* wait_popable(){
        if(objects.size() > data0.result_int){
            ++data0.result_int;
            return 0;
        }
        cort_proto* waiter = this->get_parent();
        if(waiter != 0){
            recvers.push_back(waiter);
        }
        return this;
    }
};

struct cort_channel: public cort_proto{ 
private:
    cort_channel(const cort_channel&);
    cort_channel& operator=(const cort_channel&);
protected:
    unsigned int objects;    
    cort_ptr_pool recvers;
public:
    CO_DECL(cort_channel, wait_popable);
    cort_channel(){
        objects = 0;
    }
    void push(){
        ++objects;
        cort_proto* waiter = this->get_parent();
        if(waiter != 0){
            this->set_parent(0);
            ++data0.result_int;
            waiter->decr_wait_count(1);
        }
        else if(!recvers.empty()){
            waiter = (cort_proto*)recvers.pop_back();
            ++data0.result_int;
            waiter->decr_wait_count(1);
        }
    }

    void pop(){
        assert(objects != 0
            && data0.result_int != 0); // Some coroutines pop before wait_popable? This is a check.
        --objects;
        --data0.result_int;
    }
    
    cort_proto* wait_popable(){
        if(objects > data0.result_int){
            ++data0.result_int;
            return 0;
        }
        cort_proto* waiter = this->get_parent();
        if(waiter != 0){
            recvers.push_back(waiter);
        }
        return this;
    }
}; 
#endif