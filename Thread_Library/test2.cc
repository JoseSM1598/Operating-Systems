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


unsigned int l = 2;
unsigned int cv = 3;
int count = 0;


void test_func2(void* args) {
	cout << "thread 2 starts" << endl;
}

void test_func3(void* args) {
	cout << "thread 3 starts" <<  endl;
}

void test_func4(void* args){
	cout << "thread 4 starts" << endl;
}


void test_func1(void* args) {
	cout << "thread 1 starts" << endl;
	thread_create(&test_func2, args);
	cout << "created thread 2" << endl;
	thread_create(&test_func3, args);
	cout << "created thread 3" << endl;
	thread_create(&test_func4, args);
	cout << "created thread 4" << endl;
}


int main(int argc, char* argv[]) {
	//start_preemptions(true, true, 13); //303 giving errors
	cout << "main starts" << endl;
	thread_libinit(&test_func1, static_cast<void*>(&argv));


}
