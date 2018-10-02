//
// Created by yael.chaimov on 5/1/18.
//

/*
 * User-Level Threads Library (uthreads)
 * Author: OS, os@cs.huji.ac.il
 */

#include "Thread.h"
#include <setjmp.h>
#include <signal.h>
#include <queue>
#include <list>
#include <cstdlib>
#include <cstdio>
#include <sys/time.h>
#include <cmath>
#include <iostream>

typedef unsigned long address_t;

#define MAX_THREAD_NUM 100 /* maximal number of threads */
#define STACK_SIZE 4096 /* stack size per thread (in bytes) */
#define JB_SP 6
#define JB_PC 7
#define SUCCESS 0
#define ERROR (-1)


std::queue<int> readyThreads;
Thread* allThreads[MAX_THREAD_NUM];
std::priority_queue<int, std::vector<int>, std::greater<int>> releasedId;
Thread* runningThread;
int nextThreadId = 0;
int totalQuantum = 0;
struct sigaction sa;
struct itimerval timer;
int number_of_threads = 0;
bool is_block_by_sigprocmask = false;
sigset_t set;

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
		"rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
		"rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#endif

/**
 * find an id for a new thread
 * @return - id
 */
int findId()
{
    if(releasedId.empty())
    {
        return nextThreadId++;
    }
    int id = releasedId.top();
    releasedId.pop();
    return id;
}

/**
 * block the signal SIGVTALRM
 */
void block_set()
{
    if(!is_block_by_sigprocmask)
    {
        sigemptyset(&set);
        sigaddset(&set, SIGVTALRM);
        is_block_by_sigprocmask = true;
        if(sigprocmask(SIG_BLOCK, &set, nullptr) < 0)
        {
            std::cerr<<"system error - block set fail";
            exit(1);
        }
    }

}

/**
 * un block the signal SIGVTALRM
 */
void un_block_set()
{
    if(is_block_by_sigprocmask)
    {
        sigemptyset(&set);
        sigaddset(&set, SIGVTALRM);
        is_block_by_sigprocmask = false;
        if(sigprocmask(SIG_UNBLOCK, &set, nullptr)<0)
        {
            std::cerr<<"system error - unblock set fail";
            exit(1);
        }
    }

}
/**
 * start the updated timer
 * @return - 0 on success, 1 on failure
 */
void startTimer()
{
    // Start a virtual timer. It counts down whenever this process is executing.
    if (setitimer (ITIMER_VIRTUAL, &timer, nullptr ) < 0)
    {
        std::cerr<<"system error - setitimer fail";
        exit(1);
    }
}

/**
 * switch between the current thread to a another thread
 * @param sig
 */
void runNextThread(int sig)
{
    int ret_val = sigsetjmp(runningThread->env,1);
    if (ret_val == 1)
    {
        return;
    }
    //if current running thread is not blocked by sync and not blocked himself
    //and the runner not terminate himself

    if((runningThread != NULL && allThreads[runningThread->getId()] != NULL) &&
       (runningThread->getState() != Thread::Block || readyThreads.empty()))
    {
        runningThread->setState(Thread::Ready);
        readyThreads.push(runningThread->getId());
    }

    int nextRunningThreadId = readyThreads.front();
    readyThreads.pop();
    runningThread = allThreads[nextRunningThreadId];
    runningThread->setState(Thread::Running);
    runningThread->addQuantums();
    totalQuantum++;
    startTimer();
    siglongjmp(runningThread->env,1);
}

/**
 * initiate the handler function to be the function: run next thread
 * and timer to be the quantum
 * @param quantum - the length of a quantum in micro-seconds
 */
void setTimer(int quantum)
{
    // Install timer_handler as the signal handler for SIGVTALRM.
    sa.sa_handler = & runNextThread;
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0) {
        std::cerr<<"system error - sigaction fail";
        exit(1);
    }
    // Configure the timer to expire after 1 sec... */
    timer.it_value.tv_sec = quantum/1000000;		// first time interval, seconds part
    timer.it_value.tv_usec = quantum%1000000;		// first time interval, microseconds part

    // configure the timer to expire every 3 sec after that.
    timer.it_interval.tv_sec = quantum/1000000;	// following time intervals, seconds part
    timer.it_interval.tv_usec = quantum%1000000;	// following time intervals, microseconds part
    startTimer();
}

/*
 * Description: This function initializes the thread library.
 * You may assume that this function is called before any other thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs)
{
    if(quantum_usecs <= 0)
    {
        return  ERROR;
    }
    int id = findId();
    Thread * mainThread = new Thread(id, nullptr);
    runningThread = mainThread;
    runningThread->setState(Thread::Running);
    allThreads[id] = mainThread;
    mainThread->addQuantums();
    totalQuantum++;
    if(sigemptyset(&mainThread->env->__saved_mask) < 0)
    {
        std::cerr<<"system error - sigemptyset fail";
        exit(1);
    }
    setTimer(quantum_usecs);
    return SUCCESS;
}

/*
 * Description: This function creates a new thread, whose entry point is the
 * function f with the signature void f(void). The thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * Return value: On success, return the ID of the created thread.
 * On failure, return -1.
*/
int uthread_spawn(void (*f)(void))
{
    block_set();
    if(number_of_threads == MAX_THREAD_NUM)
    {
        un_block_set();
        return  ERROR;
    }
    number_of_threads++;
    int newId = findId();
    Thread * newThread = new Thread(newId, f);
    newThread->setState(Thread::Ready);
    allThreads[newId] = newThread;
    readyThreads.push(newId);
    if(sigsetjmp(newThread->env, 1) == 0)
    {
        (newThread->env->__jmpbuf)[JB_SP] = translate_address(newThread->get_sp());
        (newThread->env->__jmpbuf)[JB_PC] = translate_address(newThread->get_pc());
        sigemptyset(&newThread->env->__saved_mask);
    }
    un_block_set();
    return newId;

}

/**
 * remove the thread with id tid from the ready queue
 * @param tid - the id of the thread that need to be removed from ready queue
 */
void removeTidFromReady(int tid)
{
    int threadId;
    std::queue<int> tempQueue;
    while (!readyThreads.empty())
    {
        threadId = readyThreads.front();
        if(threadId != tid)
        {
            tempQueue.push(threadId);
        }
        readyThreads.pop();
    }
    while (!tempQueue.empty())
    {
        threadId = tempQueue.front();
        readyThreads.push(threadId);
        tempQueue.pop();
    }
}
/**
 * remove all the thread that dependent in thread with id tid
 * @param tid - the thread to remove its dependency
 */
void remove_dependency(int tid)
{
    Thread* thread = allThreads[tid];
    for(unsigned int i = 0; i < (thread->dependent).size(); i++)
    {
        int id = thread->dependent.at(i);
        Thread* dependentThread = allThreads[id];
        if(dependentThread!=NULL)
        {
            dependentThread->un_block_by_sync();
            if(!dependentThread->get_is_block())
            {
                dependentThread->setState(Thread::Ready);
                readyThreads.push(id);
            }
        }

    }
}
/*
 * Description: This function terminates the thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this thread should be released. If no thread with ID tid
 * exists it is considered an error. Terminating the main thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the thread was successfully
 * terminated and -1 otherwise. If a thread terminates itself or the main
 * thread is terminated, the function does not return.
*/
int uthread_terminate(int tid)
{
    block_set();
    if(allThreads[tid] == NULL)
    {
        un_block_set();
        return ERROR;
    }
    if(tid == 0)
    {
        while(!readyThreads.empty()) readyThreads.pop();
        exit(0);
    }
    //add id to released
    releasedId.push(tid);
    remove_dependency(tid);
    number_of_threads--;
    //if ready
    if (allThreads[tid]->getState() == Thread::Ready)
    {
        removeTidFromReady(tid);
        allThreads[tid] = NULL;
        delete allThreads[tid];
    }
    //if running
    else if(allThreads[tid]->getState() == Thread::Running) {
        allThreads[tid] = NULL;
        delete allThreads[tid];
        un_block_set();
        runNextThread(0);
    }
    un_block_set();
    return SUCCESS;
}



/*
 * Description: This function blocks the thread with ID tid. The thread may
 * be resumed later using uthread_resume. If no thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision
 * should be made. Blocking a thread in BLOCKED state has no
 * effect and is not considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_block(int tid)
{
    block_set();
    if(allThreads[tid] == NULL || tid == 0)
    {
        un_block_set();
        return ERROR;
    }
    allThreads[tid]->block();
    //if ready
    if(allThreads[tid]->getState() == Thread::Ready)
    {
        allThreads[tid]->setState(Thread::Block);
        removeTidFromReady(tid);
    }
    //if running
    else if(allThreads[tid]->getState() == Thread::Running) {
        allThreads[tid]->setState(Thread::Block);
        un_block_set();
        runNextThread(0);
    }

    un_block_set();
    return SUCCESS;

}


/*
 * Description: This function resumes a blocked thread with ID tid and moves
 * it to the READY state. Resuming a thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid)
{
    block_set();
    if(allThreads[tid]== NULL)
    {
        un_block_set();
        return ERROR;
    }
    if(allThreads[tid]->getState() != Thread::Running && allThreads[tid]->getState() != Thread::Ready)
    {
        allThreads[tid]->un_block();
        if(!allThreads[tid]->get_is_block_by_sync())
        {
            allThreads[tid]->setState(Thread::Ready);
            readyThreads.push(tid);
        }
    }


    un_block_set();
    return SUCCESS;
}


/*
 * Description: This function blocks the RUNNING thread until thread with
 * ID tid will terminate. It is considered an error if no thread with ID tid
 * exists or if the main thread (tid==0) calls this function. Immediately after the
 * RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_sync(int tid)
{

    block_set();
    if(allThreads[tid] == NULL || runningThread->getId() == tid || runningThread->getId() == 0)
    {
        un_block_set();
        return ERROR;
    }
    allThreads[tid]->add_dependency(runningThread->getId());
    runningThread->block_by_sync();
    runningThread->setState(Thread::Block);
    un_block_set();
    runNextThread(0);
    return SUCCESS;
}


/*
 * Description: This function returns the thread ID of the calling thread.
 * Return value: The ID of the calling thread.
*/
int uthread_get_tid()
{
    return runningThread->getId();
}

/*
 * Description: This function returns the total number of quantums since
 * the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
int uthread_get_total_quantums()
{
    return totalQuantum;
}


/*
 * Description: This function returns the number of quantums the thread with
 * ID tid was in RUNNING state. On the first time a thread runs, the function
 * should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * thread with ID tid exists it is considered an error.
 * Return value: On success, return the number of quantums of the thread with ID tid.
 * 			     On failure, return -1.
*/
int uthread_get_quantums(int tid)
{
    block_set();
    if(tid < 0 || allThreads[tid] == NULL)
    {
        un_block_set();
        return ERROR;
    }
    un_block_set();
    return allThreads[tid]->getQuantum();
}



