//flags you can set:
// #define TIP_USE_RDTSC
// #define TIP_WINDOWS

// if you do not define TIP_WINDOWS, TIP will use the C++ <chrono> header for timing, which (apparently) pulls in some code that needs exceptions enabled to compile. So you may get the "enable -EHsc" error when you have them turned off!

// use
// #define TIP_IMPLEMENTATION
// to generate the implementation.


// before you can profile:
// call tip_global_init() once for your entire program, before you call anything else in tip
// call tip_thread_init() after tip_global_init() in each thread you want to profile before you call anything else in tip

// profiling commands:
// TIP_PROFILE_SCOPE(name)
// TIP_PROFILE_START(name)
// TIP_PROFILE_STOP(name)
// TIP_PROFILE_ASYNC_START(name)
// TIP_PROFILE_ASYNC_STOP(name)
// where name is a string in quotes, e.g.: TIP_PROFILE_SCOPE("setup")

// to export the profiling data in a format that can be openend in "chrome://tracing/" in your chrome browser, call:
// tip_export_snapshot_to_chrome_json(tip_create_snapshot(), "profiling_run.snapshot");
// tip is modular, which means you can add your own exporters. tip_create_snapshot() accumulates all the internal state into a package that you can convert to your own format. for more information see the tip_Snapshot struct.
// tip_export_snapshot_to_chrome_json is such a converter, which converts the snapshot to a json file, readable by "chrome://tracing/" in a chrome browser. Feel free to take a look.



//single thread simple example:

// #include <stdio.h>
// void main(){
//     tip_global_init();
//     tip_thread_init();
// 
//     TIP_PROFILE_START("for loop");
//     for(int i = 0; i < 1000; i++){
//         TIP_PROFILE_SCOPE("iteration");
//         printf("doing my work #%d", i);
//     }
//     TIP_PROFILE_STOP("for loop");
// 
//     tip_export_snapshot_to_chrome_json(tip_create_snapshot(), "profiling_run.snapshot");
// }


/*
#include <stdio.h>

void new_thread_main(int thread_index){
	tip_thread_init();
	char buffer[20];
	printf("thread %d started", thread_index);

	for(int job_index = 0; job_index < 6; job_index++){
		sprintf(buffer, "job #%d", thread_index);
		TIP_PROFILE_START(buffer); //we can't use TIP_PROFILE_SCOPE here, since it would be executed before the sprintf

		printf("doing job %d", job_index);

		TIP_PROFILE_STOP(buffer);
	}

	sprintf(buffer, "thread #%d", thread_index);
	TIP_PROFILE_ASYNC_STOP(buffer);
}

void main(){
	for(int i = 0; i < 10; i++){
		
	}

}

*/
#ifndef TIP_HEADER
#define TIP_HEADER


#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#ifndef tip_event_buffer_size
#define tip_event_buffer_size 1024 * 1024
#endif

uint32_t tip_strlen(const char* string);
bool tip_string_is_equal(char* string1, char* string2);

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

		for(uint64_t i = 0; i < name_indices.size; i++){
			if(name_indices[i] == -1)
				return;

			char* string = name_buffer.buffer + name_indices[i];
			uint64_t string_length = tip_strlen(string);
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

		uint64_t string_length = tip_strlen(string);
		uint64_t hash_index = fvn_hash(string, string_length) % name_indices.size;


		while(name_indices[hash_index] != -1){ //linear probing
			char* found_string = name_buffer.buffer + name_indices[hash_index];
			bool equal = tip_string_is_equal(string, found_string);

			if(equal)
				return uint64_t(name_indices[hash_index]);
			else
				hash_index = (hash_index + 1) % name_indices.size;
		}

		uint64_t interned_string_id = name_buffer.size;
		name_indices[hash_index] = int64_t(interned_string_id);
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

void tip_free_snapshot(tip_Snapshot snapshot);

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

static const char* tip_uncompressed_binary_text_header = "This is the uncompressed binary format v1 of tip (tiny instrumented profiler).\nYou can read it into a snapshot using the \"tip_export_snapshot_to_uncompressed_binary\" function in tip.\n";
static const uint64_t tip_uncompressed_binary_version = 1;

int64_t tip_export_snapshot_to_uncompressed_binary(tip_Snapshot snapshot, char* file_name);
tip_Snapshot tip_import_uncompressed_binary_to_snapshot(char* file_name);



	#define TIP_CONCAT_LINE_NUMBER(x, y) x ## y // and you also need this somehow. c++ is stupid
	#define TIP_CONCAT_LINE_NUMBER2(x, y) TIP_CONCAT_LINE_NUMBER(x, y) // this just concats "profauto" and the line number
	#define TIP_PROFILE_SCOPE(name) tip_Scope_Profiler TIP_CONCAT_LINE_NUMBER2(profauto, __LINE__)(name); //for id=0 and and line 23, this expands to "tip_Scope_Profiler profauto23(0);"

	#define TIP_PROFILE_START(name) tip_save_profile_event(tip_get_timestamp(), name, tip_Event_Type::start);
	#define TIP_PROFILE_STOP(name) tip_save_profile_event(tip_get_timestamp(), name, tip_Event_Type::stop);

	#define TIP_PROFILE_ASYNC_START(name) tip_save_profile_event(tip_get_timestamp(), name, tip_Event_Type::start_async);
	#define TIP_PROFILE_ASYNC_STOP(name) tip_save_profile_event(tip_get_timestamp(), name, tip_Event_Type::stop_async);

void tip_save_profile_event(uint64_t timestamp, const char* name, tip_Event_Type type);
uint64_t tip_get_timestamp();

struct tip_Scope_Profiler{
	const char* name;

	tip_Scope_Profiler(const char* event_name){
		tip_save_profile_event(tip_get_timestamp(), event_name, tip_Event_Type::start);
		name = event_name;
	}

	~tip_Scope_Profiler(){
		tip_save_profile_event(tip_get_timestamp(), name, tip_Event_Type::stop);
	}
};

template<typename T>
char* tip_serialize_value(char* buffer, T value){
	*((T*)buffer) = value;
	return buffer + sizeof(T);
}

template<typename T>
char* tip_unserialize_value(char* buffer, T* value){
	*value = *((T*)buffer);
	return buffer + sizeof(T);
}

template<typename T>
uint64_t tip_get_serialized_value_size(T value){
	((void)value);
	return sizeof(T);
}

template<typename T>
char* tip_unserialize_dynamic_array(char* buffer, tip_Dynamic_Array<T>* array){
	uint64_t size;
	buffer = tip_unserialize_value(buffer, &size);
	array->init(size);
	array->size = size;
	memcpy(array->buffer, buffer, size * sizeof(T));
	return buffer + sizeof(T) * size;
}

template<typename T>
char* tip_serialize_dynamic_array(char* buffer, tip_Dynamic_Array<T> array){
	buffer = tip_serialize_value(buffer, array.size);
	memcpy(buffer, array.buffer, array.size * sizeof(T));
	return buffer + sizeof(T) * array.size;
}

template<typename T>
uint64_t tip_get_serialized_dynamic_array_size(tip_Dynamic_Array<T> array){
	return sizeof(array.size) + array.size * sizeof(T);
}

#endif //END HEADER





#ifdef TIP_IMPLEMENTATION

uint32_t tip_strlen(const char* string){
	uint32_t length = 0;
	while(string[length++]){
	}
	return length - 1;
}

bool tip_string_is_equal(char* string1, char* string2){
	uint32_t index = 0;
	while(string1[index] == string2[index]){
		if(string1[index])
			index++;
		else
			return true;
	}
	return false;
}


#ifdef TIP_WINDOWS
#define NOMINMAX
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

void tip_save_profile_event(uint64_t timestamp, const char* name, tip_Event_Type type){
	assert(tip_thread_state.initialized);
	
	uint64_t name_length_including_terminator = tip_strlen(name) + 1;
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

void tip_thread_init(){
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
}

double tip_global_init(){
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
}

tip_Snapshot tip_create_snapshot(bool erase_snapshot_data_from_internal_state){
	tip_Snapshot snapshot;
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
				snapshot.number_of_events++;
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
	return snapshot;
}


void tip_escape_string_for_json(const char* string, tip_Dynamic_Array<char>* array){
	array->clear();
	for(int i = 0; string[i]; i++){
		switch(string[i]){
			case '\\':
				array->insert('\\');
				array->insert('\\');
				break;
			case '\n':
				array->insert('\\');
				array->insert('n');
				break;
			case '\t':
				array->insert('\\');
				array->insert('t');
				break;
			case '\"':
				array->insert('\\');
				array->insert('\"');
				break;
			case '\r':
				array->insert('\\');
				array->insert('\r');
				break;
			case '\b':
				array->insert('\\');
				array->insert('\b');
				break;
			case '\f':
				array->insert('\\');
				array->insert('\f');
				break;
			default:
				array->insert(string[i]);
		}
	}
	array->insert('\0');
}

int64_t tip_export_snapshot_to_chrome_json(tip_Snapshot snapshot, char* file_name){
	FILE* file = nullptr;
	fopen_s(&file, file_name, "w+");
	fprintf(file, "{\"traceEvents\": [\n");

	bool first = true;

	tip_Dynamic_Array<tip_Event> event_stack;
	tip_Dynamic_Array<char> escaped_name_buffer;

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

						tip_escape_string_for_json(name, &escaped_name_buffer);
						
						fprintf(file,"  {\"name\":\"%s\","
										"\"cat\":\"PERF\","
										"\"ph\":\"X\","
										"\"pid\":%d,"
										"\"tid\":%d,"
										"\"id\":100,"
										"\"ts\":%.16e,"
										"\"dur\":%.16e}", escaped_name_buffer.buffer, snapshot.process_id, thread_id, timestamp, duration);
					}
					else{
						event_stack.insert(event);
					}
				}
			}
			else{    //the only type of events left are the asynchronous ones. we just print these directly
				double timestamp = double(event.timestamp) / snapshot.clocks_per_second * 1000000.;
				const char* name = snapshot.names.get_string(event.name_id);
				tip_escape_string_for_json(name, &escaped_name_buffer);

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
								"\"ts\":%.16e}", escaped_name_buffer.buffer, type, snapshot.process_id, thread_id, timestamp);
			}
		}

		for(tip_Event event : event_stack){ //print all start and stop events, that don't have a corresponding event they could form a duration event with
			double timestamp = double(event.timestamp) / snapshot.clocks_per_second * 1000000.;
			const char* name = snapshot.names.get_string(event.name_id);
			tip_escape_string_for_json(name, &escaped_name_buffer);
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
							"\"ts\":%.16e}", escaped_name_buffer.buffer, type, snapshot.process_id, thread_id, timestamp);
		}

		event_stack.clear();
	}

	escaped_name_buffer.destroy();
	fprintf(file, "\n],\n\"displayTimeUnit\": \"ns\"\n}");
	uint64_t size = uint64_t(ftell(file));
	fclose(file);
	return size;
}

int64_t tip_export_snapshot_to_uncompressed_binary(tip_Snapshot snapshot, char* file_name){
	uint64_t file_size = 0;

	file_size += 200; //this is for the text header
	file_size += tip_get_serialized_value_size(tip_uncompressed_binary_version);

	file_size += tip_get_serialized_value_size(snapshot.clocks_per_second); 
	file_size += tip_get_serialized_value_size(snapshot.process_id); 
	file_size += tip_get_serialized_value_size(snapshot.number_of_events);

	file_size += tip_get_serialized_dynamic_array_size(snapshot.names.name_buffer);
	file_size += tip_get_serialized_dynamic_array_size(snapshot.names.name_indices); 
	file_size += tip_get_serialized_value_size(snapshot.names.count); 

	file_size += tip_get_serialized_dynamic_array_size(snapshot.thread_ids); 

	for(int i = 0; i < snapshot.events.size; i++){
		file_size += tip_get_serialized_dynamic_array_size(snapshot.events[i]); 
	}


	char* initial_buffer_position = (char*) malloc(file_size);
	char* buffer = initial_buffer_position;

	uint64_t text_header_size = tip_strlen(tip_uncompressed_binary_text_header);
	memcpy(buffer, tip_uncompressed_binary_text_header, text_header_size);
	buffer += text_header_size;

	memset(buffer, 0, 200 - text_header_size);
	buffer += 200 - text_header_size;

	buffer = tip_serialize_value(buffer, tip_uncompressed_binary_version);

	buffer = tip_serialize_value(buffer, snapshot.clocks_per_second); 
	buffer = tip_serialize_value(buffer, snapshot.process_id); 
	buffer = tip_serialize_value(buffer, snapshot.number_of_events); 

	buffer = tip_serialize_dynamic_array(buffer, snapshot.names.name_buffer);
	buffer = tip_serialize_dynamic_array(buffer, snapshot.names.name_indices); 
	buffer = tip_serialize_value(buffer, snapshot.names.count); 

	buffer = tip_serialize_dynamic_array(buffer, snapshot.thread_ids); 

	for(int i = 0; i < snapshot.thread_ids.size; i++){
		buffer = tip_serialize_dynamic_array(buffer, snapshot.events[i]); 
	}


	assert(snapshot.thread_ids.size == snapshot.events.size);
	assert(buffer == initial_buffer_position + file_size);


	FILE* file = nullptr;
	fopen_s(&file, file_name, "wb");
	fwrite(initial_buffer_position, file_size, 1, file);
	fclose(file);
	free(initial_buffer_position);

	return file_size;
}

tip_Snapshot tip_import_uncompressed_binary_to_snapshot(char* file_name){
	tip_Snapshot snapshot;

	char* initial_buffer_position;
	uint64_t file_size;

	{
		FILE* file;

		fopen_s(&file, file_name, "rb");
		fseek(file, 0, SEEK_END);
		file_size = ftell(file); 
		rewind(file);

		initial_buffer_position = (char*)malloc(file_size);
		fread(initial_buffer_position, file_size, 1, file);
		fclose(file);
	}

	char* buffer = initial_buffer_position;

	buffer += 200; //200 is the max size of the text header

	uint64_t version;
	buffer = tip_unserialize_value(buffer, &version);

	assert(version == tip_uncompressed_binary_version);

	buffer = tip_unserialize_value(buffer, &snapshot.clocks_per_second); 
	buffer = tip_unserialize_value(buffer, &snapshot.process_id); 
	buffer = tip_unserialize_value(buffer, &snapshot.number_of_events); 

	buffer = tip_unserialize_dynamic_array(buffer, &snapshot.names.name_buffer);
	buffer = tip_unserialize_dynamic_array(buffer, &snapshot.names.name_indices); 
	buffer = tip_unserialize_value(buffer, &snapshot.names.count); 

	buffer = tip_unserialize_dynamic_array(buffer, &snapshot.thread_ids); 

	for(int i = 0; i < snapshot.thread_ids.size; i++){
		snapshot.events.insert({});
		buffer = tip_unserialize_dynamic_array(buffer, &snapshot.events[i]); 
	}

	assert(snapshot.thread_ids.size == snapshot.events.size);
	assert(buffer == initial_buffer_position + file_size);
	free(initial_buffer_position);

	return snapshot;
}


void tip_free_snapshot(tip_Snapshot snapshot){
	snapshot.names.name_buffer.destroy();
	snapshot.names.name_indices.destroy();
	snapshot.thread_ids.destroy();
	for(int i = 0; i < snapshot.events.size; i++){
		snapshot.events[i].destroy();
	}
	snapshot.events.destroy();
}

#endif //TIP_IMPLEMENTATION