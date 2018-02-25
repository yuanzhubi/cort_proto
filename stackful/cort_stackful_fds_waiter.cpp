
#include "cort_stackful_fds_waiter.h"

#include <fcntl.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <map>
#include <stdarg.h>
#include <poll.h>
#include <errno.h>

#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>

static __thread cort_stackful_fds_waiter* current_cort_stackful_fds_waiter = 0;

cort_stackful_fds_waiter::cort_stackful_fds_waiter(){
    reserved_count = 0;
    fds_array = 0;
}

cort_stackful_fds_waiter::~cort_stackful_fds_waiter(){
    if(fds_array != 0){
        free(fds_array);
    }
}

void cort_stackful_fds_waiter::before_stackful_start(){
    current_cort_stackful_fds_waiter = this;
}

void cort_stackful_fds_waiter::after_stackful_resume(){
    current_cort_stackful_fds_waiter = this;
}

void cort_stackful_fds_waiter::before_stackless_resume(){
    current_cort_stackful_fds_waiter = 0;
}

static void set_non_block(int fd){  
    
    int flag = fcntl(fd, F_GETFL);
    int new_flag = flag | O_NONBLOCK;
    if(flag != new_flag){
        fcntl(fd, F_SETFL, new_flag);
    }
}

extern "C"{
    int cort_hooked_socket(int domain, int type, int protocol){
        int fd = socket(domain, type, protocol);
        cort_stackful_fds_waiter* waiter = current_cort_stackful_fds_waiter;
        if(waiter != 0 && fd > 0){
            waiter->wait_fd(fd);
            set_non_block(fd);
        }
        return fd;
    }
    
    #define CO_IO_HOOK_TEMPLATE(io_operation, pollevent, whattimeout) \
    cort_stackful_fds_waiter* waiter = current_cort_stackful_fds_waiter; \
    cort_hook_fd_info *fd_info; \
    if(waiter && (fd_info = (waiter->find_fd_waiter(fd)))){ \
        ssize_t result; \
        do{ \
            result = io_operation; \
            if(result < 0){ \
                int thread_errno = errno; \
                if(thread_errno != EAGAIN && thread_errno != EWOULDBLOCK){ \
                    if(thread_errno == EINTR){ \
                        continue; \
                    } \
                    break; \
                } \
            } \
            else{ \
                break; \
            }\
            if(waiter->is_timeout_or_stopped()){\
                errno = EAGAIN;\
                result = -1;\
                break; \
            }\
            bool new_timeout = false; \
            if(fd_info->whattimeout != 0 && !waiter->is_set_timeout()){ \
                waiter->set_timeout(fd_info->whattimeout); \
                new_timeout = true; \
            } \
            waiter->set_cort_fd(fd); \
            waiter->set_poll_request(pollevent); \
            cort_stackful_await(waiter); \
            waiter->remove_cort_fd();\
            if(waiter->is_timeout_or_stopped()){\
                errno = EAGAIN;\
                result = -1;\
                break; \
            }\
            if(new_timeout){ \
                waiter->clear_timeout(); \
            } \
        }while(true); \
        return result; \
    } \
    return io_operation; 
        
    ssize_t cort_hooked_read(int fd, void *buf, size_t count){
        CO_IO_HOOK_TEMPLATE(read(fd, buf, count), EPOLLIN, read_timeout);
    }
    ssize_t cort_hooked_readv(int fd, const struct iovec *iov, int iovcnt){
        CO_IO_HOOK_TEMPLATE(readv(fd, iov, iovcnt), EPOLLIN, read_timeout);
    }
    ssize_t cort_hooked_recv(int fd, void *buf, size_t len, int flags){
        CO_IO_HOOK_TEMPLATE(recv(fd, buf, len, flags), EPOLLIN, read_timeout);
    }
    ssize_t cort_hooked_recvfrom(int fd, void *buf, size_t len, int flags,
        struct sockaddr *src_addr, socklen_t *addrlen){
        CO_IO_HOOK_TEMPLATE(recvfrom(fd, buf, len, flags, src_addr, addrlen), EPOLLIN, read_timeout);
    }
    ssize_t cort_hooked_recvmsg(int fd, struct msghdr *msg, int flags){
        CO_IO_HOOK_TEMPLATE(recvmsg(fd, msg, flags), EPOLLIN, read_timeout);
    }
    
    ssize_t cort_hooked_write(int fd, const void *buf, size_t count){
        CO_IO_HOOK_TEMPLATE(write(fd, buf, count), EPOLLOUT, write_timeout);
    }
    ssize_t cort_hooked_writev(int fd, const struct iovec *iov, int iovcnt){
        CO_IO_HOOK_TEMPLATE(writev(fd, iov, iovcnt), EPOLLOUT, write_timeout);
    }
    ssize_t cort_hooked_send(int fd, const void *buf, size_t len, int flags){
        CO_IO_HOOK_TEMPLATE(send(fd, buf, len, flags), EPOLLOUT, write_timeout);
    }
    ssize_t cort_hooked_sendto(int fd, const void *buf, size_t len, int flags,
        const struct sockaddr *dest_addr, socklen_t addrlen){
        CO_IO_HOOK_TEMPLATE(sendto(fd, buf, len, flags, dest_addr, addrlen), EPOLLOUT, write_timeout);  
    }
    ssize_t cort_hooked_sendmsg(int fd, const struct msghdr *msg, int flags){
        CO_IO_HOOK_TEMPLATE(sendmsg(fd, msg, flags), EPOLLOUT, write_timeout);
    }
    
    int cort_hooked_connect(int fd, const struct sockaddr *addr, socklen_t addrlen){
        cort_stackful_fds_waiter* waiter = current_cort_stackful_fds_waiter;
        cort_hook_fd_info *fd_info;
        if(waiter && (fd_info = (waiter->find_fd_waiter(fd)))){         
            int status;
            connect_again:
            status = connect(fd, addr, addrlen);
            if(status != 0){
                int thread_errno = errno;
                if(thread_errno == EISCONN){
                    return 0;
                }
                else if(thread_errno != EINPROGRESS){
                    if(thread_errno == EINTR){
                        goto connect_again;
                    }
                    return status;
                }
            }else{
                return 0;
            }
            if(waiter->is_timeout_or_stopped()){
                errno = ETIMEDOUT;
                return -1;
            }
            bool new_timeout = false;
            if(fd_info->write_timeout != 0 && !waiter->is_set_timeout()){
                waiter->set_timeout(fd_info->write_timeout);
                new_timeout = true;
            }
                
            waiter->set_cort_fd(fd);
            waiter->set_poll_request(EPOLLOUT | EPOLLIN | EPOLLRDHUP);
            cort_stackful_await(waiter);
            uint32_t poll_event = waiter->get_poll_result();
            waiter->remove_cort_fd();
            
            if(waiter->is_timeout_or_stopped()){
                errno = ETIMEDOUT;
                return -1;
            }
            if(new_timeout){
                waiter->clear_timeout();
            }
            if( ((EPOLLHUP|EPOLLRDHUP|EPOLLERR) & poll_event) != 0){
                errno = ECONNREFUSED;
                return -1;
            }
            if((EPOLLOUT & poll_event) != 0){
                return 0;
            }
            
            int err = 0;
            socklen_t errlen = sizeof(err);
            getsockopt(fd,SOL_SOCKET,SO_ERROR, &err, &errlen);
            if(err) {
                errno = err;
            }
            else{
                errno = ETIMEDOUT;
            } 
            return -1;
        }
        return connect(fd, addr, addrlen);
    }
    
    int cort_hooked_accept(int fd, struct sockaddr *addr, socklen_t *addrlen){      
        cort_stackful_fds_waiter* waiter = current_cort_stackful_fds_waiter;
        cort_hook_fd_info *fd_info;
        if(waiter && (fd_info = (waiter->find_fd_waiter(fd)))){ 
            int result;
            do{             
                accept_again:
                result = accept(fd, addr, addrlen);
                if(result < 0){
                    int thread_errno = errno;
                    if(thread_errno != EAGAIN && EWOULDBLOCK != thread_errno){
                        if(thread_errno == EINTR){
                            goto accept_again;
                        }
                        break;
                    }
                }else{
                    break;
                }
                if(waiter->is_timeout_or_stopped()){
                    errno = EAGAIN;
                    return -1;
                }
                bool new_timeout = false;
                if(fd_info->read_timeout != 0 && !waiter->is_set_timeout()){
                    waiter->set_timeout(fd_info->read_timeout);
                    new_timeout = true;
                }
                waiter->set_cort_fd(fd);
                waiter->set_poll_request(EPOLLIN);
                cort_stackful_await(waiter);
                waiter->remove_cort_fd();
                if(waiter->is_timeout_or_stopped()){
                    errno = EAGAIN;
                    return -1; 
                }
                if(new_timeout){
                    waiter->clear_timeout();
                }
            }while(true);
            if(result > 0){
                waiter->wait_fd(fd);
                set_non_block(fd);
            }
            return result;
        }
        return accept(fd, addr, addrlen);
    }
    
    void cort_hooked_usleep(int usec){
        cort_stackful_fds_waiter* waiter = current_cort_stackful_fds_waiter;
        if(waiter != 0 && waiter->is_set_timeout()){
            cort_timeout_waiter::time_ms_t end_time = waiter->get_timeout_time(), now_time = cort_timer_refresh_clock();
            if(end_time > now_time){
                if(((int)(end_time - now_time))*1000 < usec){
                    waiter->stackless_resume(waiter);
                    waiter->after_stackful_resume();
                }else{
                    cort_timeout_waiter::time_ms_t sleep_time = (cort_timeout_waiter::time_ms_t)(usec/1000);
                    if(sleep_time > 0){
                        waiter->await(new cort_sleeper(sleep_time));
                        cort_stackful_await(waiter);
                    }
                }
            }
            return;
        }
        usleep(usec);
    }
    
    int cort_hooked_epoll_wait(int fd, struct epoll_event *events, int maxevents, int timeout){
        cort_stackful_fds_waiter* waiter = current_cort_stackful_fds_waiter;
        if(waiter){
            if(waiter->is_timeout_or_stopped()){
                return 0;
            }
            if(maxevents == 0){
                cort_hooked_usleep(timeout * 1000);
                return 0;
            }
            bool new_timeout = false;
            if(timeout > 0 && !waiter->is_set_timeout()){
                waiter->set_timeout(timeout);
                new_timeout = true;
            }
            waiter->set_cort_fd(fd);
            waiter->set_poll_request(EPOLLIN);
            cort_stackful_await(waiter);
            waiter->remove_cort_fd();
            if(waiter->is_timeout_or_stopped()){
                return 0;
            }
            if(new_timeout){
                waiter->clear_timeout();
            }
            // Now epoll_wait should return non-zero quickly.
            return epoll_wait(fd, events, maxevents, 0);
        }
        return epoll_wait(fd, events, maxevents, timeout);
    }
    
    static uint32_t get_epoll_event( short poll_event ){
        uint32_t result = 0;    
        if( poll_event & POLLIN )       result |= EPOLLIN;
        if( poll_event & POLLOUT )      result |= EPOLLOUT;
        if( poll_event & POLLHUP )      result |= EPOLLHUP;
        if( poll_event & POLLERR )      result |= EPOLLERR;
        if( poll_event & POLLRDNORM )   result |= EPOLLRDNORM;
        if( poll_event & POLLWRNORM )   result |= EPOLLWRNORM;
        return result;
    }
    static short get_poll_event( uint32_t epoll_event ){
        short result = 0;   
        if( epoll_event & EPOLLIN )     result |= POLLIN;
        if( epoll_event & EPOLLOUT )    result |= POLLOUT;
        if( epoll_event & EPOLLHUP )    result |= POLLHUP;
        if( epoll_event & EPOLLERR )    result |= POLLERR;
        if( epoll_event & EPOLLRDNORM ) result |= POLLRDNORM;
        if( epoll_event & EPOLLWRNORM ) result |= POLLWRNORM;
        return result;
    } 
    
    int cort_hooked_poll(struct pollfd *fds, nfds_t nfds, int timeout){     
        cort_stackful_fds_waiter* waiter = current_cort_stackful_fds_waiter;
        if(waiter){
            if(waiter->is_timeout_or_stopped()){
                return 0;
            }
            if(nfds == 1){
                 //Usually socket is writable
                if((fds->events & POLLOUT) == POLLOUT){ 
                    int result = poll(fds, nfds, 0);
                    if(result != 0){
                        return result;
                    }
                }
                bool new_timeout = false;
                if(timeout > 0 && !waiter->is_set_timeout()){
                    waiter->set_timeout(timeout);
                    new_timeout = true;
                }
                waiter->set_cort_fd(fds->fd);
                waiter->set_poll_request(get_epoll_event(fds->events));
                cort_stackful_await(waiter);
                uint32_t poll_event = waiter->get_poll_result();
                waiter->remove_cort_fd();
                if(waiter->is_timeout_or_stopped()){
                    return 0;
                }
                if(new_timeout){
                    waiter->clear_timeout();
                }
                fds->revents = get_poll_event(poll_event);
                return 1;
            }
            if(nfds == 0){
                cort_hooked_usleep(timeout * 1000);
                return 0;
            }
            
            int epfd = epoll_create(10);
            if(epfd < 0){
                return epfd;
            }
            int nnfds = nfds;
            while(nnfds-- != 0){                                        
                epoll_event ev;
                ev.events = get_epoll_event(fds[nnfds].events);
                epoll_ctl(epfd, EPOLL_CTL_ADD, fds[nnfds].fd, &ev);
            }
            bool new_timeout = false;
            if(timeout > 0 && !waiter->is_set_timeout()){
                waiter->set_timeout(timeout);
                new_timeout = true;
            }
            waiter->set_cort_fd(epfd);
            waiter->set_poll_request(EPOLLIN);
            cort_stackful_await(waiter);
            waiter->close_cort_fd();
            if(waiter->is_timeout_or_stopped()){
                return 0;
            }
            if(new_timeout){
                waiter->clear_timeout();
            }
            // Now poll should return non-zero quickly.
            return poll(fds, nfds, 0);
        }
        return poll(fds, nfds, timeout);
    }
    
    int cort_hooked_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout){
        //Usually write is available immediately
        struct timeval zero_val;
        zero_val.tv_sec = 0;
        zero_val.tv_usec = 0;
        if(writefds){
            fd_set  wrfs;
            wrfs = *writefds;
            int init_result = select(nfds, 0, &wrfs, 0, &zero_val);
            if(init_result != 0){
                if(readfds) FD_ZERO(readfds);
                if(exceptfds) FD_ZERO(exceptfds);
                *writefds = wrfs;
                return init_result;
            }
        }
        if(readfds == 0 && exceptfds == 0 && writefds == 0 && timeout != 0){
            cort_hooked_usleep(int(timeout->tv_sec * 1000 *1000 + timeout->tv_usec));
            return 0;
        }
        
        cort_stackful_fds_waiter* waiter = current_cort_stackful_fds_waiter;
        if(waiter){
            if(waiter->is_timeout_or_stopped()){
                return 0;
            }
            int epfd = epoll_create(10);
            if(epfd < 0){
                return epfd;
            }
            int nnfds = nfds;
            while(--nnfds != 0){                        
                uint32_t e1 = 0;
                if(readfds != 0 && FD_ISSET(nnfds, readfds)){
                    e1 |= EPOLLIN;              
                }
                if(writefds != 0 && FD_ISSET(nnfds, writefds)){
                    e1 |= EPOLLOUT;
                }
                if(exceptfds != 0 && FD_ISSET(nnfds, exceptfds)){
                    e1 |= EPOLLPRI;
                }
                
                if(e1 != 0){
                    epoll_event ev;
                    ev.events = e1;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, nnfds, &ev);
                }
            }
            bool new_timeout = false;
            if(timeout != 0 && !waiter->is_set_timeout()){
                cort_timeout_waiter::time_ms_t timoutms = (cort_timeout_waiter::time_ms_t)(timeout->tv_sec * 1000 + timeout->tv_usec/1000);
                if(timoutms > 0){
                    waiter->set_timeout(timoutms);
                    new_timeout = true;
                }
            }
            waiter->set_cort_fd(epfd);
            waiter->set_poll_request(EPOLLIN);
            cort_stackful_await(waiter);
            waiter->close_cort_fd();
            if(waiter->is_timeout_or_stopped()){
                return 0;
            }
            if(new_timeout){
                waiter->clear_timeout();
            }
            // Now select should return non-zero quickly.
            return select(nfds, readfds, writefds, exceptfds, &zero_val);
        }
        return select(nfds, readfds, writefds, exceptfds, timeout);
    }
    
    int cort_hooked_close(int fd){
        cort_stackful_fds_waiter* waiter = current_cort_stackful_fds_waiter;
        if(waiter){
            waiter->remove_fd(fd);
        }
        return close(fd);
    }

    int cort_hooked_setsockopt(int fd, int level, int optname, struct timeval *optval, socklen_t optlen){
        if (level == SOL_SOCKET && (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO)) {
            cort_hook_fd_info *fd_info;
            cort_stackful_fds_waiter* waiter = current_cort_stackful_fds_waiter;
            if(waiter && (fd_info = (waiter->find_fd_waiter(fd)))){ 
                uint32_t max_time = (uint32_t)(optval->tv_sec * 1000 + optval->tv_usec/10000);
                if(optname == SO_RCVTIMEO){
                    fd_info->read_timeout = max_time;
                }else{
                    fd_info->write_timeout = max_time;
                }
                return 0;
            }
        }
        return setsockopt(fd, level, optname, optval, optlen);
    }
}

