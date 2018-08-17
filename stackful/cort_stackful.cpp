#include "cort_stackful.h"

static __thread cort_stackful* current_thread_coroutine = 0;

cort_stackful* cort_stackful::get_current_thread_cort(){
    return current_thread_coroutine;
}

void cort_stackful::set_current_thread_cort(cort_stackful* arg){
    current_thread_coroutine = arg;
}

cort_proto* cort_stackful::stackful_start(cort_proto* cort, cort_proto::run_type func_address){
    size_t sp_top = (size_t)(stack_base + stack_size);
    return cort_stackful_start(cort, &cort_stackless_sp_addr, 
    #if defined(__x86_64__) 
        sp_top - ((sp_top & 0x0f) ^ ((1*8)&0x0f))   //We need to save &cort_stackless_sp_addr in the stack before calling to func_address, which costs 1*8 bytes.
    #elif   defined(__i386__)
        sp_top - ((sp_top & 0x0f) ^ ((2*4)&0x0f))   //We need to save &cort_stackless_sp_addr and "this" pointer in the stack before calling to func_address, which costs 2*4 bytes.
    #else
    #error "Stackful_coroutine is now only supported in x86&x64. You can submit your own implement in your platform!"
    #endif
    ,func_address
    );
}


#include <vector>

static const uint32_t coroutine_version_list_delta = 8;

static std::vector<int32_t> version_list(1, 1); //The non-positive elements means they have been used but free for future.

void cort_stackful_local_storage_meta::cort_stackful_local_storage_register(){
    int32_t last_index = version_list[0];
    if(last_index >= (int32_t)version_list.size()){
        version_list.resize(last_index + coroutine_version_list_delta);
    }
    this->offset = last_index;
    
    int32_t& current_ver = version_list[last_index];
    //& is for the smallest negative number x, that -x = x. So we elimate the sign bit which make -x = 0.
    current_ver = ((-current_ver) & (~(1<<(sizeof(current_ver)*8 - 1)))) + 1; 
    this->version = current_ver;

    const int32_t current_size = (int32_t)version_list.size();
    while(++last_index < current_size && version_list[last_index] > 0);
    version_list[0] = last_index;
}

void cort_stackful_local_storage_meta::cort_stackful_local_storage_unregister(){
    int32_t& last_index = version_list[0];
    int32_t loffset = this->offset;
    int32_t& lversion = version_list[loffset];
    
    lversion = -lversion;
    
    if(last_index > loffset){
        last_index = loffset;
    }
}

void** cort_stackful::get_local_storage(const cort_stackful_local_storage_meta& meta_data)
{
    static const size_t cort_stackful_local_storage_delta = sizeof(void*);
    
    //We do not allocate the coroutine local storage from the stack base 
    //because it may lead to page fault or growing overflow.
    
    size_t need_capacity = meta_data.offset + 1;
    if(cort_stackful_local_storage == 0){
        need_capacity = ((need_capacity + cort_stackful_local_storage_delta - 1) & (~(cort_stackful_local_storage_delta - 1)));
        cort_stackful_local_storage = (cort_stackful_local_storage_data*)calloc(sizeof(*cort_stackful_local_storage), need_capacity);
        cort_stackful_local_storage->pointer = (void*)need_capacity;
        cort_stackful_local_storage[meta_data.offset].version = meta_data.version;            
    }else if(need_capacity > (size_t)(cort_stackful_local_storage->pointer) ){
        size_t prev = (size_t)(cort_stackful_local_storage->pointer) ;
        need_capacity = ((need_capacity + cort_stackful_local_storage_delta - 1) & (~(cort_stackful_local_storage_delta - 1)));
        cort_stackful_local_storage = (cort_stackful_local_storage_data*)realloc(cort_stackful_local_storage, sizeof(*cort_stackful_local_storage) * need_capacity);
        memset(cort_stackful_local_storage + prev, 0, (need_capacity - prev) * sizeof(*cort_stackful_local_storage));
        cort_stackful_local_storage->pointer  = (void*)need_capacity;
        cort_stackful_local_storage[meta_data.offset].version = meta_data.version;
    }
    
    cort_stackful_local_storage_data& result = cort_stackful_local_storage[meta_data.offset];
    if(meta_data.version != result.version){
        result.pointer = meta_data.init_value;
        result.version = meta_data.version;
    }
    
    return &(result.pointer);
}