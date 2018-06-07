#ifndef CORT_FUTURE_H_
#define CORT_FUTURE_H_

#include "cort_proto.h"
#include "cort_util.h"

//cort_future can be awaited by more than one parent coroutines!
//The parent coroutines are resumed without any pre-defined order!
struct cort_future : public cort_proto{
    CO_DECL(cort_future);
private:
    unsigned int ref_count;
    
    //We steal the data10 for storing the waiters.
    template<typename T>
    void then(); 
    void then(run_type then_function);
    run_type then()const;
    
    //copy is not enabled!
    cort_future(const cort_future&);
    
    void add_parent(cort_proto* arg){
        if(data10.pdata == 0){
            data10.pdata = new cort_ptr_pool();
        }
        ((cort_ptr_pool*)(this->data10.pdata))->push_back(arg);
    }
    
public:
    cort_future() : cort_proto(){
        ref_count = 0;
    }
    cort_proto* start(){
        CO_BEGIN
            if(get_wait_count() == 0){
               CO_RETURN;
            }
            if(get_parent() != 0){
               add_parent(get_parent());
               set_parent(0);
            }
            CO_YIELD();
            set_wait_count(0);
            cort_ptr_pool* p = ((cort_ptr_pool*)this->data10.pdata);
            if(p != 0){
                while(!p->empty() ){  
                    ((cort_proto*)(p->pop_back()))->decr_wait_count(1, this);
                    if(get_wait_count() != 0){//有可能在某些父协程resume过程中重新给他添加等待
                        CO_AGAIN;
                    }
                }
                delete p;
                this->data10.pdata = 0;
            }
        CO_END
    }
    ~cort_future(){
        cort_ptr_pool* p = ((cort_ptr_pool*)this->data10.pdata);
        delete p;
    }
    
    //cort_future may be weakly referenced by different coroutines, so it is difficult to manage its life cycle, 
    //If you use "operator new" or "create" to create this coroutine, you can use add_ref/release function to safely manage it, like using "p->release();" instead of "delete p;".
    
    static cort_future* create(){
        return new cort_future();
    }
    
    void add_ref(){
        ++ref_count;
    }
    
    void release(){
        if(ref_count > 1){
            --ref_count;
        }else if(get_parent() != 0){
            throw("Fatal error! This coroutine is awaited but to be deleted!");
        }else{
            delete this;
        }            
    }
};

#endif