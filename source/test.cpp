#define TIP_USE_RDTSC
#define TIP_WINDOWS
// #define TIP_PORTABLE
#include "tip.h"

#include <assert.h>
#include <chrono>
#include <thread>
#include <string>
void sleep(int time){
	std::this_thread::sleep_for(std::chrono::milliseconds(time));
}

int main(){
	tip_global_init();
	tip_thread_init();

	for(int i = 0; i < 100; i++){
		char name[30];
		sprintf(name, "durchlauf nummer %d", i);
		TIP_PROFILE_SCOPE(name);
		for(int j = 0; j < 10; j++){
			TIP_PROFILE_SCOPE("Jahhhhooooo");
			//sleep(1);
		}
	}

	tip_export_snapshot_to_chrome_json(tip_create_snapshot(), "neuer.json");
	return 0;
}