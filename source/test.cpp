#define TIP_USE_RDTSC
#define TIP_WINDOWS
// #define TIP_PORTABLE
#include "tip.h"

#include <assert.h>
#include <chrono>
#include <thread>
void sleep(int time){
	std::this_thread::sleep_for(std::chrono::milliseconds(time));
}

void do_stuff_a(bool b){
	TIP_PROFILE_SCOPE(0);
	if(b)
		sleep(3);
};

void do_stuff_b(bool b){
	TIP_PROFILE_SCOPE(1);
	if(b){
		do_stuff_a(true);
	}else{
		sleep(10);
		do_stuff_b(!b);
	}

	for(int i=0;i<10;i++){
		do_stuff_a(false);
	}

};

void do_stuff_c(){
	TIP_PROFILE_SCOPE(2);
	TIP_PROFILE_SCOPE(2);
	sleep(4);
	do_stuff_b(true);
	do_stuff_b(false);
	sleep(10);
	TIP_PROFILE_ASYNC_START(1);
	for(int i=0;i<10;i++){
		do_stuff_b(false);
	}
	TIP_PROFILE_ASYNC_STOP(2);

	sleep(1);
}

int main(){
	tip_global_init();
	tip_thread_init();
	assert(0 == tip_add_name("do_stuff_a"));
	assert(1 == tip_add_name("do_stuff_b"));
	assert(2 == tip_add_name("do_stuff_c"));
	tip_save_profile_event(tip_get_timestamp(), 0, tip_Event_Type::start);
	TIP_PROFILE_ASYNC_START(2);
	do_stuff_c();
	TIP_PROFILE_ASYNC_STOP(1);

	tip_export_snapshot_to_chrome_json(tip_create_snapshot(), "test.json");
	return 0;
}