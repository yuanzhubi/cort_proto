#!/bin/bash
#You can add your own hook_function xxx here, then implement  "cort_hooked_xxx" and link.

hook_functions=(socket read readv recv recvfrom recvmsg write writev send sendto sendmsg connect accept usleep epoll_wait poll select close setsockopt)
for function_name in ${hook_functions[@]}
do
    objcopy --redefine-sym ${function_name}=cort_hooked_${function_name} $@ 
done
