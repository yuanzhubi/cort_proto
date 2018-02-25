#ifndef CORT_STACKFUL_FDS_WAITER_H_
#define CORT_STACKFUL_FDS_WAITER_H_
#include "../cort_timeout_waiter.h"
#include "cort_stackful.h"

struct cort_hook_fd_info{
    operator int() const{return fd;}
    int fd;
    uint32_t read_timeout;
    uint32_t write_timeout;
};
struct cort_stackful_fds_waiter: public cort_fd_waiter, public cort_stackful{ 
    CO_DECL_STACKFUL_PROTO(cort_stackful_fds_waiter)
    void before_stackful_start();
    void before_stackless_resume();
    void after_stackful_resume();
    
    cort_hook_fd_info *fds_array;
    
    void wait_fd(int fd){
        if(fds_array == 0){
            reserved_count = 1;
            fds_array = (cort_hook_fd_info*)malloc(sizeof(cort_hook_fd_info) * 6);
            fds_array[0].fd = fd;
            fds_array[0].read_timeout = 0;
            fds_array[0].write_timeout = 0;
            fds_array[1].fd = 0;
            fds_array[2].fd = 0;
            fds_array[3].fd = 0;
            fds_array[4].fd = 0;
            fds_array[5].fd = -1;
        }
        else{
            if(fds_array[reserved_count].fd == -1){
                fds_array = (cort_hook_fd_info*)realloc(fds_array, sizeof(cort_hook_fd_info) * (reserved_count+5)); //linear increase the size.             
                fds_array[reserved_count+1].fd = 0;
                fds_array[reserved_count+2].fd = 0;
                fds_array[reserved_count+3].fd = 0;
                fds_array[reserved_count+4].fd = 0;
                fds_array[reserved_count+5].fd = -1;    
            }
            fds_array[reserved_count].fd = fd;
            fds_array[reserved_count].read_timeout = 0;
            fds_array[reserved_count].write_timeout = 0;
            ++reserved_count;
        }
    }
    void remove_fd(int fd){
        uint16_t total_count = reserved_count;
        for(uint i = 0; i< total_count; ++i){
            if(fds_array[i].fd == fd){
                --reserved_count;
                fds_array[i] = fds_array[reserved_count];               
            }
        }
    }
    cort_hook_fd_info *find_fd_waiter(int fd) const{
        uint16_t total_count = reserved_count;
        for(uint i = 0; i< total_count; ++i){
            if(fds_array[i].fd == fd){
                return fds_array + i;           
            }
        }
        return 0;
    }
    cort_stackful_fds_waiter();
    ~cort_stackful_fds_waiter();
};          

 
#include <sys/select.h>
#include <sys/epoll.h>
#include <sys/stat.h>                                                       
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <poll.h>
extern "C"{
    extern int cort_hooked_socket(int domain, int type, int protocol);
    
    extern ssize_t cort_hooked_read(int fd, void *buf, size_t count);
    extern ssize_t cort_hooked_readv(int fd, const struct iovec *iov, int iovcnt);
    extern ssize_t cort_hooked_recv(int fd, void *buf, size_t len, int flags);
    extern ssize_t cort_hooked_recvfrom(int fd, void *buf, size_t len, int flags,
            struct sockaddr *src_addr, socklen_t *addrlen);
    extern ssize_t cort_hooked_recvmsg(int fd, struct msghdr *msg, int flags);
    
    extern ssize_t cort_hooked_write(int fd, const void *buf, size_t count);
    extern ssize_t cort_hooked_writev(int fd, const struct iovec *iov, int iovcnt);
    extern ssize_t cort_hooked_send(int fd, const void *buf, size_t len, int flags);
    extern ssize_t cort_hooked_sendto(int fd, const void *buf, size_t len, int flags,
            const struct sockaddr *dest_addr, socklen_t addrlen);
    extern ssize_t cort_hooked_sendmsg(int fd, const struct msghdr *msg, int flags);
    
    extern int cort_hooked_connect(int fd, const struct sockaddr *addr, socklen_t addrlen);
    extern int cort_hooked_accept(int fd, struct sockaddr *addr, socklen_t *addrlen);
    
    
    extern void cort_hooked_usleep(int usec);
    extern int cort_hooked_epoll_wait(int fd, struct epoll_event *events, int maxevents, int timeout);
    extern int cort_hooked_poll(struct pollfd *fds, nfds_t nfds, int timeout);
    extern int cort_hooked_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
    
    extern int cort_hooked_close(int fd);
    extern int cort_hooked_setsockopt(int fd, int level, int optname, struct timeval *optval, socklen_t optlen);
}



#endif