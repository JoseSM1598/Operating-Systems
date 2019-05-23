#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "thread.h"


using namespace std;

unsigned int arg = 161;

void first_thread(void *argg);
void second_thread(void *argg);
void third_thread(void *argg);

//TEST: thread exists while still holding lock, this should be ok, thread library should still exit

int main(int argc, char *argv[]){
	thread_libinit((thread_startfunc_t) first_thread, NULL);
}
void first_thread(void *argg) {
	cout << "Start first_thread\n";
	unsigned int arg2= arg;
	cout << "signaling without lock: " << arg2-1 << endl;
	thread_signal(arg2-1, 1);
	cout << "acquiring lock: " <<arg2<< endl;
	thread_lock(arg2);
	cout << "got lock: " <<arg2<< endl;
	thread_create((thread_startfunc_t) second_thread, NULL);
	cout << "done creating second_thread\n";
	thread_yield();
	cout << "End first_thread\n";
}

void second_thread(void *argg) {
	cout << "Start second_thread\n";
	unsigned int arg2= arg;
	cout << "acquiring lock: " <<arg2<< endl;
	thread_lock(arg2+1);
	cout << "got lock: " <<arg2<< endl;
	cout << "second_thread yield " << endl;
	thread_yield();
	cout << "second_thread return from yield" << endl;
	cout << "broadcast lock: " <<arg2<< endl;
	thread_broadcast(arg2, 1);
	cout << "broadcast without lock: " << arg2+1 << endl;
	thread_broadcast(arg2+1, 1);
	cout << "request lock: " <<arg2<< endl;
	thread_lock(arg2);
	cout << "got lock: " <<arg2<< endl;
	thread_unlock(arg2);
	cout << "unlock: " <<arg2<< endl;
	cout << "create first_thread " << endl;
	thread_create((thread_startfunc_t) first_thread, NULL);
	cout << "second_thread yield" << endl;
	thread_yield();
	cout << "second_thread return from yield" << endl;
	thread_unlock(arg2+1);
	cout << "unlock: " << arg2+1 << endl;
	cout << "End second_thread\n";
}

void third_thread(void *argg){
  cout << "Start second_thread\n";
  unsigned int arg2= arg;
  thread_lock(arg2+2);
  cout << "got lock: " <<arg2<< endl;
	cout << "third_thread yield " << endl;
	thread_yield();
  thread_unlock(arg2);
  cout << "unlock: " << arg2+1 << endl;
	cout << "End third_thread\n";

}
