//
// Created by eilam on 24/04/2025.
//
#ifndef _UTHREADS_CPP
#define _UTHREADS_CPP

#include <algorithm>
#include <iostream>
#include <signal.h>
#include <unordered_map>
#include <vector>
#include <sys/time.h>
#include <queue>
#include <setjmp.h>
#include "uthreads.h"

//macros:
// #define MAX_THREAD_NUM 100 /* maximal number of threads */
// #define STACK_SIZE 4096 /* stack size per thread (in bytes) */
#define MAIN_THREAD_ID 0
#define JB_SP 6
#define JB_PC 7

//struct thread
// typedef void (*thread_entry_point)(void);
typedef enum {RUNNING,READY,BLOCKED} ThreadState; /* thread state (RUNNING, READY, BLOCKED) */


typedef struct
{
	int tid; /* thread id */
	ThreadState state; /* thread state (RUNNING, READY, BLOCKED) */
	char * stack; /* pointer to the thread's stack */
	sigjmp_buf context; /* context of the thread */
	int quantum_run; /* number of times quantums was in RUNNING state */
	int sleep_remaining; /* number of quantums left to sleep */
} thread;

//typedefs:
typedef unsigned long address_t;

// globals:
std::unordered_map<int, thread> _threads;
std::priority_queue<int, std::vector<int>, std::greater<int>> _minHeap;
std::deque <int> _readyQueue;
char thread_stacks[MAX_THREAD_NUM][STACK_SIZE]; //todo check if this is ok
int _current_thread = -1;
int _total_quantums = 0;

//functions:
void _disable_timer_signals()
{
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGVTALRM);
	if(sigprocmask(SIG_BLOCK, &set, nullptr) < 0)
	{
		std::cerr << "thread library error: failed to block signals in _disable_timer_signals" << std::endl;
	}
}

void _enable_timer_signals()
{
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGVTALRM);
	if (sigprocmask(SIG_UNBLOCK, &set, nullptr))
	{
		std::cerr << "thread library error: failed to unblock signals in _enable_timer_signals" << std::endl;
	}
}




void _switch_thread()
{
	_disable_timer_signals();


	if (_threads.find(_current_thread) != _threads.end() && _threads[_current_thread].state != BLOCKED)
	{
		_threads[_current_thread].state = READY;
		if (_threads[_current_thread].sleep_remaining == 0){
			_readyQueue.push_back(_current_thread);
		}
	}


	if (!(_readyQueue.empty()))
	{
		_current_thread = _readyQueue.front();
		_readyQueue.pop_front();
		_threads[_current_thread].state = RUNNING;
		_threads[_current_thread].quantum_run++;
		_enable_timer_signals();


		siglongjmp(_threads[_current_thread].context, 1);
	}
	_enable_timer_signals();
}

void _timer_handler(int sig)
{
	_disable_timer_signals();
	for (int i = 0; i < MAX_THREAD_NUM; ++i)
	{
		if (_threads.find(i) != _threads.end() && _threads[i].sleep_remaining > 0)
		{
			_threads[i].sleep_remaining--;

			if (_threads[i].sleep_remaining == 0 && _threads[i].state != BLOCKED) {
				// Wake up thread
				_threads[i].state = READY;
				_readyQueue.push_back(i);
			}
		}
	}
	_total_quantums++;
	int ret_val = sigsetjmp(_threads[_current_thread].context, 1);
	bool did_just_save_bookmark = ret_val == 0;
	// bool did_jump_from_another_thread = ret_val != 0;
	if (did_just_save_bookmark)
	{
		_enable_timer_signals();
		_switch_thread();
	}
	_enable_timer_signals();
}

address_t _translate_address(address_t addr)
{
	address_t ret;
	asm volatile("xor    %%fs:0x30,%0\n"
		"rol    $0x11,%0\n"
				 : "=g" (ret)
				 : "0" (addr));
	return ret;
}


void _setup_thread(int tid,ThreadState state ,char *stack, unsigned int quantum_run, thread_entry_point entry_point)
{
	// initializes env[tid] to use the right stack, and to run from the function 'entry_point', when we'll use
	// siglongjmp to jump into the thread.
	address_t sp = (address_t) stack + STACK_SIZE - sizeof(address_t);
	address_t pc = (address_t) entry_point;
	_threads[tid].tid = tid;
	_threads[tid].state = state;
	_threads[tid].stack = stack; // todo check what needs to be here
	_threads[tid].quantum_run = 0;
	sigsetjmp(_threads[tid].context, 1);
	(_threads[tid].context->__jmpbuf)[JB_SP] = _translate_address(sp); /* tid *2 because every thread useses 2 pointer of jumpbuff so i keep them by order*/
	(_threads[tid].context->__jmpbuf)[JB_PC] = _translate_address(pc); /* todo if unexpected behavior occurs, check if the stack is not corrupted */
	sigemptyset(&_threads[tid].context->__saved_mask); /* tid here represent the thread id */
}



int uthread_init(int quantum_usecs)
{
	if (quantum_usecs <= 0)
	{
		fprintf(stderr, "thread library error: invalid quantum_usecs for init function\n");
		return -1;
	}
	_current_thread = MAIN_THREAD_ID;
	//declaration of main thread
	_threads[MAIN_THREAD_ID].tid = MAIN_THREAD_ID ;
	_threads[MAIN_THREAD_ID].state = RUNNING;
	_threads[MAIN_THREAD_ID].stack = nullptr;
	// _threads[MAIN_THREAD_ID].stack = thread_stacks[MAIN_THREAD_ID]; //todo check if this is ok
	_threads[MAIN_THREAD_ID].quantum_run = 1;
	sigsetjmp(_threads[MAIN_THREAD_ID].context, 1);
	_threads[MAIN_THREAD_ID].context->__jmpbuf[JB_SP] = 0; //todo maybe needs to be 0 and 1 insted of JB_SP AND JB_PC
	_threads[MAIN_THREAD_ID].context->__jmpbuf[JB_PC] = 0;
	sigemptyset(&_threads[MAIN_THREAD_ID].context->__saved_mask);

	//set the timer
	struct sigaction sa ={0};
	sa.sa_handler = &_timer_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGVTALRM, &sa, nullptr) < 0)
	{
		std::cerr << "thread library error: sigaction error in uthread_init function." << std::endl;
		return -1;
	}

	struct itimerval timer;
	timer.it_value.tv_sec = 0;        // first time interval, seconds part
	timer.it_value.tv_usec = quantum_usecs;        // first time interval, microseconds part
	timer.it_interval.tv_sec = 0;    // following time intervals, seconds part
	timer.it_interval.tv_usec = quantum_usecs;    // following time intervals, microseconds part
	if (setitimer(ITIMER_VIRTUAL, &timer, nullptr) < 0)
	{
		std::cerr << "thread library error: setitimer error in uthread_init function " << std::endl;
		return -1;
	}
	_total_quantums++;
	//min heap initialize
	for (int i = 1; i <= 99; ++i)
	{
		_minHeap.push(i);
	}
	return 0;
}




/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point)
{
	if (entry_point == nullptr || _threads.size() >= MAX_THREAD_NUM)
	{
		std::cerr << "thread library error: invalid entry_point for spawn function or thread is full" << std::endl;
		return -1;
	}
	_disable_timer_signals();
	int new_tid = _minHeap.top();
	_minHeap.pop();
	char* stack = thread_stacks[new_tid]; //todo check if this is ok
	_setup_thread(new_tid, READY, stack, 0, entry_point);
	_readyQueue.push_back(new_tid);

	_enable_timer_signals();
	return new_tid;
}



/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid)
{
	if (_threads.find(tid) == _threads.end())
	{
		std::cerr << "thread library error: invalid tid for terminate function " << std::endl;
		return -1;
	}
	_disable_timer_signals();

	if (tid == MAIN_THREAD_ID)
	{
		// for (auto& thread : _threads)
		// {
		// 	delete[] thread.second.stack;
		// }
		exit(0);
	}

	_threads.erase(tid);
	_minHeap.push(tid);
	auto to_remove = std::find(_readyQueue.begin(), _readyQueue.end(), tid);
	if (to_remove != _readyQueue.end())
	{
		_readyQueue.erase(to_remove);
	}
	_total_quantums++;
	if (_current_thread == tid)
	{
		_enable_timer_signals();
		_switch_thread();
		//todo if everything is collapsing return something
	}
	_enable_timer_signals();
	return 0;
}



/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid)
{
	if (_threads.find(tid) == _threads.end())
	{
		std::cerr << "thread library error: invalid tid for Blocked function " << std::endl;
		return -1;
	}
	if (tid == MAIN_THREAD_ID)
	{
		std::cerr << "thread library error: cannot block the main thread" << std::endl;
		return -1;
	}
	_disable_timer_signals();
	if(_current_thread == tid)
	{
		_threads[tid].state = BLOCKED; //todo new change
		int ret_val = sigsetjmp(_threads[_current_thread].context, 1);
		bool did_just_save_bookmark = ret_val == 0;
		// bool did_jump_from_another_thread = ret_val != 0;
		if (did_just_save_bookmark)
		{
			_enable_timer_signals();
			_switch_thread();
		}
		_enable_timer_signals();
	}
	else
	{
		auto to_remove = std::find(_readyQueue.begin(), _readyQueue.end(), tid);
		if (to_remove != _readyQueue.end())
		{
			_readyQueue.erase(to_remove);
		}
	}
	_threads[tid].state = BLOCKED;
	_enable_timer_signals();
	return 0;
}


/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid)
{
	if (_threads.find(tid) == _threads.end()) {
		std::cerr << "thread library error: invalid thread id for resume function" << std::endl;
		return -1;
	}
	_disable_timer_signals();
	if (_threads[tid].state == BLOCKED)
	{
		_threads[tid].state = READY;
		if (_threads[tid].sleep_remaining == 0)
		{
			_readyQueue.push_back(tid);
		}
	}
	_enable_timer_signals();
	return 0;
}


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums)
{
	if (_current_thread == 0) {
		std::cerr << "thread library error: cannot sleep the main thread" << std::endl;
		return -1;
	}

	if (num_quantums <= 0) {
		std::cerr << "thread library error: invalid num_quantums for sleep function" << std::endl;
		return -1;
	}

	_disable_timer_signals(); // block SIGVTALRM to prevent race conditions


	_threads[_current_thread].sleep_remaining = num_quantums;


	int ret_val = sigsetjmp(_threads[_current_thread].context, 1);
	bool did_just_save_bookmark = ret_val == 0;
	// bool did_jump_from_another_thread = ret_val != 0;
	if (did_just_save_bookmark)
	{
		_enable_timer_signals();
		_switch_thread();
	}
	_enable_timer_signals(); // re-enable SIGVTALRM
	// _switch_thread(); // perform context switch to next READY thread

	return 0;
}


/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid()
{
	return _current_thread;
}


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums()
{

	return _total_quantums;
}


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid)
{
	if (_threads.find(tid) == _threads.end()) {
		std::cerr << "thread library error: invalid thread id for get_quantums function" << std::endl;
		return -1;
	}
	return _threads[tid].quantum_run;
}


#endif // _UTHREADS_CPP
