#//define _XOPEN_SOURCE 600
// Jose's Thread Library!

#include "interrupt.h"
#include "thread.h"
#include <ucontext.h>
#include <queue>
#include <set>
#include <map>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <sstream>
#include <assert.h>
using namespace std;

// classes
class thread {
    public:
        ucontext_t* currContext;
        unsigned int lockWait;
        bool blockedCV;
        bool blockedLock;
        unsigned int CVWait;
        set<unsigned int> heldLocks;
        unsigned int CVLockWait;
};

// global variables
static map<unsigned int, deque<thread*> > CVQueue;
static queue<thread*> readyQueue;
static queue<thread*> blockedQueue;
static queue<thread*> inactiveQueue;
static thread* runningThread;
static set<unsigned int> locksHeld;
static ucontext_t* headPtr;
static bool initCalled = false;
static bool isFirstThread = true;


int checkIfExitable(){
  if (!initCalled) {
    interrupt_enable();
    return -1;
  }
	if(readyQueue.size() == 0) {
        cout << "Thread library exiting.\n";
        interrupt_enable();
        exit(0);
    }
    return 0;
}

void thread_create_helper_method(thread_startfunc_t func, void *arg) {
    interrupt_enable();
    func(arg);
    interrupt_disable();
    swapcontext(runningThread->currContext, headPtr);
}

int thread_lib_helper(thread_startfunc_t func, void *arg) {
    if(!initCalled) {
        interrupt_enable();
        return -1;
    }
    char* stack;
    thread * newThread;
    try {
      newThread = new thread;
      try {
        stack = new char[STACK_SIZE];
        newThread->currContext = new ucontext_t;
        getcontext(newThread->currContext);
      }
      catch(bad_alloc e) {
          interrupt_enable();
          return -1;
      }
      newThread->currContext->uc_stack.ss_sp = stack;
      newThread->currContext->uc_stack.ss_size = STACK_SIZE;
      newThread->currContext->uc_stack.ss_flags = 0;
      newThread->currContext->uc_link = NULL;
      newThread->blockedLock = false;
      newThread->lockWait = 0;
      newThread->blockedCV = false;
      newThread->CVLockWait = 0;
      newThread->CVWait = 0;
      makecontext(newThread->currContext, (void(*)())&thread_create_helper_method, 2, func, arg);
      readyQueue.push(newThread);
  }
  catch (std::bad_alloc b) {
    //cout << "bad alloc caught while making thread\n";
    delete newThread->currContext;
    delete newThread;
    interrupt_enable();
    return -1;
  }

}

extern int thread_libinit(thread_startfunc_t func, void *arg){
  interrupt_disable();
  if (!initCalled) {
    interrupt_enable();
    return -1;
  }
  initCalled = true;

  try {
      headPtr = new ucontext_t;
  } catch(bad_alloc e) {
      interrupt_enable();
      return -1;
  }

  //Obtain context of headptr
  getcontext(headPtr);

  try {
      char* main_stack = new char[STACK_SIZE];
      headPtr->uc_stack.ss_sp = main_stack;
      headPtr->uc_stack.ss_size = STACK_SIZE;
      headPtr->uc_stack.ss_flags = 0;
      headPtr->uc_link = NULL;
  } catch(bad_alloc e) {
      interrupt_enable();
      return -1;
  }

  thread_lib_helper(func, arg);



  while(readyQueue.size() != 0) {
      if(!isFirstThread) {
        thread* prev = runningThread;
        delete (char*) (prev->currContext->uc_stack.ss_sp);
        delete prev->currContext;
        delete prev;
      }
      runningThread = readyQueue.front();
      readyQueue.pop();
      isFirstThread = false; // FIRST THREAD IS EVER TRUE ONCE
      swapcontext(headPtr, runningThread->currContext);
  }
  delete (char*) (headPtr->uc_stack.ss_sp);
  delete headPtr;
  cout << "Thread library exiting.\n";
  interrupt_enable();
  exit(0);

}

extern int thread_create(thread_startfunc_t func, void *arg){
	interrupt_disable();
    if(!initCalled) {
        interrupt_enable();
        return -1;
    }
    char* stack;
    thread * newThread;
    try {
    	newThread = new thread;
    	try {
        stack = new char[STACK_SIZE];
        newThread->currContext = new ucontext_t;
        getcontext(newThread->currContext);
    	}
    	catch(bad_alloc e) {
    	    interrupt_enable();
    	    return -1;
    	}
    	newThread->currContext->uc_stack.ss_sp = stack;
    	newThread->currContext->uc_stack.ss_size = STACK_SIZE;
    	newThread->currContext->uc_stack.ss_flags = 0;
    	newThread->currContext->uc_link = NULL;
    	newThread->blockedLock = false;
    	newThread->lockWait = 0;
    	newThread->blockedCV = false;
    	newThread->CVLockWait = 0;
    	newThread->CVWait = 0;
    	makecontext(newThread->currContext, (void(*)())&thread_create_helper_method, 2, func, arg);
    	readyQueue.push(newThread);
    	interrupt_enable();
    	return 0;
  }
  catch (std::bad_alloc b) {
    //cout << "bad alloc caught while making thread\n";
    delete newThread->currContext;
    delete newThread;
    interrupt_enable();
    return -1;
  }

}

//TODO: WORKING?
extern int thread_yield() {
    interrupt_disable();
    if(!initCalled) {
        interrupt_enable();
        return -1;
    }

    // If there is at least one thread in the readyQueue, pop the front one
    // and switch it with the current thread
    else if(readyQueue.size() != 0) {
        // pop off first thing from readyQueue
        // push curr_thread to readyQueue
        thread* nextThread = readyQueue.front();
        readyQueue.pop();
        thread* prevThread = runningThread;
        runningThread = nextThread;
        readyQueue.push(prevThread);
        swapcontext(prevThread->currContext, nextThread->currContext);
    }
    interrupt_enable();
    return 0;
}



/**
// Implementation of the thread_lock util.
// We FIRST disable interrupts, and check if initCalled is true. We then entire a while loop that checks if
// any locks are currently being held, and if not, we give a lock to the current thread inside of the function
// and break out of the while loop.
// Otherwise, we put put the current on status of waiting for the parameter lock, and switch contexts
*/
extern int thread_lock(unsigned int lock){
	interrupt_disable();
    int returnVal = 0;
    //Begin the lock loop
    while(true){
    	//TODO: WHAT IS INITCALLED?
    	if(!initCalled) {
            return -1;
        }
    	if(locksHeld.count(lock) == 0) {
            locksHeld.insert(lock);
            runningThread->heldLocks.insert(lock);
            break;
        }
        //Else, there is at least 1 lock currently being held
        else if (checkIfExitable() == 0){
        	if(runningThread->heldLocks.count(lock) == 1) {
                returnVal = -1;
                break;
            }
            thread* next = readyQueue.front();
            readyQueue.pop();

            runningThread->blockedLock = true;
            runningThread->lockWait = lock;

            thread* prev = runningThread;
            blockedQueue.push(prev);
            runningThread = next;

            // Last step is to swap contexts (aka swap threads)
            swapcontext(prev->currContext, next->currContext);
        }
    }
    interrupt_enable();
    return returnVal;

}

/**
// Implementation of the thread_unlock method
*/
extern int thread_unlock(unsigned int lock){
	interrupt_disable();
	int val = 0;
  	if (!initCalled) {
  	  interrupt_enable();
  	  return -1;
  	}

  	if (runningThread->heldLocks.count(lock) == 0){
  		// ERROR
  		interrupt_enable();
  		return -1;
  	}
  	else { // Else if there is a lock currently being held
        // remove from set
        // loop blocked queue
        // add to ready queue if not blocked anymore
        if (locksHeld.count(lock) == 0){
    		// There are no threads that currently hold this lock!
    		interrupt_enable();
    		return -1;
  		}
  		// Remove lock from both sets
  		locksHeld.erase(lock);
  		runningThread->heldLocks.erase(lock);

        queue<thread*> buffer;
        while(blockedQueue.size() > 0) {
            thread* first = blockedQueue.front();
            if(first->blockedLock == true){
            	if(first->lockWait == lock){
            		first->blockedLock = false;
                	first->lockWait = 0;
                	//Add to ready queue
                	readyQueue.push(first);
            	}
            } else {
            	// Add to our buffer
                buffer.push(first);
            }
            //Pop the last entry
            blockedQueue.pop();
        }
        while(buffer.size() > 0) {
        	// Add to blocked queue from buffer
            blockedQueue.push(buffer.front());
            buffer.pop();
        }
    }
    interrupt_enable();
    return val;

}

//TODO: Fix this lmao
extern int thread_wait(unsigned int lock, unsigned int cond){
    interrupt_disable();
    if(!initCalled) {
        interrupt_enable();
        return -1;
    }
    if(locksHeld.count(lock) == 0) {
        // throw error
        interrupt_enable();
        return -1;
    } else {

    ////UNLOCK
    if (runningThread->heldLocks.count(lock) == 0){
      interrupt_enable();
      return -1;
      //break;
    }
    else {
      if (locksHeld.count(lock) == 0){
        // There are no threads that currently hold this lock!
        interrupt_enable();
        return -1;
      }
      // Remove lock from both sets
      locksHeld.erase(lock);
      runningThread->heldLocks.erase(lock);

        queue<thread*> buffer;
        while(blockedQueue.size() > 0) {
            thread* first = blockedQueue.front();
            if(first->blockedLock == true){
              if(first->lockWait == lock){
                first->blockedLock = false;
                  first->lockWait = 0;
                  readyQueue.push(first);
              }
            } else {
              // Add to our buffer
                buffer.push(first);
            }
            //Pop the last entry
            blockedQueue.pop();
        }
        while(buffer.size() > 0) {
          // Add to blocked queue from buffer
            blockedQueue.push(buffer.front());
            buffer.pop();
        }
    }
        ///UNLOCK END

        if (checkIfExitable() == -1){
          return -1;
        }
        thread* next = readyQueue.front();
        readyQueue.pop();
        thread* prev = runningThread;
        prev->blockedCV = true;
        prev->CVLockWait = lock;
        CVQueue[cond].push_back(runningThread);
        prev->CVWait = cond;
        blockedQueue.push(prev);
        runningThread = next;
        swapcontext(prev->currContext, next->currContext);

        //LOCK BEGIN
        while(true){
          if(locksHeld.count(lock) == 0) {
                locksHeld.insert(lock);
                runningThread->heldLocks.insert(lock);
                break;
            }
            //Else, there is at least 1 lock currently being held
            else if (checkIfExitable() == 0){
              if(runningThread->heldLocks.count(lock) == 1) {
                    break;
                }
                thread* next = readyQueue.front();
                readyQueue.pop();

                runningThread->blockedLock = true;
                runningThread->lockWait = lock;

                thread* prev = runningThread;
                blockedQueue.push(prev);
                runningThread = next;

                // Last step is to swap contexts (aka swap threads)
                swapcontext(prev->currContext, next->currContext);
            }
        }
    }
    interrupt_enable();
    return 0;
}


/**
// Implementation of the thread_signal method
*/
extern int thread_signal(unsigned int lock, unsigned int cond){

	interrupt_disable();
  	if (!initCalled) {
    	interrupt_enable();
    	return -1;
  }
  	// If a thread is waiting on the conditional variable, Move it to the CV immediately!
  	if (!CVQueue[cond].empty()) {
  	  thread* wokeThread = CVQueue[cond].front();
  	  CVQueue[cond].pop_front();
  	  readyQueue.push(wokeThread);
  	}
  	interrupt_enable();
  	      return 0;

}
extern int thread_broadcast(unsigned int lock, unsigned int cond){
	interrupt_disable();
  	if (!initCalled) {
  	  interrupt_enable();
  	  return -1;
  	}

  	//Wake up ALL threads and push them into the ready queue
  	while (!CVQueue[cond].empty()) {
  	  thread* waitThread= CVQueue[cond].front();
  	  CVQueue[cond].pop_front();
  	  readyQueue.push(waitThread);
  	}
  	interrupt_enable();
  	return 0;

}
