#ifndef CORT_UTIL_H_
#define CORT_UTIL_H_

//We provide a quick alternative version of std::vector optimized for pod elements.
#include <stdlib.h>
#include <string.h> //for memcpy.

template<typename T, typename size_type = size_t>
struct cort_pod_pool{
private:
    T* ptr;
    size_type the_size;
    size_type the_capacity;

private:
    cort_pod_pool(const cort_pod_pool&);
    cort_pod_pool& operator=(const cort_pod_pool&);
    
public:
    size_type size() const {
        return the_size;
    }
    
    size_type capacity() const {
        return the_capacity;
    }
    
    bool empty() const {
        return the_size == 0;
    }
    
    cort_pod_pool(size_type init_capacity = 16){
        the_size = 0;
        the_capacity = init_capacity;
        ptr = (T*)malloc(sizeof(T) * init_capacity);
    }
    
    ~cort_pod_pool(){
        free(ptr);
    }
    
    T& back() const{
        return ptr[the_size - 1];
    }
    
    T pop_back(){
        return ptr[--the_size];
    }
    
    void push_back(const T& arg){
        if(the_size == the_capacity){
            if(the_size < 1024){
                ptr = (T*)realloc(ptr, sizeof(T)*(the_capacity *= 2));
            }
            else{
                ptr = (T*)realloc(ptr, sizeof(T)*(the_capacity += (the_capacity>>1)));
            }
        }   
        ptr[the_size++] = arg;
    }
    
    void pop_front(){
        ptr[0] = ptr[--the_size];
    }
    
    void pop_at(size_type pos){
        ptr[pos] = ptr[--the_size];
    }
};                                                          

template<typename T, typename size_type = size_t>
struct cort_pod_queue{
private:
    T* ptr;
    size_type the_size;
    size_type the_capacity;
    size_type the_begin;

private:
    cort_pod_queue(const cort_pod_queue&);
    cort_pod_queue& operator=(const cort_pod_queue&);
    
public:
    size_type size() const {
        return the_size;
    }
    
    bool empty() const {
        return the_size == 0;
    }
    
    const static size_type init_capacity = 16;
    cort_pod_queue(size_type capacity = init_capacity){
        the_size = 0;
        the_capacity = capacity;
        the_begin = 0;
        ptr = (T*)malloc(sizeof(T) * capacity); 
    }
    
    ~cort_pod_queue(){
        free(ptr);
    }
    
    #define CORT_MOD(i) i -= (the_capacity & ( -((the_capacity - i - 1) >> (sizeof(i) * 8 - 1))))    
    /*
    T& back() const{
        size_type index = (the_begin + the_size - 1);
        CORT_MOD(index);
        return ptr[index];
    }*/
    template <typename G>
    void display(G& out, char sep = ' ') const {
        for(size_type size = 0; size++  <      the_size;){
            out << ptr[(the_begin + size)%the_capacity] ;
            if(size < the_size){
                out << sep;
            }
        }
        out << "\r\n";
    }
    
    T& operator[](size_t offset) const{
        size_type index = the_begin + offset;
        CORT_MOD(index);
        return ptr[index];
    }
    
    T& front() const{
        return ptr[the_begin];
    }
    
    void push_back(const T& arg){
        push(arg);
    }
    
    void pop_front(){
        pop();
    }
    
    void pop(){
        --the_size;
        ++the_begin;
        CORT_MOD(the_begin);
    }
    
    void push(const T& arg){
        if(the_size == the_capacity){
            size_type capacity_delta = the_capacity >> 1;
            ptr = (T*)realloc(ptr, sizeof(T)*(the_capacity + capacity_delta));
            if(the_begin > capacity_delta){
                memcpy(ptr + capacity_delta + the_begin, ptr + the_begin, (the_capacity - the_begin) * sizeof(T)); 
                the_begin += capacity_delta;
            }
            else{
                memcpy(ptr + the_capacity, ptr, the_begin * sizeof(T)); 
            }
            the_capacity += capacity_delta;
        }   
        size_type index = the_begin + the_size++;
        CORT_MOD(index);
        ptr[index] = arg;
    }
    #undef CORT_MOD
}; 

template <typename T>
struct cort_is_class_or_union{
    typedef cort_is_class_or_union<T> this_type;
    
    template <typename G>
    static char is_class_test(int G::*);  
    
    struct char2 { char c[2]; };
    template <typename G>
    static char2 is_class_test(...);
    
    const static bool result = sizeof(is_class_test<T>(0)) == sizeof(char);
};
  
                                                         
  
#endif
