#ifndef CORT_CHANNEL_H_
#define CORT_CHANNEL_H_
#include "cort_proto.h"
#include "cort_util.h"

struct cort_channel_proto : public cort_proto{
public:    
    const static size_t infinite_buffer = size_t(-1);
    const static size_t no_buffer = 0;
private:
    //We steal the data10 for storing the waiters.
    template<typename G>
    void then(); 
    void then(run_type then_function);
    run_type then()const;
    
protected:
    cort_pod_queue<cort_proto*> recvers;
    cort_pod_queue<cort_proto*> senders;
     
    cort_base simulate_cort_proto_for_sender;   
    cort_proto* get_last_sender(){
        return (cort_proto*)(&simulate_cort_proto_for_sender);
    }
    
    size_t& pinned_object_count(){
        return data10.pinned_object_count;
    }
  
public:
    const static size_t closed_buffer = size_t(-2);
    
    cort_channel_proto(size_t default_buffer_size = no_buffer){
        *(size_t*)(&simulate_cort_proto_for_sender) = default_buffer_size; //We use virtual function table pointer of simulate_cort_proto_for_sender.
        get_last_sender()->remove_parent();
    }
    //We use the vtable pointer to save the information.
    //If buffer_size > get_buffer_size(), some blocked producer will be try_resumed.  
    void set_buffer_size(size_t buffer_size){
        size_t old_count = get_buffer_size();
        *(size_t*)(&simulate_cort_proto_for_sender) = buffer_size;
        if(buffer_size > old_count){
            cort_proto *react_cort = get_last_sender();
            cort_proto *waiter = react_cort->get_parent();
            if(waiter != 0){
                react_cort->remove_parent();
                waiter->try_resume(this);
                --buffer_size;
            }
            while(buffer_size-- > old_count && !senders.empty()){
                waiter = senders.front();
                senders.pop_front();
                waiter->try_resume(this);
            }
        }    
    }
    
    size_t get_buffer_size() const{
        return *(size_t*)(&simulate_cort_proto_for_sender);
    }
    
    template <typename Subclass>    
    cort_proto* wait_popable_check(){
        if(is_closed()){
            return 0;
        }
        if(((Subclass*)this)->size() > pinned_object_count()){
            ++pinned_object_count();
            return 0;
        }
        cort_proto* waiter = this->get_parent(); //This is the last waiter.
        if(waiter != 0){
            this->remove_parent();
            recvers.push_back(waiter);  
        }
        return this;
    }
    
    bool is_closed() const {
        return (get_buffer_size() == closed_buffer);
    }
protected:
    //Produce
    //The function may return non-zero so you need to CO_AWAIT or CO_UNTIL.
    template <typename Subclass>
    cort_proto* after_push(size_t product_count_delta = 1){
        cort_proto* waiter = this->get_parent();
        if(waiter != 0){
            this->remove_parent();
            recvers.push_back(waiter);
        }
        while(product_count_delta-- != 0 && !recvers.empty()){
            waiter = recvers.front();
            recvers.pop_front();
            ++pinned_object_count();
            waiter->try_resume(this);
        }
        
        if(((Subclass*)this)->size() <= get_buffer_size()){
            return 0;
        }       
        cort_proto *react_cort = get_last_sender();
        waiter = react_cort->get_parent();
        if(waiter != 0){
            react_cort->remove_parent();
            senders.push_back(waiter);   
        }  
        return react_cort;
    }
    
    template <typename Subclass>
    bool test_try_push(size_t delta_count)  {
        if(is_closed()){
            return false;
        }
        return (((Subclass*)this)->size() + delta_count) < get_buffer_size();
    }
       
    //Consume
    //After "get", you need to pop. 
    //Or else one object will be leaked(it will be never accessed by get function), or consumed twice.
    template <typename Subclass>
    void after_pop(){             
        --pinned_object_count();
        if(((Subclass*)this)->size() < get_buffer_size()){
            return;
        }
        cort_proto *react_cort = get_last_sender();
        cort_proto* waiter = react_cort->get_parent();
        if(waiter != 0){
            react_cort->remove_parent();
            waiter->try_resume(this);
        }
        else if(!senders.empty()){
            waiter = senders.front();
            senders.pop_front();
            waiter->try_resume(this);
        }
    }
    
    template <typename Subclass>
    bool test_try_get() {
        if(is_closed()){
            return false;
        }
        return (pinned_object_count() < ((Subclass*)this)->size());
    }
    
    //This will awake all the awaiting producers/consumers.
    //Awake does not mean the awaiting coroutines will be resumed immediately because they may wait other coroutine.
    //Some consumer may receive NULL if the product is not enough when it is resumed.
    //If you push into a channel that has been "awake_all" already, the old consumer awaked before may receive the new product.
    //The new consumer will not receive NULL until the channel is closed again.
    template <typename Subclass>
    void awake_all(){     
        cort_proto *react_cort = get_last_sender();
        cort_proto *waiter;
        bool need_recheck;
        size_t& pinned_obj = pinned_object_count();
        do {
            need_recheck = false;           
            //Wake up senders 
            if((waiter = react_cort->get_parent()) != 0){
                react_cort->remove_parent();
                senders.push_back(waiter);
                need_recheck = true;
            }
            while(!senders.empty()){
                waiter = senders.front();
                senders.pop_front();
                waiter->try_resume(this);
                need_recheck = true;
            }
            
            //Wake up recvers
            if((waiter = this->get_parent()) != 0){
                this->remove_parent();
                recvers.push_back(waiter);
                need_recheck = true;
            }
            while(!recvers.empty()){
                waiter = recvers.front();
                recvers.pop_front();
                if(((Subclass*)this)->size() > pinned_obj){
                    ++pinned_obj;
                }
                waiter->try_resume(this);
                need_recheck = true;
            }
        }while(need_recheck);
    }
    
    //Like awake_all, but the channel can not be reused and the awaked consumer will receive NULL pointer.
    template <typename Subclass>
    void close(){  
        *(size_t*)(&simulate_cort_proto_for_sender) = closed_buffer;
        awake_all<Subclass>();
    }
};


template <class T, bool using_pod_queue = !cort_is_class_or_union<T>::result>
struct cort_queue_test{
    typedef cort_pod_queue<T> type;
};    

#include <deque>
template <class T>
struct cort_queue_test<T, false>{
    typedef std::deque<T> type;
};   

template<typename T, typename Container_T = typename cort_queue_test<T>::type > //If T is class or union, using std::deque or else cort_pod_queue.
struct cort_channel: public cort_channel_proto{ 
public:
    typedef Container_T objects_container_type;
protected:
    objects_container_type objects;    
    size_t size() const{
        return objects.size();
    }
    friend class cort_channel_proto;
    typedef cort_channel<T> channel_type;
public:
    CO_DECL(cort_channel, wait_popable);   
    cort_channel(size_t default_buffer_size = cort_channel_::no_buffer): cort_channel_proto(default_buffer_size){
    }
   
    //But you need to await the coroutine wait_popable before consume.
    T* get(){
        if(objects.size() == 0 || is_closed()){
            return 0;
        }
        return &objects.front();
    }
    
    T* try_get() {
        if(test_try_get<channel_type>()){
            wait_popable_check<channel_type>();
            return get(); 
        }
        return 0;
    }
    
    //Produce
    //If you resize(x) and x>0, the function may return non-zero so you need to CO_AWAIT or CO_UNTIL.
    cort_proto* push(const T& arg){
        if(is_closed()){
            return 0;
        }
        objects.push_back(arg);
        return after_push<channel_type>();
    }
    
    bool try_push(const T& arg){
        if(test_try_push<channel_type>(1)){
            push_back(arg);
            return true;
        }
        return false;
    }
    
    //Consume
    //After "get", you need to pop. 
    //Or else one object will be consumed twice.
    size_t pop(){ 
        size_t result = objects.size();      
        if(result != 0){           
            objects.pop_front();
            after_pop<channel_type>();
        }      
        return result;
    }
    
    cort_proto* wait_popable(){
        return cort_channel_proto::wait_popable_check<channel_type>();
    }
    
    void awake_all(){     
        cort_channel_proto::awake_all<channel_type>();
    }
    
    void close(){     
        cort_channel_proto::close<channel_type>();
    }
    
    template <typename G>
    void display(G& out, char sep = ' ') const {
        size_t total_size = size();
        for(size_t i = 0; i < total_size;){
            out << objects[i] ;
            if(++i < total_size){
                out << sep;
            }
        }
        out << "\r\n";
    }
};
   
//Unlike cort_channel, you can "produce"  virtual events for the consumers with cort_event_channel.
//So the argument of push is not the channel element, but the count(1 in default) of events.
struct cort_event_channel: public cort_channel_proto{   
    typedef cort_event_channel channel_type;
protected:
    size_t& event_count(){
        return data0.object_count;
    } 
    size_t size() const{
        return data0.object_count;
    }
    friend class cort_channel_proto;
    
public:
    CO_DECL(cort_event_channel, wait_popable);   
    cort_event_channel(size_t default_buffer_size = cort_channel_proto::no_buffer): cort_channel_proto(default_buffer_size){
    }
    
    //Produce
    //If you resize(x) and x>0, the function may return non-zero so you need to CO_AWAIT or CO_UNTIL.
    //You can produce one more by setting count_delta>1;
    cort_proto* push(size_t count_delta = 1){
        event_count() += count_delta;
        return after_push<channel_type>(count_delta);
    }
    
    bool try_push(size_t count_delta = 1){
        if(test_try_push<channel_type>(count_delta)){
            push(count_delta);
            return true;
        }
        return false;
    }

    //But you need to await the coroutine wait_popable before consume.
    size_t get() const{
        if(is_closed()){
            return 0;
        }
        return this->size();
    }
    
    size_t try_get() {
        if(test_try_get<channel_type>()){
            wait_popable();
            return get(); 
        }
        return 0;
    }
    
    size_t pop(){ 
        size_t result = event_count();
        if(result != 0){
            --event_count(); 
            after_pop<channel_type>();
        }
        return result;
    }
    
    cort_proto* wait_popable(){       
        return wait_popable_check<channel_type>();
    }
    
    void awake_all(){     
        return cort_channel_proto::awake_all<channel_type>();
    }
    
    void close(){     
        cort_channel_proto::close<channel_type>();
    }
}; 

#endif