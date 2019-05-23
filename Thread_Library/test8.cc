#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "thread.h"
using namespace std;

unsigned int num_locks = 1;
unsigned int cv = 2;
int count = 0;
int val;

void firstThread(void* args);
void secondThread(void* args);

void thirdThread(void* args) {
	cout << "third thread starts" << endl;
	val = thread_lock(num_locks);
	cout << val << " thread 3 obtains lock" << endl;
	val = thread_unlock(num_locks);
  thread_yield();
	cout << val << " thread 3 unlocks" << endl;
}

void secondThread(void* args) {
	cout << count << " thread copy starts" << endl;
	int local_count = count;
	count += 1;
	if(count < 250) {
		val = thread_create(&thirdThread, args);
		cout << val << " new thread copy created" << endl;
		val = thread_yield();
	}
}

void mainThread(void* args) {
	cout << "first thread starts" << endl;
	val = thread_create(&secondThread, args);
}

int main(int argc, char* argv[]) {
	//start_preemptions(true, true, 1024); //5 98 1024 giving errors 309 wait correct
	cout << "main begins" << endl;
	thread_libinit(&mainThread, static_cast<void*>(&argv));
}
