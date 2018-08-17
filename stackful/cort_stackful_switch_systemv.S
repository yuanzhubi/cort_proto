#if defined(__x86_64__) 

.text
.globl cort_stackful_switch
.type  cort_stackful_switch, @function
.align 16
cort_stackful_switch:
/* src_sp_addr@rdi, dest_sp_addr@rsi, return_result@rdx */

    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq  %rsp, (%rdi)                                                                                                           
    movq  (%rsi), %rsp                                                                                                          
                                                                                                                                 
    popq  %r15                                                                                                                   
    popq  %r14                                                                                                                   
    popq  %r13                                                                                                                   
    popq  %r12                                                                                                                   
    popq  %rbx                                                                                                                   
    popq  %rbp                                                                                                                   
    
    movq  %rdx, %rax
    retq                                                                                                                           
                                                                                                                                                                                                                                                       
.size cort_stackful_switch,.-cort_stackful_switch 


.text
.globl cort_stackful_start
.type  cort_stackful_start, @function
.align 16
cort_stackful_start:
/*  cort@rdi, src_sp_addr@rsi, dest_sp_value@rdx, start_func@rcx*/

    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq  %rsp, (%rsi)     /*cort_stackful_switch can use the address to return*/                                                                                                      
    movq  %rdx, %rsp       /*%rdx should be 16x+8*/
    pushq %rsi
    call  *%rcx                                                                                                                                                                                                                              
    movq  (%rsp), %rsi     /*We should return as same form of cort_stackful_switch*/                                                                                                                                                                                                                        
    movq  (%rsi), %rsp                                                                                                          
                                                                                                                                 
    popq  %r15                                                                                                                   
    popq  %r14                                                                                                                   
    popq  %r13                                                                                                                   
    popq  %r12                                                                                                                   
    popq  %rbx                                                                                                                   
    popq  %rbp                                                                                                                   
    
    xorq  %rax, %rax
    retq                                                                                                                                          
                                                                                                                            
.size cort_stackful_start,.-cort_stackful_start 



#elif   defined(__i386__)

.text
.globl cort_stackful_switch
.type  cort_stackful_switch, @function
.align 2
cort_stackful_switch:
/* from@4(%ESP), to@8(%ESP), return_result@0x0c(%ESP)  */

    pushl  %ebp
    pushl  %ebx
    pushl  %edi
    pushl  %esi

    movl  0x14(%esp), %eax
    movl  0x18(%esp), %edx
    movl  %esp, (%eax)
    movl  0x1c(%esp), %eax
    movl  (%edx), %esp
                                                                                                                                 
    popl  %esi                                                                                                                   
    popl  %edi                                                                                                               
    popl  %ebx                                                                                                                   
    popl  %ebp 
    
    ret                                                                                                                  
                                                                                                                                 
.size cort_stackful_switch,.-cort_stackful_switch 

.text
.globl cort_stackful_start
.type  cort_stackful_start, @function
.align 2
cort_stackful_start:
/* this@4(%ESP), from_save_addr@8(%ESP), to_value@0x0c(%ESP), start_func@0x10(%ESP)  */

    pushl  %ebp
    pushl  %ebx
    pushl  %edi
    pushl  %esi

    movl  0x14(%esp), %ebx          /* now we can use callee saved register free */
    movl  0x18(%esp), %eax
    movl  0x1c(%esp), %edx
    movl  0x20(%esp), %ecx
    
    movl  %esp, (%eax)
    movl  %edx, %esp                 /* %edx should be 16x+2*4 */
    pushl %eax 
    pushl %ebx
    call  *%ecx
    movl  4(%esp), %edx                                                                                                                                                                                                                           
    movl  (%edx),  %esp        
                                                                                                                                 
    popl  %esi                                                                                                                   
    popl  %edi                                                                                                               
    popl  %ebx                                                                                                                   
    popl  %ebp 
    
    xorl  %eax, %eax
    ret                                                                                                                  
                                                                                                                                 
.size cort_stackful_start,.-cort_stackful_start 

#endif
