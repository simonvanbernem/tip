#if 0
#define TIP_USE_RDTSC
#define TIP_WINDOWS
#define TIP_IMPLEMENTATION
#include "tip.h"


void profile_stuff(){
	for(int i = 0; i < 11; i++){
		char* name = "adurchlauf nummer";
		TIP_PROFILE_SCOPE(name);
		for(int j = 0; j < 9; j++){
			TIP_PROFILE_SCOPE("bJahhhhooooo");
		}
	}

	for(int i = 0; i < 11; i++){
		char* name = "bdurchlauf nummer";
		TIP_PROFILE_SCOPE(name);
		for(int j = 0; j < 9; j++){
			TIP_PROFILE_SCOPE("aJahhhhooooo");
		}
	}

	for(int i = 0; i < 11; i++){
		char* name = "cdurchlauf nummer";
		TIP_PROFILE_SCOPE(name);
		for(int j = 0; j < 9; j++){
			TIP_PROFILE_SCOPE("cJahhhhooooo");
		}
	}
}


void main(){
	tip_global_init();
	tip_thread_init();

	profile_stuff();

	auto snapshot = tip_create_snapshot(true);
	//tip_export_snapshot_to_compressed_binary(snapshot, "binary.snapshot");
	//auto snapshot2 = tip_import_snapshot_from_compressed_binary("binary.snapshot");
	tip_export_snapshot_to_chrome_json(snapshot, "json1.snapshot");
	// tip_file_format_tcb3::export_snapshot(snapshot, "binary.json");
	// auto snapshot2 = tip_file_format_tcb3::import_snapshot("binary.json");


	// assert(snapshot == snapshot2);
	/*

	tip_free_snapshot(snapshot);

	char data[] = {1,2,3,4,1,5,1,6,7,2,3,1,1,8,1,9,8,6,7,1,2,3,1,5,1,1};
	char encoded[30];
	unsigned count = 20;
	tip_file_format_compressed_binary_v3::Huffman_Encoder encoder = {};

	for(unsigned i = 0; i < count; i++){
		encoder.count_value(data + i);
	}

	encoder.create_encoder_data();

	uint64_t bit_position = 0;

	for(unsigned i = 0; i < count; i++){
		bit_position = encoder.serialize_encoded_value(encoded, bit_position, data + i);
	}
	*/

}
#endif
#define e3

#ifdef e1
#define TIP_AUTO_INIT //make TIP take care of initialization
#define TIP_IMPLEMENTATION //generate implementation in this file
#include "tip.h"

void main(){
  {
    TIP_PROFILE_SCOPE("cool stuff happening");
  }
  tip_export_state_to_chrome_json("profiling_data.json");
  //open this file with a chrome browser at the URL chrome://tracing
}
#endif

#ifdef e2

#define TIP_AUTO_INIT
#define TIP_IMPLEMENTATION
#define TIP_EVENT_BUFFER_SIZE 1024
#include "tip.h"

void burn_cpu(int index){
  TIP_PROFILE_FUNCTION();
  for(int dummy = 0; dummy < index * 1000 + 1000; dummy++){}
}

void do_stuff(int index){
  TIP_PROFILE_FUNCTION();

  if(index == 5)
    TIP_PROFILE_ASYNC_START("Time from 5");

  if(index == 17)
    TIP_PROFILE_ASYNC_STOP("Time until 17");

  {
    TIP_PROFILE_SCOPE_COND("If even, profile this scope.", index % 2 == 0);
    burn_cpu(index);
  }
  burn_cpu(index);
}

void main(){
  TIP_PROFILE_ASYNC_START("Time until 17");
  TIP_PROFILE_FUNCTION();
  TIP_PROFILE_START("manual_main");

  for(int i = 0; i < 20; i++){
    TIP_PROFILE_SCOPE("scope1");
    do_stuff(i);
  }

  TIP_PROFILE_ASYNC_STOP("Time from 5");
  TIP_PROFILE_STOP("manual_main");

  tip_export_state_to_chrome_json("profiling_data.json");
}

#endif

#ifdef e3

#define TIP_WINDOWS
#define TIP_USE_RDTSC
#define TIP_IMPLEMENTATION
#define TIP_EVENT_BUFFER_SIZE 1024 * 1024
// #define TIP_MEMORY_LIMIT
#include "tip.h"

void main(){
  tip_global_init();
  tip_set_memory_limit(16 * 1024 * 1024);
  tip_thread_init();

  for(int i = 0; i < 1000; i++){
    TIP_PROFILE_SCOPE("scope1");
    for(int j = 0; j < 1000; j++) {
      TIP_PROFILE_SCOPE("scope2");
    }
    // if (i == 3000)
    //   tip_set_memory_limit(5 * 1024 * 1024);
    // if (i == 7000)
    //   tip_set_memory_limit(0);
    // if (i == 8000)
    //   tip_set_memory_limit(128 * 1024);
  }

  printf("%.3fKiB/%.3fKiB used.\n", double(tip_get_current_memory_footprint()) / 1024., double(tip_get_memory_limit()) / 1024.);

  tip_set_memory_limit(0);
  printf("Average duration of a single profiling event is %fns.\n", tip_measure_average_duration_of_recording_a_single_profiling_event() * 1000000000.);
  tip_export_state_to_chrome_json("profiling_data.json");
}

#endif