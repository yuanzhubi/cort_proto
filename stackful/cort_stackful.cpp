
#include "cort_stackful.h"



cort_proto* cort_stackful::stackful_start(cort_proto* func_argument, cort_proto::run_type func_address){
	size_t sp_top = (size_t)(stack_base + stack_size);
	return cort_stackful_start(func_argument, &cort_stackless_sp_addr, 
	#if defined(__x86_64__) 
		sp_top - ((sp_top & 0x0f) ^ ((1*8)&0x0f))	//We need to save &cort_stackless_sp_addr in the stack before calling to func_address, which costs 1*8 bytes.
	#elif	defined(__i386__)
		sp_top - ((sp_top & 0x0f) ^ ((2*4)&0x0f))   //We need to save &cort_stackless_sp_addr and "this" pointer in the stack before calling to func_address, which costs 2*4 bytes.
	#else
	#error "Stackful_coroutine is now only supported in x86&x64. You can submit your own implement in your platform!"
	#endif
	,func_address
	);
}