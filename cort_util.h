#ifndef CORT_UTIL_H_
#define CORT_UTIL_H_

template<typename T>
struct cort_pod_pool{
private:
    T* ptr;
    unsigned int the_size;
    unsigned int the_capacity;
public:

    unsigned int size() const {
        return the_size;
    }
    
    unsigned int capacity() const {
        return the_capacity;
    }
    
    bool empty() const {
        return the_size == 0;
    }
    
    cort_ptr_pool(){
        ptr = (T*)malloc(sizeof(T)*8);
        the_size = 0;
        the_capacity = 8;
    }
    
    cort_ptr_pool(unsigned int init_capacity){
        ptr = (T*)malloc(sizeof(T) * init_capacity);
        the_size = 0;
        the_capacity = init_capacity;
    }
    
    ~cort_ptr_pool(){
        free(ptr);
    }
    
    void push_back(const T& arg){
        if(the_size < the_capacity){
            ptr[the_size++] = arg;
        }else{
            ptr = (T*)realloc(ptr, sizeof(T)*(the_capacity *= 2));
            ptr[the_size++] = arg;
        }
    }
    
    T& back() const{
        return ptr[the_size - 1];
    }
    
    T& pop_back(){
        return ptr[--the_size];
    }
    
    void pop_at(unsigned int pos){
        ptr[pos] = ptr[--the_size];
    }
};

typedef cort_pod_pool<void*> cort_ptr_pool;                                                           
  
#endif