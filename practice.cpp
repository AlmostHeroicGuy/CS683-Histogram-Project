#include <iostream>
#include <omp.h>
using namespace std;

int main() {
	cout << "before: only 1 thread here" << endl;

	#pragma omp parallel num_threads(4)
	{
		int id = omp_get_thread_num();
		cout << "hello from thread " << id << endl;
	}

	cout << "after: back to 1 thread" << endl;
	return 0;
}
