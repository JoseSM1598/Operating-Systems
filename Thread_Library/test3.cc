#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "thread.h"
using namespace std;


using namespace std;

unsigned int arg = 161;

void first_thread(void* ptr);
void second_thread(void* ptr);

//Test: unlock a lock you don't have and lock then try to acquire it again, and exit with locks still held

int main(int argc, char* argv[]){
  //Create the library
	thread_libinit((thread_startfunc_t) first_thread, NULL);
}

void first_thread(void* ptr){
	cout << "Start first_thread\n";
	cout << "create second_thread\n";
	thread_create((thread_startfunc_t) second_thread, NULL);
	cout << "unlock: " << arg<< endl;
	thread_unlock(arg);
	cout << "Finish first_thread\n";
}
void second_thread(void* ptr){
	cout << "Start second_thread\n";
	cout << "request lock: " << arg +1 << endl;
	thread_lock(arg+1);
	cout << "Unlock: " << arg << endl;
	thread_unlock(arg);
	cout << "request lock: " << arg << endl;
	thread_lock(arg);
	cout << "Finish second_thread\n";
}
