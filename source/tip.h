#pragma once
#include <stdint.h>
#include <vector>
#include <assert.h>

#ifdef TIP_PORTABLE
#include <mutex>
#include <chrono>
#include <thread>
#include <map>
#elif defined(TIP_WINDOWS)
#include "Windows.h"
#endif

using u64 = uint64_t;
using u32 = uint32_t;
using s64 = int64_t;
using s32 = int32_t;
using f64 = double;

#define TIP_USE_RDTSC

const u64 tip_event_buffer_size = 1024 * 1024;


#ifdef TIP_DISABLED
	#define TIP_PROFILE_SCOPE(id)

	#define TIP_PROFILE_START(id)
	#define TIP_PROFILE_STOP(id)

	#define TIP_PROFILE_ASYNC_START(id)
	#define TIP_PROFILE_ASYNC_STOP(id)
#else

	#define TIP_CONCAT_LINE_NUMBER(x, y) x ## y // and you also need this somehow. c++ is stupid
	#define TIP_CONCAT_LINE_NUMBER2(x, y) TIP_CONCAT_LINE_NUMBER(x, y) // this just concats "profauto" and the line number
	#define TIP_PROFILE_SCOPE(id) tip_Scope_Profiler TIP_CONCAT_LINE_NUMBER2(profauto, __LINE__)(id); //for id=0 and and line 23, this expands to "tip_Scope_Profiler profauto23(0);"

	#define TIP_PROFILE_START(id) tip_save_profile_event(tip_get_timestamp(), id, tip_Event_Type::start);
	#define TIP_PROFILE_STOP(id) tip_save_profile_event(tip_get_timestamp(), id, tip_Event_Type::stop);

	#define TIP_PROFILE_ASYNC_START(id) tip_save_profile_event(tip_get_timestamp(), id, tip_Event_Type::start_async);
	#define TIP_PROFILE_ASYNC_STOP(id) tip_save_profile_event(tip_get_timestamp(), id, tip_Event_Type::stop_async);
#endif


enum class tip_Event_Type{
	start = 0,
	stop = 1,
	start_async = 2,
	stop_async = 3,
	enum_size = 4
};

struct tip_Event{
	u64 timestamp;
	u64 name_index;
	tip_Event_Type type;
};

struct tip_Snapshot{
	f64 clocks_per_second;
	s32 process_id;
	u64 number_of_events;

	std::vector<std::string> names;
	std::vector<s32> thread_ids;
	std::vector<std::vector<tip_Event>> events; // the inner array contains the events of one thread.
};

#ifdef TIP_PORTABLE
struct Mutex{
	std::mutex* mutex;
};

Mutex tip_create_mutex(){
	return {new std::mutex};
}

void tip_lock_mutex(Mutex mutex){
	mutex.mutex->lock();
}

void tip_unlock_mutex(Mutex mutex){
	mutex.mutex->unlock();
}

u64 tip_get_timestamp(){
	assert(std::chrono::high_resolution_clock::is_steady);
	auto timepoint = std::chrono::high_resolution_clock::now().time_since_epoch();
	auto timepoint_as_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(timepoint);
	return timepoint_as_duration.count();
}

s32 tip_get_thread_id(){
	static std::mutex vector_mutex;
	static s32 current_thread_id = 0;
	static std::map<std::thread::id, s32> thread_id_map;

	std::thread::id thread_id = std::this_thread::get_id();
	s32 thread_id_s32 = 0; 

	vector_mutex.lock();
	
	auto iterator = thread_id_map.find(thread_id);
	if(iterator != thread_id_map.end()){
		thread_id_s32 = thread_id_map[thread_id];
	}
	else{
		thread_id_map[thread_id] = current_thread_id;
		thread_id_s32 = current_thread_id;
		current_thread_id++;
	}
	vector_mutex.unlock();

	return thread_id_s32;
}

s32 tip_get_process_id(){
	return 0;
}

u64 tip_get_reliable_timestamp(){
	return tip_get_timestamp();
}

u64 tip_get_reliable_timestamp_frequency(){
	return 1000000000;
}

#elif defined(TIP_WINDOWS)

//THREADING
struct Mutex{
	HANDLE handle;
};

Mutex tip_create_mutex(){
	Mutex mutex;
	mutex.handle = CreateMutex(0, false, 0);
	return mutex;
}

void tip_lock_mutex(Mutex mutex){
	WaitForSingleObject(mutex.handle, INFINITE);
}

void tip_unlock_mutex(Mutex mutex){
	ReleaseMutex(mutex.handle);
}

//TIMING
u64 tip_get_timestamp(){
#ifdef TIP_USE_RDTSC
	return __rdtsc();
#else
	LARGE_INTEGER temp;
	QueryPerformanceCounter(&temp);
	return temp.QuadPart;
#endif
}

s32 tip_get_thread_id(){
	return GetCurrentThreadId();
}

s32 tip_get_process_id(){
	return GetCurrentProcessId();
}

u64 tip_get_reliable_timestamp(){
	LARGE_INTEGER temp;
	QueryPerformanceCounter(&temp);
	return temp.QuadPart;
}

u64 tip_get_reliable_timestamp_frequency(){
	LARGE_INTEGER temp;
	QueryPerformanceFrequency(&temp);
	return temp.QuadPart;
}

#endif

struct tip_Thread_State{
	bool initialized = false;
	s32 thread_id;

	tip_Event* current_event_buffer;
	u64 current_position_in_event_buffer;

	Mutex event_buffers_mutex;
	std::vector<tip_Event*> event_buffers;
};

struct tip_Global_State{
	bool initialized = false;
	s32 process_id;
	f64 clocks_per_second;
	std::vector<std::string> names;

	Mutex thread_states_mutex;
	std::vector<tip_Thread_State*> thread_states;
};

thread_local tip_Thread_State tip_thread_state;
static tip_Global_State tip_global_state;


inline void get_new_event_buffer_if_necessairy(){
	if(tip_thread_state.current_position_in_event_buffer < tip_event_buffer_size)
		return;

	tip_lock_mutex(tip_thread_state.event_buffers_mutex);
	tip_thread_state.current_event_buffer = (tip_Event*) malloc(tip_event_buffer_size * sizeof(tip_Event));
	tip_thread_state.current_position_in_event_buffer = 0;
	tip_thread_state.event_buffers.push_back(tip_thread_state.current_event_buffer);
	tip_unlock_mutex(tip_thread_state.event_buffers_mutex);
}

void tip_save_profile_event(u64 timestamp, u64 name_index, tip_Event_Type type){
	get_new_event_buffer_if_necessairy();
	tip_thread_state.current_event_buffer[tip_thread_state.current_position_in_event_buffer] = {timestamp, name_index, type};
	tip_thread_state.current_position_in_event_buffer++;
}

struct tip_Scope_Profiler{
	u64 name_index;

	tip_Scope_Profiler(u64 id){
		tip_save_profile_event(tip_get_timestamp(), id, tip_Event_Type::start);
		name_index = id;
	}

	~tip_Scope_Profiler(){
		tip_save_profile_event(tip_get_timestamp(), name_index, tip_Event_Type::stop);
	}
};


void tip_thread_init(){
#ifndef TIP_DISABLED
	assert(tip_global_state.initialized);

	if(tip_thread_state.initialized)
		return;

	tip_thread_state.thread_id = tip_get_thread_id();

	tip_thread_state.current_event_buffer = (tip_Event*) malloc(tip_event_buffer_size * sizeof(tip_Event));
	tip_thread_state.current_position_in_event_buffer = 0;

	tip_thread_state.event_buffers.push_back(tip_thread_state.current_event_buffer);
	tip_thread_state.event_buffers_mutex = tip_create_mutex();
	tip_thread_state.initialized = true;

	tip_lock_mutex(tip_global_state.thread_states_mutex);
	tip_global_state.thread_states.push_back(&tip_thread_state);
	tip_unlock_mutex(tip_global_state.thread_states_mutex);
#endif
}

f64 tip_global_init(){
#ifndef TIP_DISABLED
	if(tip_global_state.initialized)
		return 1. / tip_global_state.clocks_per_second;

#if defined(TIP_USE_RDTSC) && defined(TIP_WINDOWS) 
	u64 reliable_start = tip_get_reliable_timestamp();
	u64 rdtsc_start = __rdtsc();

	Sleep(10);

	u64 reliable_end = tip_get_reliable_timestamp();
	u64 rdtsc_end = __rdtsc();

	s64 rdtsc_diff = rdtsc_end - rdtsc_start;
	s64 reliable_diff = reliable_end - reliable_start;

	assert(rdtsc_diff > 0 && reliable_diff > 0);

	f64 time_passed = f64(reliable_diff) / f64(tip_get_reliable_timestamp_frequency());
	tip_global_state.clocks_per_second = rdtsc_diff / time_passed;

#else
	tip_global_state.clocks_per_second = f64(tip_get_reliable_timestamp_frequency());
#endif

	tip_global_state.process_id = tip_get_process_id();
	tip_global_state.initialized = true;
	tip_global_state.thread_states_mutex = tip_create_mutex();
	return 1. / tip_global_state.clocks_per_second;
#else
	return 0;
#endif
}

int tip_add_name(std::string name){
#ifndef TIP_DISABLED
	int id = int(tip_global_state.names.size());
	tip_global_state.names.push_back(name.c_str());
	return id;
#else
	return -1;
#endif
}

tip_Snapshot tip_create_snapshot(bool erase_snapshot_data_from_internal_state = false){
	tip_Snapshot snapshot;
#ifndef TIP_DISABLED
	snapshot.clocks_per_second = tip_global_state.clocks_per_second;
	snapshot.process_id = tip_global_state.process_id;
	snapshot.names = tip_global_state.names;

	tip_lock_mutex(tip_global_state.thread_states_mutex);

	for(int i = 0; i < tip_global_state.thread_states.size(); i++){

		tip_Thread_State* thread_state = tip_global_state.thread_states[i];
		snapshot.thread_ids.push_back(thread_state->thread_id);
		tip_lock_mutex(thread_state->event_buffers_mutex);

		std::vector<tip_Event> thread_events;

		for(tip_Event* event_buffer : thread_state->event_buffers){
			int events_in_buffer = tip_event_buffer_size;

			if(event_buffer == thread_state->current_event_buffer)
				events_in_buffer = int(thread_state->current_position_in_event_buffer);

			for(int j = 0; j < events_in_buffer; j++){
				thread_events.push_back(event_buffer[j]);
			}

			if(erase_snapshot_data_from_internal_state)
				free(event_buffer);
		}

		if(erase_snapshot_data_from_internal_state){
			thread_state->event_buffers.clear();
			thread_state->current_event_buffer = (tip_Event*) malloc(tip_event_buffer_size * sizeof(tip_Event));
			thread_state->current_position_in_event_buffer = 0;
			thread_state->event_buffers.push_back(thread_state->current_event_buffer);
		}

		snapshot.events.push_back(thread_events);

		tip_unlock_mutex(thread_state->event_buffers_mutex);
	}

	tip_unlock_mutex(tip_global_state.thread_states_mutex);
#endif
	return snapshot;
}

s64 tip_export_snapshot_to_chrome_json(tip_Snapshot snapshot, std::string file_name){
#ifndef TIP_DISABLED
	FILE* file = fopen(file_name.c_str(), "w+");
	fprintf(file, "{\"traceEvents\": [\n");

	bool first = true;

	std::vector<tip_Event> event_stack;

	for(int thread_index = 0; thread_index < snapshot.thread_ids.size(); thread_index++){
		s32 thread_id = snapshot.thread_ids[thread_index];
		for(tip_Event event : snapshot.events[thread_index]){

			if(event.type == tip_Event_Type::start) //we put this event on the stack, so we can check if we can form duration events using this and its corresponding close event later
				event_stack.push_back(event);

			else if(event.type == tip_Event_Type::stop){ //we check if the last thing on the stack is the corresponding start event for this stop event. if so, we merge both into a duration event and print it
				if(event_stack.empty())
					event_stack.push_back(event);
				else{
					tip_Event last_event_on_stack = event_stack[event_stack.size() - 1];
					if(last_event_on_stack.type == tip_Event_Type::start && last_event_on_stack.name_index == event.name_index){
						event_stack.pop_back();

						if(first)
							first = false;
						else
							fprintf(file, ",\n");

						const char* name = snapshot.names[event.name_index].c_str();
						f64 timestamp = f64(last_event_on_stack.timestamp) / snapshot.clocks_per_second * 1000000.;
						f64 duration = f64(event.timestamp - last_event_on_stack.timestamp) / snapshot.clocks_per_second * 1000000.;


						fprintf(file,"  {\"name\":\"%s\","
										"\"cat\":\"PERF\","
										"\"ph\":\"X\","
										"\"pid\":%d,"
										"\"tid\":%d,"
										"\"id\":100,"
										"\"ts\":%.16e,"
										"\"dur\":%.16e}", name, snapshot.process_id, thread_id, timestamp, duration);
					}
					else{
						event_stack.push_back(event);
					}
				}
			}
			else{    //the only type of events left are the asynchronous ones. we just print these directly
				f64 timestamp = f64(event.timestamp) / snapshot.clocks_per_second * 1000000.;
				const char* name = snapshot.names[event.name_index].c_str();
				char type = '!';
				if(event.type == tip_Event_Type::start_async)
					type = 'b';
				else
					type = 'e';

				if(first)
					first = false;
				else
					fprintf(file, ",\n");

				fprintf(file,"  {\"name\":\"%s\","
								"\"cat\":\"PERF\","
								"\"ph\":\"%c\","
								"\"pid\":%d,"
								"\"tid\":%d,"
								"\"id\":100,"
								"\"ts\":%.16e}", name, type, snapshot.process_id, thread_id, timestamp);
			}
		}

		for(tip_Event event : event_stack){ //print all start and stop events, that don't have a corresponding event they could form a duration event with
			f64 timestamp = f64(event.timestamp) / snapshot.clocks_per_second * 1000000.;
			const char* name = snapshot.names[event.name_index].c_str();
			char type = '!';
			if(event.type == tip_Event_Type::start)
				type = 'B';
			else
				type = 'E';

			if(first)
				first = false;
			else
				fprintf(file, ",\n");

			fprintf(file,"  {\"name\":\"%s\","
							"\"cat\":\"PERF\","
							"\"ph\":\"%c\","
							"\"pid\":%d,"
							"\"tid\":%d,"
							"\"id\":100,"
							"\"ts\":%.16e}", name, type, snapshot.process_id, thread_id, timestamp);
		}

		event_stack.clear();
	}

	fprintf(file, "\n],\n\"displayTimeUnit\": \"ns\"\n}");
	u64 size = u64(ftell(file));
	fclose(file);
	return size;
#else
	return -1;
#endif
}