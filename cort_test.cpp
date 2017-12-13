#define CO_JOIN2(x,y) x##y 
#define CO_JOIN(x,y) CO_JOIN2(x,y)

#define CO_STATE_MAX_COUNT ((count_type)((1u<<(sizeof(count_type)*8)) - 1u)) // you can increase the number if your compiler affordable

#define CO_STATE_EVAL_COUNTER(counter) (sizeof(*counter((struct_int<CO_STATE_MAX_COUNT>*)0)) \
          - sizeof(*counter((void*)0)))
//We can change the result of CO_STATE_EVAL_COUNTER if we use CO_STATE_INCREASE_COUNTER or CO_STATE_SET_COUNTER

#define CO_STATE_INCREASE_COUNTER(counter, delta)  static char (*counter(struct_int<CO_STATE_EVAL_COUNTER(counter) + 1>*))[CO_STATE_EVAL_COUNTER(counter) + sizeof(*counter((void*)0)) + (delta)] 

#define CO_STATE_SET_COUNTER(counter, value)  static char (*counter(struct_int<CO_STATE_EVAL_COUNTER(counter) + 1>*))[value + sizeof(*counter((void*)0))]

struct task_type{
	
	//Init the task.
	//For example, epoll_add or epoll_ctrl.
	//If return false, task is completed and will not to be complete checked.
	virtual bool init(void*) = 0;
	
	//Need to try best to complete the task and check whether the task has complete.
	//For example, do "recv" until "EWOULDBLOCK" or a complete http packet received.
	//If return true, task is completed and "check_complete" will not be called further.
	virtual bool check_complete(void*) = 0;
	
	virtual ~task_type(){}
};

#define DEF_CO_BEGIN(cort_type_name, args_type_name) \
struct CO_JOIN(cort_type_name, _t) { \
	typedef CO_JOIN(cort_type_name, _t) cort_type; \
	typedef args_type_name args_type; \
	typedef cort_proto interface_type; \
	typedef unsigned char count_type; \
	const static count_type nstate = (count_type)(-1); \
	template<count_type N, int M = 0> \
	struct struct_int : struct_int<N - 1, 0> {}; \
	template<int M> \
	struct struct_int<0, M> {}; \
	static char (*first_counter(...))[1]; \
	template <count_type N = (count_type)-1, int M = 0> \
	struct cort_state_struct: public args_type_name { \
		typedef task_type* (*fptr)(cort_state_struct<0>*); \
		count_type cort_next_state; \
		const static count_type cort_this_state = N; \
		inline void init(void*){this->cort_next_state = 0;} \
		template <int NN>void* dummy1(){ \

#define CO_STATE_END_TAG }};

#define CO_STATE_BEGIN(state_name) \
	const static count_type state_name = CO_STATE_EVAL_COUNTER(first_counter) ; \
	CO_STATE_INCREASE_COUNTER(first_counter, 1); \
	template <int M> \
	struct cort_state_struct<state_name, M> : \
		public cort_state_struct<state_name - 1, 0>{ \
			typedef cort_state_struct<state_name> this_type; \
			typedef cort_state_struct<state_name - 1> parent_type; \
			const static count_type cort_this_state = state_name; \
			inline void init(fptr *p){ p[state_name] = (fptr)(&this_type::do_exec_static);((parent_type*)this)->init(p);} 
			
#define CO_STATE_NAME(x) CO_STATE_END_TAG CO_STATE_BEGIN(x)
			
#define CO_STATE CO_STATE_NAME(CO_JOIN(CO_INCREASE, __LINE__))

#define CO_ACTION \
	static task_type* do_exec_static(this_type* this_ptr){return this_ptr->do_exec();} \
	inline task_type* do_exec(){  
	
struct cort_proto{
	virtual task_type* run() = 0;
	virtual ~cort_proto(){}
};

struct is_cort{
	static char test(...);
    static long test(cort_proto*);
};
#define IS_CORT_TEST(x) (sizeof(is_cort::test(x)) == sizeof(long))
	
template<typename T, bool is_cort_result = IS_CORT_TEST((T*)0)>
struct is_cort_tester{
	static inline task_type* get_task(T* arg){
		return arg;
	}
};

template<typename T>
struct is_cort_tester<T, true>{
	static inline task_type* get_task(T* arg){
		return arg->run();
	}
};

template<typename T>
inline task_type* get_task(T* arg){
	return is_cort_tester<T>::get_task(arg);
}

#define CO_AWAIT_TASK(x) \
	cort_next_state  = cort_this_state + 1; \
	return CO_JOIN(x); \
	CO_STATE \
	CO_ACTION \
	
#define CO_AWAIT_STATE(x) \
	task_type* CO_JOIN(COARG, __LINE__) = get_task(x); \
	if(!IS_CORT_TEST(x) && (CO_JOIN(COARG, __LINE__) == 0)){ \
		return ((cort_state_struct<cort_this_state + 2>*)(this))->do_exec(); \
	} \
	cort_next_state  = cort_this_state + (IS_CORT_TEST(x) ? 1:2); \
	return CO_JOIN(COARG, __LINE__); /*When resumed, goto next "clean" state for "generator mode" waiting. */ \
	CO_STATE_NAME(CO_JOIN(anonymous, __LINE__)) \
	CO_ACTION \
	task_type* CO_JOIN(COARG, __LINE__) = get_task(x); \
	if(IS_CORT_TEST(x) && (CO_JOIN(COARG, __LINE__) == 0)){ \
		return ((cort_state_struct<cort_this_state + 1>*)(this))->do_exec(); \
	} \
	return CO_JOIN(COARG, __LINE__); /*Break to next state until CO_JOIN(COARG, __LINE__)==0.*/\
	CO_STATE \


#define CO_AWAIT(x) \
	CO_AWAIT_STATE(x) \
	CO_ACTION 

#define CO_RETURN(x) \
	task_type* CO_JOIN(COARG, __LINE__) = get_task(x); \
	if(!IS_CORT_TEST(x) && CO_JOIN(COARG, __LINE__) == 0){ \
		return 0; \
	} \
	cort_next_state  = cort_this_state + 1; \
	return x; /*When resumed, goto next "clean" state for "generator mode" waiting. */ \
	CO_STATE_NAME(CO_JOIN(anonymous, __LINE__)) \
	CO_ACTION \
	if(sizeof(task_type) == sizeof(*x) ){ return 0;}\
	task_type* CO_JOIN(COARG, __LINE__) = get_task(x); \
	if(IS_CORT_TEST(x) && (CO_JOIN(COARG, __LINE__) != 0)){ \
		return CO_JOIN(COARG, __LINE__); /*Break until CO_JOIN(COARG, __LINE__)==0.*/\
	} \


#define CO_BREAK return 0;

#define DEF_CO_END(cort_type_name) return 0; CO_STATE_END_TAG \
	const static count_type state_total_count = CO_STATE_EVAL_COUNTER(first_counter) ; \
	template <int M> \
	struct cort_state_struct<state_total_count, M> : \
		public interface_type, public cort_state_struct<state_total_count - 1, 0> { \
			typedef cort_state_struct<state_total_count> this_type; \
			typedef cort_state_struct<state_total_count - 1, 0> parent_type; \
			fptr action_function_pointer[state_total_count]; \
			cort_state_struct(){cort_state_struct<state_total_count - 1>::init(action_function_pointer);} \
			/*return zero means cort finished!*/	\
			virtual task_type* run(){\
				return action_function_pointer[cort_next_state](this); \
			}; \
		private: \
			cort_state_struct(const cort_state_struct&); \
	};\
	typedef cort_state_struct<state_total_count, 0 > final_type; \
}; typedef CO_JOIN(cort_type_name, _t)::final_type cort_type_name;

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>



struct task_type_example: public task_type{
	int start;
	std::string result;
	int step;
	task_type_example():start(0),result(" "), step(1){}
	
	virtual bool init(void* arg){
		int *poller = (int*)arg;
		*poller = start;
		return true;
	}
	virtual bool check_complete(void* arg){
		int *poller = (int*)arg;
		if(*poller < 100){
			++(*poller);
			return false;
		}
		if(*poller >= 300 || result.size() >= 1000){
			return true;
		}
		++step;
		*poller += step;

		std::string::size_type index = strlen(result.c_str());
		result.resize(strlen(result.c_str())+3);
		#ifdef _MSC_VER
			_snprintf((char*)(result.c_str()+index), result.size()-index, "%d", *poller );
		#else
			snprintf((char*)(result.c_str()+index), result.size()-index, "%d", *poller );
		#endif
		if(result.empty() || *(result.rbegin()) != '9'){
			return false;
		}
		return true;
	}
};

struct args_type_example{
	int start;
	int step;
};

DEF_CO_BEGIN(first_cort, args_type_example)
	CO_STATE
		task_type_example first_task;
		task_type_example second_task;
	CO_ACTION
		first_task.start = start;
		first_task.step = step;
		CO_AWAIT(&first_task)
		puts(first_task.result.c_str());
		
		second_task.start = start;
		second_task.step = step+1;
		CO_AWAIT(&second_task)
		puts(second_task.result.c_str());
		puts(first_task.result.c_str());	
DEF_CO_END(first_cort)


DEF_CO_BEGIN(second_cort, args_type_example)
	CO_STATE
		task_type_example first_task;
		first_cort sub_cort;
	CO_ACTION
		first_task.start = start;
		first_task.step = step;
		CO_AWAIT(&first_task)
		puts(first_task.result.c_str());
		sub_cort.start = start;
		sub_cort.step = step;
		CO_AWAIT(&sub_cort)
		puts(sub_cort.first_task.result.c_str());
DEF_CO_END(second_cort)

#ifdef LINUX
#include <unistd.h>
#endif
#ifdef WINDOWS
#include <windows.h>
#endif

void mySleep(int sleepMs)
{
#ifdef LINUX
    usleep(sleepMs*1000 );   
#endif
#ifdef WINDOWS
    Sleep(sleepMs);
#endif
};   

int main(){
	int poller = 2;
	
	cort_proto* p = new first_cort();
	((first_cort*)p)->start = 0;
	((first_cort*)p)->step = 1;
	int task_finished = 0;
	for(task_type* the_task = (task_type*)(p->run()); the_task != 0; the_task = (task_type*)(p->run())){
		if(!the_task->init(&poller)){
			continue;
		}
		while(!the_task->check_complete(&poller)){
			mySleep(1);
		}
		printf("task_finished:%d\n", ++task_finished);
	}
	delete p;
	
	p = new second_cort();
	((second_cort*)p)->start = 2;
	((second_cort*)p)->step = 3;
	for(task_type* the_task = (task_type*)(p->run()); the_task != 0; the_task = (task_type*)(p->run())){
		if(!the_task->init(&poller)){
			continue;
		}
		while(!the_task->check_complete(&poller)){
			mySleep(1);
		}
		printf("task_finished:%d\n", ++task_finished);
	}
	
	return 0;
}