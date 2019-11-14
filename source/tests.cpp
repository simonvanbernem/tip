#include <cstdlib>

bool percentage_chance(int percent){
	return (rand() % 100) < percent;
}

int realloc_fail_propability = 0;
void* funky_realloc(void* old, size_t new_size){
	if(percentage_chance(realloc_fail_propability))
		return nullptr;
	else
		return realloc(old, new_size);
}


#define TIP_REALLOC funky_realloc
#define TIP_IMPLEMENTATION
#include "tip.h"


double clocks = 0;
uint64_t recorded_zones = 0;
uint64_t emitted_zones = 0;

void startup(int32_t process_id, double clocks_per_second){
	clocks = clocks_per_second;
	(void) process_id;
}

uint64_t first_timestamp = 0;
void handle_event(tip_Event event, int32_t thread_id){
	if(first_timestamp == 0)
		first_timestamp = event.timestamp;

	if (event.type == tip_Event_Type::tip_get_new_buffer_start || event.type == tip_Event_Type::tip_get_new_buffer_stop)
		return;

	TIP_ASSERT(event.type == tip_Event_Type::start || event.type == tip_Event_Type::stop);
	recorded_zones++;

	(void) thread_id;

	// double time = double(event.timestamp - first_timestamp) / clocks;
	
	// printf("%6.2f %.*s\n", time, (int) event.name_length_including_terminator - 1, event.name);
}

void recurse(){
	tip_zone("recurse", 1); emitted_zones+=2;
	if(percentage_chance(80))
		recurse();
}

void main(){
	tip_Event_Handler handler;
	handler.handle_event_function = handle_event;
	handler.startup_function = startup;

	tip_set_event_handler(handler);

	tip_set_global_toggle(true);
	for(int i = 0; i < 1000; i++){
		tip_zone("loop", 1); emitted_zones+=2;
		// Sleep(1);
		recurse();
		tip_zone_start("a", 1); emitted_zones++;
		recurse();
		tip_zone_stop(1); emitted_zones++;
		recurse();
	}

	tip_debug_print_events_written();
	tip_sleep_until_all_events_are_handled();
	printf("emmitted: %llu, recorded: %llu\n", emitted_zones, recorded_zones);
}
