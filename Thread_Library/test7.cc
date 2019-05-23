#include "thread.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <stdlib.h>
#include <cstring>
#include <sstream>

using namespace std;

unsigned int locks = 2;
unsigned int cv = 3;
int count = 0;

void firstThread(void* args);
void secondThread(void* args);
void thirdFourth(void* args);
void fourthThread(void* args);



void thirdFourth(void* args) {
	cout << "Third thread starts, tries to get lock" <<  endl;
	thread_lock(locks);
	cout << "Third thread gets the lock, signaling and yielding" << endl;
	thread_signal(locks, cv);
	cout << "signaled" << endl;
	//thread_yield();
	//deadlocked
}

void secondThread(void* args) {
	cout << "Second thread starts" << endl;
	thread_lock(locks);
	cout << "Second thread gets the lock" << endl;
	count += 1;
	cout << "Second thread about to wait, count now is " << count << endl;
	int val = thread_wait(locks, cv);
        cout << val << endl;
	cout << "Second thread wakes up" << endl;
	thread_unlock(locks);
	cout <<  "Second thread unlocks and exits" << endl;

}

void firstThread(void* args) {
	cout << "thread 1 starts" << endl;
	thread_create(&secondThread, args);
	cout << "created second thread" << endl;
	thread_create(&thirdThread, args);
	cout << "created third thread" << endl;


}


int main(int argc, char* argv[]) {
	cout << "main starts" << endl;
	thread_libinit(&firstThread, static_cast<void*>(&argv));


}
