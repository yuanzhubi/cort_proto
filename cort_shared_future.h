#ifndef CORT_SHARED_FUTURE_H_
#define CORT_SHARED_FUTURE_H_

#include <stdexcept>

#include "cort_proto.h"
#include "cort_util.h"


//cort_shared_future can be awaited by more than one parent coroutines!
//The parent coroutines are resumed without any pre-defined order!
struct cort_shared_future : public cort_proto{
    CO_DECL(cort_shared_future);
private:
    size_t& ref_count(){
        return data10.ref_count;
    }
    
    //We steal the data10 for storing the waiters.
    template<typename T>
    void then(); 
    void then(run_type then_function);
    run_type then()const;
    
    
    typedef cort_pod_pool<cort_proto*> cort_ptr_pool;
    cort_ptr_pool parent_list;
    cort_ptr_pool* get_parent_list(){
        return &parent_list;
    }
public:
    void add_parent(cort_proto* arg){
        if(arg != 0){
            parent_list.push_back(arg);
        }   
    }
    
    //Through we do not use std::enable_shared_from_this but support intrusive reference count, 
    //we need to pay attention to the count in member function because we have "call back" behaviour.
    //When we should increase the count before the callback function calling to avoid this pointer become invalid,
    //if the reference count is not zero at the beginning of the member function.
    cort_proto* start(){
    CO_BEGIN_THIS
        if(get_wait_count() == 0){
           CO_RETURN;
        }
        if(get_parent() != 0){
           add_parent(get_parent());
           remove_parent();
        }
        CO_YIELD();            
        set_wait_count(0);
       
        bool is_using_ref_count = (ref_count() != 0);
        if(is_using_ref_count){
            this->add_ref();
        }
        
        cort_proto* cort_parent;
        while((cort_parent = get_parent() ) != 0){
            remove_parent();
            cort_parent->try_resume(this);
            if(get_wait_count() != 0){ //New waiting is added?
                if(is_using_ref_count){
                    this->release();
                }
                CO_AGAIN;
            }
        }

        while(!parent_list.empty() ){  
            cort_proto* p = parent_list.pop_back();
            p->try_resume(this);
            if(get_wait_count() != 0){
                if(is_using_ref_count){
                    this->release();
                }
                CO_AGAIN;
            }
        }

        if(is_using_ref_count){
            this->release();
        }
    CO_END
    }
    
    //cort_shared_future may be strongly referenced by different coroutines, so it is difficult to manage its life cycle.
    //If you use "operator new" or "create" to create cort_shared_future and use add_ref/release function to safely manage it, 
    //like using "p->release();" instead of "delete p;".   
    static cort_shared_future* create(){
        return new cort_shared_future();
    }
    
    void add_ref(){
        ++ref_count();
    }
    
    void release(){
        if(ref_count() > 1){
            --ref_count();
        }else if(get_wait_count() != 0){
            throw(std::logic_error("This coroutine is waiting other coroutine but to be deleted! Missing add_ref operation in some coroutines or thread violation happend!"));
        }else{
            delete this;
        }            
    }
};

#endif
