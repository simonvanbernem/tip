#pragma once
#include <stdint.h>
#include <vector>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#ifdef TIP_PORTABLE
#include <mutex>
#include <chrono>
#include <thread>
#include <map>
#elif defined(TIP_WINDOWS)
#include "Windows.h"
#endif

using u8 = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s32 = int32_t;
using s64 = int64_t;

using f32 = float;
using f64 = double;

const u64 tip_event_buffer_size = 1024 * 1024;


template<typename T>
struct Dynamic_Array{
	T* buffer;
	s64 size;
	s64 capacity;
	bool initialized = false;

	void init(s64 initial_capacity = 32){
		assert(!initialized);
		if(initial_capacity > 0)
			buffer = (T*) malloc(sizeof(T) * initial_capacity);
		else 
			buffer = nullptr;

		capacity = initial_capacity;
		size = 0;
		initialized = true;
	}

	void insert(T element, s64 count = 1){
		assert(initialized);
		s64 new_size = size + count;

		if(new_size > capacity){
			capacity *= 2;
			assert(new_size <= capacity);
			realloc(buffer, capacity);
		}

		for(s64 i = size; i < new_size; i++){
			buffer[i] = element;
		}

		size = new_size;
	}

	void insert(T* elements, s64 number_of_elements){
		assert(initialized);
		s64 new_size = size + number_of_elements;

		if(new_size > capacity){
			capacity *= 2;
			assert(new_size <= capacity);
			realloc(buffer, capacity);
		}

		memcpy(buffer + size, elements, number_of_elements);

		size = new_size;
	}

	T& operator[](s64 index){
		assert(initialized);
		return buffer[index];
	}

	void destroy(){
		assert(initialized);
		free(buffer);
		buffer = nullptr;
		size = -1;
		capacity = -1;
		initialized = false;
	}
};

struct String_Interning_Hash_Table{
	Dynamic_Array<char> name_buffer;
	Dynamic_Array<s64> name_indices;
	s64 count;
	bool initialized = false;

	void init(s64 size = 256){
		assert(!initialized);
		name_buffer.init(size * 16); //just a random guess that on average a name will have 15 characters (+0 terminator)
		name_indices.init(size);
		name_indices.insert(-1, size);
		initialized = true;
	}

	u32 fvn_hash(const char* string, s64 length)
	{
		u32 hash = 2166136261; // offset basis (32 bits)
		for (s64 i = 0; i < length; i++){
			hash ^= string[i];
			hash *= 16777619;
		}
		return hash;
	}

	void resize_table(){
		s64 new_size = name_indices.size * 2;
		Dynamic_Array<s64> new_name_indices;
		new_name_indices.init(new_size);
		new_name_indices.insert(-1, new_size);

		for(int i = 0; i < name_indices.size; i++){
			if(name_indices[i] == -1)
				return;

			char* string = name_buffer.buffer + name_indices[i];
			s64 string_length = strlen(string);
			s64 hash_index = fvn_hash(string, string_length) % new_name_indices.size;

			while(new_name_indices[hash_index] != -1) //linear probing, we know that this is a collision since every string is unique in the hashmap
					hash_index = (hash_index + 1) % name_indices.size;

			new_name_indices[hash_index] = name_indices[i];
		}

		name_indices.destroy();
		name_indices = new_name_indices;
	}

	s64 intern_string(char* string){
		assert(initialized);
		s64 string_length = strlen(string);
		s64 hash_index = fvn_hash(string, string_length) % name_indices.size;


		while(name_indices[hash_index] != -1){ //linear probing
			char* found_string = name_buffer.buffer + name_indices[hash_index];
			bool equal = strcmp(string, found_string) == 0;

			if(equal)
				return name_indices[hash_index];
			else
				hash_index = (hash_index + 1) % name_indices.size;
		}

		s64 interned_string_id = name_buffer.size;
		name_indices[hash_index] = interned_string_id;
		name_buffer.insert(string, string_length + 1);
		count++;

		if(f32(count) > 2.f / 3.f * f32(name_indices.size))
			resize_table();

		return interned_string_id;
	}

	char* get_string(s64 id){
		assert(initialized);
		return name_buffer.buffer + id;
	}

	void destroy(){
		assert(initialized);
		name_buffer.destroy();
		name_indices.destroy();
		initialized = false;
	}
};



#define TIP_USE_RDTSC

#ifdef TIP_DISABLED
	#define TIP_PROFILE_SCOPE(name)

	#define TIP_PROFILE_START(name)
	#define TIP_PROFILE_STOP(name)

	#define TIP_PROFILE_ASYNC_START(name)
	#define TIP_PROFILE_ASYNC_STOP(name)
#else

	#define TIP_CONCAT_LINE_NUMBER(x, y) x ## y // and you also need this somehow. c++ is stupid
	#define TIP_CONCAT_LINE_NUMBER2(x, y) TIP_CONCAT_LINE_NUMBER(x, y) // this just concats "profauto" and the line number
	#define TIP_PROFILE_SCOPE(name) tip_Scope_Profiler TIP_CONCAT_LINE_NUMBER2(profauto, __LINE__)(name); //for id=0 and and line 23, this expands to "tip_Scope_Profiler profauto23(0);"

	#define TIP_PROFILE_START(name) tip_save_profile_event(tip_get_timestamp(), name, tip_Event_Type::start);
	#define TIP_PROFILE_STOP(name) tip_save_profile_event(tip_get_timestamp(), name, tip_Event_Type::stop);

	#define TIP_PROFILE_ASYNC_START(name) tip_save_profile_event(tip_get_timestamp(), name, tip_Event_Type::start_async);
	#define TIP_PROFILE_ASYNC_STOP(name) tip_save_profile_event(tip_get_timestamp(), name, tip_Event_Type::stop_async);
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
	s64 name_id;
	tip_Event_Type type;
};

struct tip_Snapshot{
	f64 clocks_per_second;
	s32 process_id;
	u64 number_of_events;

	String_Interning_Hash_Table names;
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

struct tip_Event_Buffer{
	u8* data;
	u8* end;
	u8* current_position;
	u8* position_of_first_event; //this is used to delete events from a buffer when making a snapshot
	tip_Event_Buffer* next_buffer;
};

struct tip_Thread_State{
	bool initialized = false;
	s32 thread_id;

	tip_Event_Buffer* first_event_buffer;
	tip_Event_Buffer* current_event_buffer;
};

struct tip_Global_State{
	bool initialized = false;
	s32 process_id;
	f64 clocks_per_second;

	Mutex thread_states_mutex;
	std::vector<tip_Thread_State*> thread_states;
};

thread_local tip_Thread_State tip_thread_state;
static tip_Global_State tip_global_state;


void tip_get_new_event_buffer(){
	tip_Event_Buffer* new_buffer = (tip_Event_Buffer*) malloc(sizeof(tip_Event_Buffer));
	new_buffer->data = (u8*) malloc(tip_event_buffer_size);
	new_buffer->end = new_buffer->data + tip_event_buffer_size;
	new_buffer->current_position = new_buffer->data;
	new_buffer->position_of_first_event = new_buffer->data;
	new_buffer->next_buffer = nullptr;

	tip_thread_state.current_event_buffer->next_buffer = new_buffer;
	tip_thread_state.current_event_buffer = new_buffer;
}

void tip_save_profile_event(u64 timestamp, char* name, tip_Event_Type type){
	u64 name_length_including_terminator = strlen(name) + 1;
	u64 event_size = sizeof(timestamp) + sizeof(type) + sizeof(name_length_including_terminator) + name_length_including_terminator;
	
	if(event_size + tip_thread_state.current_event_buffer->current_position > tip_thread_state.current_event_buffer->end)
		tip_get_new_event_buffer();

	tip_Event_Buffer* buffer = tip_thread_state.current_event_buffer; 

	u8* data_pointer = buffer->current_position;
	*((u64*)data_pointer) = timestamp;
	data_pointer += sizeof(timestamp);
	*((tip_Event_Type*)data_pointer) = type;
	data_pointer += sizeof(type);
	*((u64*)data_pointer) = name_length_including_terminator;
	data_pointer += sizeof(name_length_including_terminator);
	memcpy(data_pointer, name, name_length_including_terminator);
	data_pointer += name_length_including_terminator;

	buffer->current_position = data_pointer;
}

struct tip_Scope_Profiler{
	char* name;

	tip_Scope_Profiler(char* event_name){
		tip_save_profile_event(tip_get_timestamp(), event_name, tip_Event_Type::start);
		name = event_name;
	}

	~tip_Scope_Profiler(){
		tip_save_profile_event(tip_get_timestamp(), name, tip_Event_Type::stop);
	}
};


void tip_thread_init(){
#ifndef TIP_DISABLED
	assert(tip_global_state.initialized);

	if(tip_thread_state.initialized)
		return;

	tip_thread_state.thread_id = tip_get_thread_id();

	tip_Event_Buffer* new_buffer = (tip_Event_Buffer*) malloc(sizeof(tip_Event_Buffer));

	new_buffer->data =(u8*) malloc(tip_event_buffer_size);
	new_buffer->end = new_buffer->data + tip_event_buffer_size;
	new_buffer->current_position = new_buffer->data;
	new_buffer->position_of_first_event = new_buffer->data;
	new_buffer->next_buffer = nullptr;

	tip_thread_state.current_event_buffer = new_buffer;
	tip_thread_state.first_event_buffer = new_buffer; 

	tip_lock_mutex(tip_global_state.thread_states_mutex);
	tip_global_state.thread_states.push_back(&tip_thread_state);
	tip_unlock_mutex(tip_global_state.thread_states_mutex);

	tip_thread_state.initialized = true;
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

tip_Snapshot tip_create_snapshot(bool erase_snapshot_data_from_internal_state = false){
	tip_Snapshot snapshot;
#ifndef TIP_DISABLED
	snapshot.clocks_per_second = tip_global_state.clocks_per_second;
	snapshot.process_id = tip_global_state.process_id;
	snapshot.names.init(256);

	tip_lock_mutex(tip_global_state.thread_states_mutex);

	for(int i = 0; i < tip_global_state.thread_states.size(); i++){

		tip_Thread_State* thread_state = tip_global_state.thread_states[i];
		snapshot.thread_ids.push_back(thread_state->thread_id);

		std::vector<tip_Event> thread_events;

		tip_Event_Buffer* event_buffer = thread_state->first_event_buffer;

		while(event_buffer){
			u8* data_pointer = event_buffer->position_of_first_event;

			while(data_pointer != event_buffer->current_position){
				tip_Event event;
				event.timestamp = *((u64*)data_pointer);
				data_pointer += sizeof(event.timestamp);
				event.type = *((tip_Event_Type*)data_pointer);
				data_pointer += sizeof(event.type);
				u64 name_length_including_terminator = *((u64*)data_pointer);
				data_pointer += sizeof(name_length_including_terminator);
				event.name_id = snapshot.names.intern_string((char*)data_pointer);
				data_pointer += name_length_including_terminator;
				thread_events.push_back(event);
			}

			if(erase_snapshot_data_from_internal_state){
				if(thread_state->current_event_buffer == event_buffer){
					event_buffer->position_of_first_event = data_pointer;
					event_buffer = event_buffer->next_buffer;
				}
				else{
					tip_Event_Buffer* next_buffer = event_buffer->next_buffer;
					free(event_buffer->data);
					free(event_buffer);
					thread_state->first_event_buffer = next_buffer;
					event_buffer = next_buffer;
				}
			}
			else{
				event_buffer = event_buffer->next_buffer;
			}
		}

		snapshot.events.push_back(thread_events);
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
					if(last_event_on_stack.type == tip_Event_Type::start && last_event_on_stack.name_id == event.name_id){
						event_stack.pop_back();

						if(first)
							first = false;
						else
							fprintf(file, ",\n");

						const char* name = snapshot.names.get_string(event.name_id);
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
				const char* name = snapshot.names.get_string(event.name_id);
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
			const char* name = snapshot.names.get_string(event.name_id);
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