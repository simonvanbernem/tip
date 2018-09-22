#define TIP_USE_RDTSC
#define TIP_WINDOWS
#define TIP_IMPLEMENTATION
#define TIP_ASSERT(condition)
#include "tip.h"

int main(){
	
	tip_global_init();
	tip_thread_init();
	{
		TIP_PROFILE_SCOPE("hallo");
	}
	tip_export_snapshot_to_chrome_json(tip_create_snapshot(true), "2.json");
}

/*
#include <chrono>
#include <thread>
void sleep(int time){
	std::this_thread::sleep_for(std::chrono::milliseconds(time));
}

template<typename T>
T* alloc(unsigned count){
	return (T*) malloc(sizeof(T) * count);
}

int main(){
	
	tip_global_init();
	tip_thread_init();

	printf("size of signed is %d\n", int(sizeof(signed)));

	for(int i = 0; i < 11; i++){
		char* name = "adurchlauf nummer";
		TIP_PROFILE_SCOPE(name);
		for(int j = 0; j < 9; j++){
			TIP_PROFILE_SCOPE("bJahhhhooooo");
		}
	}

	tip_export_snapshot_to_chrome_json(tip_create_snapshot(), "1.json");

	for(int i = 0; i < 11; i++){
		char* name = "bdurchlauf nummer";
		TIP_PROFILE_SCOPE(name);
		for(int j = 0; j < 9; j++){
			TIP_PROFILE_SCOPE("aJahhhhooooo");
		}
	}

	tip_export_snapshot_to_chrome_json(tip_create_snapshot(true), "2.json");

	for(int i = 0; i < 11; i++){
		char* name = "cdurchlauf nummer";
		TIP_PROFILE_SCOPE(name);
		for(int j = 0; j < 9; j++){
			TIP_PROFILE_SCOPE("cJahhhhooooo");
		}
	}

	tip_export_snapshot_to_chrome_json(tip_create_snapshot(true), "3.json");


	return 0;
}

*/