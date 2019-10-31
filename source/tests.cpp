#include <cstdlib>

bool percentage_chance(int percent){
	return (rand() % 100) < percent;
}

int realloc_fail_propability = 50;
void* funky_realloc(void* old, size_t new_size){
	if(percentage_chance(realloc_fail_propability))
		return nullptr;
	else
		return realloc(old, new_size);
}


#define TIP_REALLOC funky_realloc
#define TIP_IMPLEMENTATION
#define TIP_MEMORY_LIMIT
#include "tip.h"


void recurse(){
	tip_zone("recurse", 1);
	if(percentage_chance(80))
		recurse();
}

void main(){
	tip_set_global_toggle(true);
	for(int i = 0; i < 10000; i++){
		tip_zone("loop", 1);
		recurse();
		tip_zone_start("a", 1);
		if(percentage_chance(5))
			tip_set_memory_limit(tip_get_current_memory_footprint() - 100);
		recurse();
		tip_zone_stop(1);
		if(percentage_chance(20))
			tip_set_memory_limit(tip_get_current_memory_footprint() + TIP_EVENT_BUFFER_SIZE * 3);
		recurse();
	}
	realloc_fail_propability = 0;
	tip_export_current_state_to_chrome_json("test_output.json");
}
