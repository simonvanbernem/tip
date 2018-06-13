//flags you can set:
// #define TIP_USE_RDTSC
// #define TIP_WINDOWS
// #define TIP_DISABLED

#ifndef TIP_HEADER
#define TIP_HEADER


#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef tip_event_buffer_size
#define tip_event_buffer_size 1024 * 1024
#endif

template<typename T>
struct tip_Dynamic_Array{
	T* buffer = nullptr;
	uint64_t size = 0;
	uint64_t capacity = 0;

	void init(uint64_t initial_capacity){
		buffer = (T*) malloc(sizeof(T) * initial_capacity);
		capacity = initial_capacity;
		size = 0;
	}

	void insert(T element, uint64_t count = 1){
		if(buffer == nullptr)
			init(count * 2);

		uint64_t new_size = size + count;

		if(new_size > capacity){
			capacity = new_size * 2;
			buffer = (T*) realloc(buffer, capacity * sizeof(T));
		}

		for(uint64_t i = size; i < new_size; i++){
			buffer[i] = element;
		}

		size = new_size;
	}

	void insert(T* elements, uint64_t number_of_elements){
		if(buffer == nullptr)
			init(number_of_elements * 2);

		uint64_t new_size = size + number_of_elements;

		if(new_size > capacity){
			capacity = new_size * 2;
			buffer = (T*) realloc(buffer, capacity * sizeof(T));
		}

		memcpy(buffer + size, elements, number_of_elements);

		size = new_size;
	}

	T& operator[](uint64_t index){
		return buffer[index];
	}

	void delete_last(){
		if(size > 0)
			size--;
	}

	T* begin(){
		return buffer;
	}

	T* end(){
		return buffer + size;
	}

	void clear(){
		size = 0;
	}

	void destroy(){
		free(buffer);
		buffer = nullptr;
		size = 0;
		capacity = 0;
	}
};

struct tip_String_Interning_Hash_Table{
	tip_Dynamic_Array<char> name_buffer;
	tip_Dynamic_Array<int64_t> name_indices;
	uint64_t count = 0;


	void init(uint64_t size){
		name_buffer.init(size * 16); //just a random guess that on average a name will have 15 characters (+0 terminator)
		name_indices.init(size);
		name_indices.insert(-1, size);
	}

	uint32_t fvn_hash(const char* string, uint64_t length)
	{
		uint32_t hash = 2166136261; // offset basis (32 bits)
		for (uint64_t i = 0; i < length; i++){
			hash ^= string[i];
			hash *= 16777619;
		}
		return hash;
	}

	void resize_table(){
		uint64_t new_size = name_indices.size * 2;
		tip_Dynamic_Array<int64_t> new_name_indices;
		new_name_indices.init(new_size);
		new_name_indices.insert(-1, new_size);

		for(int i = 0; i < name_indices.size; i++){
			if(name_indices[i] == -1)
				return;

			char* string = name_buffer.buffer + name_indices[i];
			uint64_t string_length = strlen(string);
			uint64_t hash_index = fvn_hash(string, string_length) % new_name_indices.size;

			while(new_name_indices[hash_index] != -1) //linear probing, we know that this is a collision since every string is unique in the hashmap
					hash_index = (hash_index + 1) % name_indices.size;

			new_name_indices[hash_index] = name_indices[i];
		}

		name_indices.destroy();
		name_indices = new_name_indices;
	}

	uint64_t intern_string(char* string){
		if(name_buffer.buffer == nullptr)
			init(64);

		uint64_t string_length = strlen(string);
		uint64_t hash_index = fvn_hash(string, string_length) % name_indices.size;


		while(name_indices[hash_index] != -1){ //linear probing
			char* found_string = name_buffer.buffer + name_indices[hash_index];
			bool equal = strcmp(string, found_string) == 0;

			if(equal)
				return name_indices[hash_index];
			else
				hash_index = (hash_index + 1) % name_indices.size;
		}

		uint64_t interned_string_id = name_buffer.size;
		name_indices[hash_index] = interned_string_id;
		name_buffer.insert(string, string_length + 1);
		count++;

		if(float(count) > 2.f / 3.f * float(name_indices.size))
			resize_table();

		return interned_string_id;
	}

	char* get_string(uint64_t id){
		return name_buffer.buffer + id;
	}

	void destroy(){
		name_buffer.destroy();
		name_indices.destroy();
	}
};


enum class tip_Event_Type{
	start = 0,
	stop = 1,
	start_async = 2,
	stop_async = 3,
	enum_size = 4
};

struct tip_Event{
	uint64_t timestamp;
	int64_t name_id;
	tip_Event_Type type;
};

struct tip_Snapshot{
	double clocks_per_second;
	uint32_t process_id;
	uint64_t number_of_events;

	tip_String_Interning_Hash_Table names;
	tip_Dynamic_Array<uint32_t> thread_ids;
	tip_Dynamic_Array<tip_Dynamic_Array<tip_Event>> events; // the inner array contains the events of one thread.
};

/*
	tip_global_init will initialize the global profiler state. It needs to be called only once, before any other calls to the tip API (this includes the profiling macros). If you are using rdtsc, this function will block for ca. 10ms to determine the speed of the rdtsc clock.

	the return value is the resolution of the clock used in cycles per second. Note that the practical granularity of the clock may be much lower
*/
double tip_global_init(); //call this once globally in program, before you interact with tip IN ANY OTHER WAY


/*
	tip_thread_init will initialize the thread-local profiler state. It needs to be called once in thread, before beginning profiling on that thread
*/
void tip_thread_init(); //call this once on each thread you want to profile on, before you start profiling on that thread and after calling tip_global_init

/*
	tip_create_snapshot will create a copy of the internal profiler state, containing every profiling event of every thread up to that point. profiling will not be interrupted by calling this function, however any calls to tip_thread_init will block until it has finished. If erase_snapshot_data_from_internal_state is set to true, any data contained in this snapshot will be erased from the internal state. That means that any consequent calls to tip_create_shnapshot will not contain that data.

	You can pass the returned snapshot to tip_export_snapshot_to_chrom_json to create a file that can be read by the built-in chrome profiling frontend, or alternatively use this information to export to your own format. For details on the snapshot, see tip_Snapshot.
*/ 
tip_Snapshot tip_create_snapshot(bool erase_snapshot_data_from_internal_state = false);

/*
	tip_export_snapshot_to_chrome_json will export a snapshot to a file, that can be read by the built-in chrome profiling frontend. You can find the frontend at the url "chrome://tracing" in chrome. 

	the function returns the size of the created file
*/
int64_t tip_export_snapshot_to_chrome_json(tip_Snapshot snapshot, char* file_name);


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

#endif //END HEADER





#ifdef TIP_IMPLEMENTATION


#ifdef TIP_WINDOWS
#include "Windows.h"

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
uint64_t tip_get_timestamp(){
#ifdef TIP_USE_RDTSC
	return __rdtsc();
#else
	LARGE_INTEGER temp;
	QueryPerformanceCounter(&temp);
	return temp.QuadPart;
#endif
}

int32_t tip_get_thread_id(){
	return GetCurrentThreadId();
}

int32_t tip_get_process_id(){
	return GetCurrentProcessId();
}

uint64_t tip_get_reliable_timestamp(){
	LARGE_INTEGER temp;
	QueryPerformanceCounter(&temp);
	return temp.QuadPart;
}

uint64_t tip_get_reliable_timestamp_frequency(){
	LARGE_INTEGER temp;
	QueryPerformanceFrequency(&temp);
	return temp.QuadPart;
}

#else //END WINDOWS IMPLEMENTATION

#include <mutex>
#include <chrono>
#include <thread>
#include <map>


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

uint64_t tip_get_timestamp(){
	auto timepoint = std::chrono::high_resolution_clock::now().time_since_epoch();
	auto timepoint_as_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(timepoint);
	return timepoint_as_duration.count();
}

int32_t tip_get_thread_id(){
	static std::mutex vector_mutex;
	static int32_t current_thread_id = 0;
	static std::map<std::thread::id, int32_t> thread_id_map;

	std::thread::id thread_id = std::this_thread::get_id();
	int32_t thread_id_s32 = 0; 

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

int32_t tip_get_process_id(){
	return 0;
}

uint64_t tip_get_reliable_timestamp(){
	return tip_get_timestamp();
}

uint64_t tip_get_reliable_timestamp_frequency(){
	return 1000000000;
}

#endif

#include <stdio.h>

struct tip_Event_Buffer{
	uint8_t* data;
	uint8_t* end;
	uint8_t* current_position;
	uint8_t* position_of_first_event; //this is used to delete events from a buffer when making a snapshot
	tip_Event_Buffer* next_buffer;
};

struct tip_Thread_State{
	bool initialized = false;
	int32_t thread_id;

	tip_Event_Buffer* first_event_buffer;
	tip_Event_Buffer* current_event_buffer;
};

struct tip_Global_State{
	bool initialized = false;
	int32_t process_id;
	double clocks_per_second;

	Mutex thread_states_mutex;
	tip_Dynamic_Array<tip_Thread_State*> thread_states;
};

thread_local tip_Thread_State tip_thread_state;
static tip_Global_State tip_global_state;


void tip_get_new_event_buffer(){
	tip_Event_Buffer* new_buffer = (tip_Event_Buffer*) malloc(sizeof(tip_Event_Buffer));
	new_buffer->data = (uint8_t*) malloc(tip_event_buffer_size);
	new_buffer->end = new_buffer->data + tip_event_buffer_size;
	new_buffer->current_position = new_buffer->data;
	new_buffer->position_of_first_event = new_buffer->data;
	new_buffer->next_buffer = nullptr;

	tip_thread_state.current_event_buffer->next_buffer = new_buffer;
	tip_thread_state.current_event_buffer = new_buffer;
}

void tip_save_profile_event(uint64_t timestamp, char* name, tip_Event_Type type){
	uint64_t name_length_including_terminator = strlen(name) + 1;
	uint64_t event_size = sizeof(timestamp) + sizeof(type) + sizeof(name_length_including_terminator) + name_length_including_terminator;
	
	if(event_size + tip_thread_state.current_event_buffer->current_position > tip_thread_state.current_event_buffer->end)
		tip_get_new_event_buffer();

	tip_Event_Buffer* buffer = tip_thread_state.current_event_buffer; 

	uint8_t* data_pointer = buffer->current_position;
	*((uint64_t*)data_pointer) = timestamp;
	data_pointer += sizeof(timestamp);
	*((tip_Event_Type*)data_pointer) = type;
	data_pointer += sizeof(type);
	*((uint64_t*)data_pointer) = name_length_including_terminator;
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

	new_buffer->data =(uint8_t*) malloc(tip_event_buffer_size);
	new_buffer->end = new_buffer->data + tip_event_buffer_size;
	new_buffer->current_position = new_buffer->data;
	new_buffer->position_of_first_event = new_buffer->data;
	new_buffer->next_buffer = nullptr;

	tip_thread_state.current_event_buffer = new_buffer;
	tip_thread_state.first_event_buffer = new_buffer; 

	tip_lock_mutex(tip_global_state.thread_states_mutex);
	tip_global_state.thread_states.insert(&tip_thread_state);
	tip_unlock_mutex(tip_global_state.thread_states_mutex);

	tip_thread_state.initialized = true;
#endif
}

double tip_global_init(){
#ifndef TIP_DISABLED
	if(tip_global_state.initialized)
		return 1. / tip_global_state.clocks_per_second;

#if defined(TIP_USE_RDTSC) && defined(TIP_WINDOWS) 
	uint64_t reliable_start = tip_get_reliable_timestamp();
	uint64_t rdtsc_start = __rdtsc();

	Sleep(10);

	uint64_t reliable_end = tip_get_reliable_timestamp();
	uint64_t rdtsc_end = __rdtsc();

	int64_t rdtsc_diff = rdtsc_end - rdtsc_start;
	int64_t reliable_diff = reliable_end - reliable_start;

	assert(rdtsc_diff > 0 && reliable_diff > 0);

	double time_passed = double(reliable_diff) / double(tip_get_reliable_timestamp_frequency());
	tip_global_state.clocks_per_second = rdtsc_diff / time_passed;

#else
	tip_global_state.clocks_per_second = double(tip_get_reliable_timestamp_frequency());
#endif

	tip_global_state.process_id = tip_get_process_id();
	tip_global_state.initialized = true;
	tip_global_state.thread_states_mutex = tip_create_mutex();
	return 1. / tip_global_state.clocks_per_second;
#else
	return 0;
#endif
}

tip_Snapshot tip_create_snapshot(bool erase_snapshot_data_from_internal_state){
	tip_Snapshot snapshot;
#ifndef TIP_DISABLED
	snapshot.clocks_per_second = tip_global_state.clocks_per_second;
	snapshot.process_id = tip_global_state.process_id;
	snapshot.names.init(256);

	tip_lock_mutex(tip_global_state.thread_states_mutex);

	for(int i = 0; i < tip_global_state.thread_states.size; i++){

		tip_Thread_State* thread_state = tip_global_state.thread_states[i];
		snapshot.thread_ids.insert(thread_state->thread_id);

		tip_Dynamic_Array<tip_Event> thread_events;
		tip_Event_Buffer* event_buffer = thread_state->first_event_buffer;

		while(event_buffer){
			uint8_t* data_pointer = event_buffer->position_of_first_event;

			while(data_pointer != event_buffer->current_position){
				tip_Event event;
				event.timestamp = *((uint64_t*)data_pointer);
				data_pointer += sizeof(event.timestamp);
				event.type = *((tip_Event_Type*)data_pointer);
				data_pointer += sizeof(event.type);
				uint64_t name_length_including_terminator = *((uint64_t*)data_pointer);
				data_pointer += sizeof(name_length_including_terminator);
				event.name_id = snapshot.names.intern_string((char*)data_pointer);
				data_pointer += name_length_including_terminator;
				thread_events.insert(event);
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

		snapshot.events.insert(thread_events);
	}

	tip_unlock_mutex(tip_global_state.thread_states_mutex);
#endif
	return snapshot;
}

int64_t tip_export_snapshot_to_chrome_json(tip_Snapshot snapshot, char* file_name){
#ifndef TIP_DISABLED
	FILE* file = fopen(file_name, "w+");
	fprintf(file, "{\"traceEvents\": [\n");

	bool first = true;

	tip_Dynamic_Array<tip_Event> event_stack;

	for(int thread_index = 0; thread_index < snapshot.thread_ids.size; thread_index++){
		int32_t thread_id = snapshot.thread_ids[thread_index];
		for(tip_Event event : snapshot.events[thread_index]){

			if(event.type == tip_Event_Type::start) //we put this event on the stack, so we can check if we can form duration events using this and its corresponding close event later
				event_stack.insert(event);

			else if(event.type == tip_Event_Type::stop){ //we check if the last thing on the stack is the corresponding start event for this stop event. if so, we merge both into a duration event and print it
				if(event_stack.size == 0)
					event_stack.insert(event);
				else{
					tip_Event last_event_on_stack = event_stack[event_stack.size - 1];
					if(last_event_on_stack.type == tip_Event_Type::start && last_event_on_stack.name_id == event.name_id){
						event_stack.delete_last();

						if(first)
							first = false;
						else
							fprintf(file, ",\n");

						const char* name = snapshot.names.get_string(event.name_id);
						double timestamp = double(last_event_on_stack.timestamp) / snapshot.clocks_per_second * 1000000.;
						double duration = double(event.timestamp - last_event_on_stack.timestamp) / snapshot.clocks_per_second * 1000000.;


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
						event_stack.insert(event);
					}
				}
			}
			else{    //the only type of events left are the asynchronous ones. we just print these directly
				double timestamp = double(event.timestamp) / snapshot.clocks_per_second * 1000000.;
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
			double timestamp = double(event.timestamp) / snapshot.clocks_per_second * 1000000.;
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
	uint64_t size = uint64_t(ftell(file));
	fclose(file);
	return size;
#else
	return -1;
#endif
}

#endif //TIP_IMPLEMENTATION