#include"ThreadPool.h"
#include<unistd.h>
#include<time.h>
#include<stdio.h>
#include<mutex>
std::mutex mt;
using namespace std;
int main(int argc, char* argv[]) {

	
	ThreadPool *pool= new ThreadPool(2);
	for (int i = 0; i < 5; i++) {
		pool->Enqueue([=]() {
			printf("time%d=%ld\n",i, (long)time(NULL));	
			std::lock_guard<std::mutex> lk(mt);
			sleep(1);
			printf("time%d=%ld\n",i, (long)time(NULL));
		});
	}
	
	while(true) {
		sleep(1);
	}
}		
