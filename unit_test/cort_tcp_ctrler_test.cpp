#ifdef CORT_TCP_CTRLER_TEST

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../network/cort_tcp_ctrler.h"

//We send to a a server http request and output the result. You can set your own.
char send_content[]= "GET /get-bin/get-cloud?amt=0&appid=100000&openid=10000000&payitem=^0^457278^^1^10000000^^3^1^^5^0^^6^cid^^4^^^10^3^^9^10000000^ HTTP/1.1\r\nHost: x.y.z.w:10574\r\nAccept: */*\r\n\r\n";
	
int main(int argc, char* argv[]){
	cort_timer_init();	
	struct local_cort : public cort_proto{
		CO_DECL(local_cort)
		cort_tcp_request_response cort_test0;
		cort_tcp_request_response cort_test1;
		int argc;
		char** argv;
		local_cort(int c, char** v) :  argc(c), argv(v){}
		cort_proto* start(){
			CO_BEGIN
				if(argc >= 3){
					cort_test0.set_dest_addr(argv[1], (unsigned short)atoi(argv[2])); 
					cort_test1.set_dest_addr(argv[1], (unsigned short)atoi(argv[2])); 
				}else{
					cort_test0.set_dest_addr("10.137.15.231", 80); 
					cort_test1.set_dest_addr("10.137.15.231", 80); 
				}				
				if(argc >= 4){
					cort_test0.set_timeout(atoi(argv[3]));
					cort_test1.set_timeout(atoi(argv[3]));
				}else{
					cort_test0.set_timeout(30);
					cort_test1.set_timeout(30);
				}
				if(argc >= 5){
					cort_test0.set_keep_alive(atoi(argv[4]));
					cort_test1.set_keep_alive(atoi(argv[4]));
				}else{
					cort_test0.set_keep_alive(10);
					cort_test1.set_keep_alive(1000);
				}
				
				cort_test0.set_send_buffer(send_content, sizeof(send_content)-1);				
				cort_test0.alloc_recv_buffer();
				cort_test1.set_send_buffer(send_content, sizeof(send_content)-1);				
				cort_test1.alloc_recv_buffer();
				CO_AWAIT_ALL(&cort_test0, &cort_test1);
				CO_SLEEP(500);	
				//If cort_test->set_keep_alive(x) x<500, then cort_test->is_timeout() may be true.
				//So we suggest you use get_errno to know whether request is successful.
				int err = cort_test0.get_errno(); 
				printf("\ncost %dms\n", (int)cort_test0.get_time_cost());
				if(err == 0){
					write(1, cort_test0.get_recv_buffer(), cort_test0.get_recv_buffer_size());
				}
				else{
					printf("error :%d\n", err);
				}
				err = cort_test1.get_errno(); 
				printf("\ncost %dms\n", (int)cort_test1.get_time_cost());
				if(err == 0){
					write(1, cort_test1.get_recv_buffer(), cort_test1.get_recv_buffer_size());
				}
				else{
					printf("error :%d\n", err);
				}
			CO_END
		}
	}test(argc, argv);
	test.start();
	cort_timer_loop();
	cort_timer_destroy();
	return 0;	
}

#endif