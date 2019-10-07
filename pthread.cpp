// User Mode Thread Library Project -- WITH thread synchronization
// By Edward Yuen | edwardyuensf@gmail.com
// Finished April 2019

// Goal was to add functionality to my basic user mode thread library by 
// adding restrictions to ensure threads can safely co-exist
// It was to also to implement important synchronization primitives including
// semaphores and locks


#include <pthread.h>
#include <setjmp.h>
#include <iostream>
#include <vector>
#include <string>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <semaphore.h>

using namespace std;

// Global Variables
static pthread_t nextAvailableID = 0;
static int nextSemID = 0;
static int currentThread = 0;
static int numcalls = 0;
static bool pthread_init = false;
const string RUNNABLE = "RUNNABLE";
const string EXITED = "EXITED";
const string BLOCKED = "BLOCKED";
const int SEM_VALUE_MAX = 65536;

sigset_t x;
static bool lock_init = false;

static long int i64_ptr_mangle(long int p)
{
	long int ret;
	asm(" mov %1, %%rax;\n"
		" xor %%fs:0x30, %%rax;"
		" rol $0x11, %%rax;"
		" mov %%rax, %0;"
		: "=r"(ret)
		: "r"(p)
		: "%rax"
		);
	return ret;
}

void lock(){
	if(!lock_init){
		sigemptyset(&x);
		sigaddset(&x, SIGALRM);
		sigprocmask(SIG_BLOCK, &x, NULL);
	}else{
		sigprocmask(SIG_BLOCK, &x, NULL);
	}
}

void unlock(){
	sigprocmask(SIG_UNBLOCK, &x, NULL);
}

struct TCB {

	pthread_t thread_ID;
	jmp_buf context;
	string status;
	void *(*start_routine)(void *);
	void *arg;
	char *stack;
	TCB *joiningThread;
	void *exitValue;

};

struct mySem_t {

	int semaphore_ID;
	unsigned int currentValue;
	vector<TCB*> blockedThreads;

};

static vector<TCB*> table;
static vector<mySem_t*> semaphoreTable;

void findNextThread(){
	do {
		currentThread--;
		if (currentThread < 0){
			currentThread = table.size() - 1;
		}
	} while (table.at(currentThread)->status == EXITED || table.at(currentThread)->status == BLOCKED);
}

void scheduler(int signal){
	if (setjmp((table.at(currentThread))->context) == 0) {
		lock();
		do {
			currentThread--;
			if (currentThread < 0){
				currentThread = table.size()-1;
			}
		} while (table.at(currentThread)->status == EXITED || table.at(currentThread)->status == BLOCKED);
		unlock();

		longjmp((table.at(currentThread))->context, 1);
	} else {
		return;
	}

}

void timer(){
	// sets up the sigaction struct, points the handler to our scheduler
	struct sigaction sigact;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_handler = scheduler;
	sigact.sa_flags = SA_NODEFER;
	sigaction(SIGALRM, &sigact, NULL);

	// sets up t he ualarm timer, sends SIGALRM every 50 ms
	ualarm(50000,50000);
}

void wrapper(){
	// grabs the current thread's start_routine and arguments
	void *(*start_routine)(void *) = (table.at(currentThread)->start_routine);
	void *arg = (table.at(currentThread))->arg;

	// executes the function passed into pthread_create and then exits
	void* returnValue = start_routine(arg);
	pthread_exit(returnValue);
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg){
	if(!pthread_init){

		pthread_init = true;
		TCB* mainTCB = new TCB();
		mainTCB->thread_ID = nextAvailableID;
		mainTCB->status = RUNNABLE;
		mainTCB->start_routine = start_routine;
		mainTCB->arg = arg;
		mainTCB->stack = NULL;
		mainTCB->joiningThread = NULL;
		nextAvailableID++;
	  //setjmp(mainTCB->context);
		setjmp(mainTCB->context);
		table.push_back(mainTCB);
		timer();

	}
	lock();
	// Assign the next available thread ID to *thread, and create a new TCB with that new threadID/RUNNING/start_routine/arg
	*thread = nextAvailableID;
	void* bottomOfStack = (void*)malloc(32767);
	void* topOfStack = (char*)(bottomOfStack)+32767-21;

	TCB* tempTCB = new TCB();
	
	tempTCB->thread_ID = nextAvailableID;
	tempTCB->status = RUNNABLE;
	tempTCB->start_routine = start_routine;
	tempTCB->arg = arg;
	tempTCB->stack = (char*)bottomOfStack;
	tempTCB->joiningThread = NULL;
	//table.insert(table.begin()+2, tempTCB);

	setjmp(tempTCB->context);
	*((unsigned long *)&(tempTCB->context)+6) = i64_ptr_mangle((long int)((long int*)topOfStack));
	*((unsigned long *)&(tempTCB->context)+7) = i64_ptr_mangle((long int)&wrapper);
	table.push_back(tempTCB);

	// Increment the nextAvailableThreadID
	nextAvailableID++;
	//table.push_back(tempTCB);
	unlock();
	return 0;


}

int pthread_join(pthread_t thread, void **value_ptr){
	lock();
	int waitingThreadIndex = -1;
	if (thread == currentThread){
		return EDEADLK;
	}

	// table.at(currentThread)->status = BLOCKED;
	// find index of thread to wait for
	for (int i = 0; i < table.size(); i++){
		if (table.at(i)->thread_ID == thread){
			waitingThreadIndex = i;
			break;
		}
	}

	if (waitingThreadIndex == -1)
		return ESRCH;

	TCB *waitingThread = table.at(waitingThreadIndex);
	// if thread not terminated, set joining thread, and block waiting thread
	if (waitingThread->joiningThread != NULL){
		return EINVAL;
	}
	unlock();

	if (waitingThread->status != EXITED){
		waitingThread->joiningThread = table.at(currentThread);
		table.at(currentThread)->status = BLOCKED;
		scheduler(0);
	}

	lock();
	if (value_ptr != NULL){
		*value_ptr = waitingThread->exitValue;
	}

	for (int i = 0; i < table.size(); i++){
		if (table.at(i)->thread_ID == thread){
			waitingThreadIndex = i;
			break;
		}
	}

	free(table.at(waitingThreadIndex)->stack);
	delete(table.at(waitingThreadIndex));
	table.erase(table.begin()+waitingThreadIndex);
	unlock();

	return 0;

}

void pthread_exit(void *value_ptr){
	
	lock();
	table.at(currentThread)->status = EXITED;
	table.at(currentThread)->exitValue = value_ptr;
	if (table.at(currentThread)->joiningThread != NULL){
		TCB *threadToWake = table.at(currentThread)->joiningThread;
		threadToWake->status = RUNNABLE;
		threadToWake->exitValue = value_ptr;
	}

  // longjmp to the next available thread

	do {
		currentThread--;
		if (currentThread < 0)
			currentThread = table.size()-1;
	} while (table.at(currentThread)->status != RUNNABLE);
	unlock();

	longjmp((table.at(currentThread))->context, 1);

}

pthread_t pthread_self(void){
  // return the "current" thread's ID
	return (pthread_t)(table.at(currentThread))->thread_ID;
}

int sem_init(sem_t *sem, int pshared, unsigned int value){
	lock();
	if (sem == NULL){
		unlock();
		return EINVAL;
	}

	if (value > SEM_VALUE_MAX){
		unlock();
		return EINVAL;

	}

	if (pshared != 0){
		unlock();
		return ENOSYS;
	}

	mySem_t *newSem = new mySem_t();
	newSem->currentValue = value;
	newSem->semaphore_ID = nextSemID++;
	sem->__align = (long int)newSem;

	semaphoreTable.push_back(newSem);
	unlock();
	return 0;
}

int sem_destroy(sem_t *sem){
	lock();
	if (sem == NULL){
		unlock();
		return EINVAL;
	}

	if (semaphoreTable.empty()){
		unlock();
		return EINVAL;
	}

	//int destroyedSemID = (sem->__align)->semaphore_ID;
	mySem_t *tempSem = (mySem_t*)sem->__align;
	int destroyedSemID = tempSem->semaphore_ID;

	int indexOfSemaphore = -1;
	for (int i = 0; i < semaphoreTable.size(); i++){
		if (semaphoreTable.at(i)->semaphore_ID == destroyedSemID){
			indexOfSemaphore = i;
			break;

		}
	}

	if (indexOfSemaphore == -1){
		return EINVAL;
	}

	semaphoreTable.erase(semaphoreTable.begin()+indexOfSemaphore);
	unlock();
	return 0;
}

int sem_wait(sem_t *sem){
	if (sem == NULL){
		return EINVAL;
	}

	mySem_t *tempSem = (mySem_t*)sem->__align;
	if (tempSem->currentValue == 0){
		table.at(currentThread)->status = BLOCKED;
		tempSem->blockedThreads.push_back(table.at(currentThread));
		scheduler(0);
	}
	lock();
	tempSem->currentValue = tempSem->currentValue - 1;
	unlock();


	return 0;
}

int sem_post(sem_t *sem){
	lock();

	if (sem == NULL){
		unlock();
		return EINVAL;
	}


	mySem_t *tempSem = (mySem_t*)sem->__align;
	if (tempSem->currentValue == 65536){
		unlock();
		return EOVERFLOW;
	}

	tempSem->currentValue = tempSem->currentValue + 1;
	if (tempSem->currentValue > 0 && !(tempSem->blockedThreads.empty())){
		tempSem->blockedThreads.at(tempSem->blockedThreads.size()-1)->status = RUNNABLE;
		tempSem->blockedThreads.pop_back();
	}

	unlock();
	return 0;
}
