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

		memcpy(buffer + size, elements, number_of_elements * sizeof(T));

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

template<typename T>
bool operator==(tip_Dynamic_Array<T>& lhs, tip_Dynamic_Array<T>& rhs){
	if(lhs.size != rhs.size)
		return false;

	for(uint64_t i = 0; i < lhs.size; i++){
		if(lhs[i] == rhs[i])
			continue;
		return false;
	}

	return true;
}

struct tip_String_Interning_Hash_Table{
	tip_Dynamic_Array<char> name_buffer;
	tip_Dynamic_Array<int64_t> name_indices;
	uint64_t count = 0;


	void init(uint64_t size){
		name_buffer.init(size * 16); //just a random guess that on average a name will have 15 characters (+0 terminator)
		name_indices.init(size);
		name_indices.insert(-1, size);
	}

	static uint32_t fvn_hash(const char* string, uint64_t length)
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

bool operator==(tip_String_Interning_Hash_Table& lhs, tip_String_Interning_Hash_Table& rhs);


enum class tip_Event_Type{
	start = 0,
	stop = 1,
	start_async = 2,
	stop_async = 3,
	enum_size = 4
};

struct tip_Event{
	uint64_t timestamp;
	int64_t name_id; //I think this cant be unsigned, because of the string interning hashtable, I don't know though, might be wrong on this.
	tip_Event_Type type;
};

bool operator==(tip_Event& lhs, tip_Event& rhs);

struct tip_Snapshot{
	double clocks_per_second;
	uint32_t process_id;
	uint64_t number_of_events;

	tip_String_Interning_Hash_Table names;
	tip_Dynamic_Array<uint32_t> thread_ids;
	tip_Dynamic_Array<tip_Dynamic_Array<tip_Event>> events; // the inner array contains the events of one thread.
};

bool operator==(tip_Snapshot& lhs, tip_Snapshot& rhs);

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

static const char* tip_compressed_binary_text_header = "This is the compressed binary format v2 of tip (tiny instrumented profiler).\nYou can read it into a snapshot using the \"tip_export_snapshot_to_compressed_binary\" function in tip.\n";
static const uint64_t tip_compressed_binary_version = 2;



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


#endif //END HEADER





#ifdef TIP_IMPLEMENTATION

bool operator==(tip_Event& lhs, tip_Event& rhs){
	bool a = (lhs.timestamp == rhs.timestamp) && (lhs.name_id == rhs.name_id) && (lhs.type == rhs.type);
	if (a)
		return true;
	else
		return false;
}

bool operator==(tip_String_Interning_Hash_Table& lhs, tip_String_Interning_Hash_Table& rhs){
	bool a = (lhs.count == rhs.count) && (lhs.name_buffer == rhs.name_buffer) && (lhs.name_indices == rhs.name_indices);
	if (a)
		return true;
	else 
		return false;
}

bool operator==(tip_Snapshot& lhs, tip_Snapshot& rhs){
	bool a =  (lhs.clocks_per_second == rhs.clocks_per_second) && (lhs.process_id == rhs.process_id) && (lhs.number_of_events == rhs.number_of_events) && (lhs.names == rhs.names) && (lhs.thread_ids == rhs.thread_ids) && (lhs.events == rhs.events);
	if (a)
		return true;
	else
		return false;
}

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






#if defined(TIP_FILE_FORMAT_COMPRESSED_BINARY_V3) && ! defined(TIP_FILE_FORMAT_COMPRESSED_BINARY_V3_HEADER)
#define TIP_FILE_FORMAT_COMPRESSED_BINARY_V3_HEADER
#include <vector>
#include <algorithm>
namespace tip_file_format_compressed_binary_v3{
	void export_snapshot(char* file_name, tip_Snapshot snapshot);
	tip_Snapshot import_snapshot(char* file_name);



	uint64_t write_bits_into_buffer(char* buffer, uint64_t write_position_in_bits, const void* data, uint64_t number_of_bits_to_write);

	template<typename T>
	uint64_t serialize_value(char* buffer, uint64_t write_position_in_bits, T value){
		return write_bits_into_buffer(buffer, write_position_in_bits, &value, sizeof(T));
	}

	template<typename T>
	void serialize_value_byte_aligned(char** buffer, T value){
		*(T*)(*buffer) = value;
		*buffer += sizeof(T);
	}

	template<typename T>
	void deserialize_value_byte_aligned(char** buffer, T* value){
		*value = *(T*)(*buffer);
		*buffer += sizeof(T);
	}


	void serialize_range_byte_aligned(char** buffer, void* range, uint64_t range_size);
	void serialize_zeros_byte_aligned(char** buffer, uint64_t count);
	void serialize_range_bit_aligned(char* buffer, uint64_t& write_position_in_bits, void* range, uint64_t bits_to_write);

	void deserialize_range_byte_aligned(char** buffer, void* range, uint64_t range_size);
	void deserialize_range_bit_aligned(char* buffer, uint64_t& write_position_in_bits, void* range, uint64_t bits_to_range);

	template<typename T>
	void deserialize_dynamic_array_byte_aligned(char** buffer, tip_Dynamic_Array<T>* array){
		uint64_t size;
		deserialize_value_byte_aligned(buffer, &size);
		array->init(size);
		array->size = size;
		memcpy(array->buffer, *buffer, size * sizeof(T));
		*buffer += sizeof(T) * size;
	}

	template<typename T>
	void serialize_dynamic_array_byte_aligned(char** buffer, tip_Dynamic_Array<T> array){
		serialize_value_byte_aligned(buffer, array.size);
		memcpy(*buffer, array.buffer, array.size * sizeof(T));
		*buffer += sizeof(T) * array.size;
	}
}

#endif

#if defined(TIP_FILE_FORMAT_COMPRESSED_BINARY_V3) && defined(TIP_IMPLEMENTATION)

namespace tip_file_format_compressed_binary_v3{
	void serialize_range_byte_aligned(char** buffer, void* range, uint64_t range_size){
		memcpy(*buffer, range, range_size);
		*buffer += range_size;
	}

	void serialize_zeros_byte_aligned(char** buffer, uint64_t count){
		for(uint64_t i = 0; i < count; i++)
			(*buffer)[i] = 0;
		*buffer += count;
	}

	void deserialize_range_byte_aligned(char** buffer, void* range, uint64_t range_size){
		memcpy(range, *buffer, range_size);
		*buffer += range_size;
	}

	uint64_t get_number_bits_needed_to_represent_number(uint64_t number) {
		uint64_t number_of_bits = 1;
		while (number >= 1llu << number_of_bits)
			number_of_bits++;
		return number_of_bits;

	}

	

	//this writes number_of_bits_to_write bits from data to buffer. The write is offset by write_position_in_bits bits
	//the return value is the first bit in buffer after the written data
	void serialize_range_bit_aligned_bit_length(char* buffer, uint64_t* write_position_in_bits, void* range, uint64_t bits_to_write){
		uint64_t write_position_bytes_part = *write_position_in_bits / 8;
		uint64_t write_position_bits_part = *write_position_in_bits % 8;

		uint64_t read_position_in_bytes = 0;
		uint64_t read_position_in_bits = 0;

		while(read_position_in_bits + read_position_in_bytes * 8 < bits_to_write){
			auto is_bit_set = (1 << read_position_in_bits) & ((char*)range)[read_position_in_bytes];
			//we also handle unintialized memory here (so we actually overwrite with 0, and don't just expect the memory to already be 0)
			if(is_bit_set)
				buffer[write_position_bytes_part] |= (1 << write_position_bits_part);
			else
				buffer[write_position_bytes_part] &= ~(1 << write_position_bits_part);

			read_position_in_bits = (read_position_in_bits + 1) % 8;
			if(read_position_in_bits == 0)
				read_position_in_bytes++;

			write_position_bits_part = (write_position_bits_part + 1) % 8;
			if(write_position_bits_part == 0)
				write_position_bytes_part++;
		}

		*write_position_in_bits += bits_to_write;
	}

	void deserialize_range_bit_aligned_bit_length(char* buffer, uint64_t* read_position_in_bits, void* data, uint64_t bits_to_read){
		uint64_t read_position_bytes_part = *read_position_in_bits / 8;
		uint64_t read_position_bits_part = *read_position_in_bits % 8;

		uint64_t write_position_in_bytes = 0;
		uint64_t write_position_in_bits = 0;

		while(write_position_in_bits + write_position_in_bytes * 8 < bits_to_read){
			auto is_bit_set = (1 << read_position_bits_part) & buffer[read_position_bytes_part];
			//we also handle unintialized memory here (so we actually overwrite with 0, and don't just expect the memory to already be 0)
			if (is_bit_set)
				((char*)data)[write_position_in_bytes] |= (1 << write_position_in_bits);
			else
				((char*)data)[write_position_in_bytes] &= ~(1 << write_position_in_bits);

			read_position_bits_part = (read_position_bits_part + 1) % 8;
			if(read_position_bits_part == 0)
				read_position_bytes_part++;

			write_position_in_bits = (write_position_in_bits + 1) % 8;
			if(write_position_in_bits == 0)
				write_position_in_bytes++;
		}

		*read_position_in_bits += bits_to_read;
	}



	//usage:
	//call setup once before you use the encoder
	//call count_value on every byte of the data you want to encode
	//call create_encoder_data
	//you can now call encode_value to get the code and length of that code in bits for each byte of data you want to encode
	//the decoder needs the huffman table, to be able to decode the data. So you need to store it along with your encoded data:
	//call get_conservative_size_estimate_when_serialized to get a conservativethe size you need to reserve for the serialized table itsself
	//call serialize_table to serialize the table
	struct Huffman_Encoder{
		struct Node { 
			//this is what you are here for: the code is in
			bool is_right_child = false;
			unsigned occurences;
			unsigned depth = 0;
			Node *left = nullptr, *right = nullptr, *previous = nullptr; 
		};

		Node nodes[256];

		void setup(){
			for(int i = 0; i < 256; i++){
				nodes[i] = {};
			}
		}

		//https://stackoverflow.com/questions/759707/efficient-way-of-storing-huffman-tree


		void count_value(char* memory){
			nodes[*reinterpret_cast<unsigned char*>(memory)].occurences++;
		}

		struct Heap_Comparison_Struct{ 
			bool operator()(const Node* lhs, const Node* rhs) const{ 
				return lhs->occurences > rhs->occurences; 
			}
		};


		std::vector<Node*> internal_nodes;
		Node* root_node = nullptr;

		unsigned assign_depths(unsigned depth, Node* node){
			node->depth = depth;
			if(!node->left || !node->right)
				return depth;

			unsigned left_tree_depth = assign_depths(depth + 1, node->left);
			unsigned right_tree_depth = assign_depths(depth + 1, node->right);
			
			return (left_tree_depth > right_tree_depth) ? left_tree_depth : right_tree_depth;
		}

		void create_encoder_data(){
			std::vector<Node*> heap;
			for(int i = 0; i < 256; i++){
				heap.push_back(nodes + i);
			}
			std::make_heap(heap.begin(), heap.end(), Heap_Comparison_Struct());

			//we get rid of all nodes for values that didn't appear in the data to compress. That way the serialized tree will be smaller
			while(true){
				std::pop_heap(heap.begin(), heap.end(), Heap_Comparison_Struct());
				
				auto smallest = heap.back();
				if(smallest->occurences != 0)
					break;

				heap.pop_back();
			}

			assert(heap.size() > 1);

			while(!heap.empty()){
				std::pop_heap(heap.begin(), heap.end(), Heap_Comparison_Struct());
				auto smallest = heap.back();
				heap.pop_back();

				std::pop_heap(heap.begin(), heap.end(), Heap_Comparison_Struct());
				auto second_smallest = heap.back();
				heap.pop_back();

				Node* new_internal_node = new Node();
				new_internal_node->occurences = smallest->occurences + second_smallest->occurences;
				
				new_internal_node->left = smallest;
				smallest->previous = new_internal_node;
				smallest->is_right_child = false;

				new_internal_node->right = second_smallest;
				second_smallest->previous = new_internal_node;
				second_smallest->is_right_child = true;

				root_node = new_internal_node;
				internal_nodes.push_back(new_internal_node);
				
				if (heap.empty())
					break;

				heap.push_back(new_internal_node);
				std::push_heap(heap.begin(), heap.end(), Heap_Comparison_Struct());
			}

			printf("max code length is %d", int(assign_depths(0, root_node)));
		}

		uint64_t serialize_encoded_value(char* buffer, uint64_t write_position_in_bits, char* value){
			Node* node = &nodes[*reinterpret_cast<unsigned char*>(value)];
			unsigned total_code_length = node->depth;

			while(node->depth > 0){
				write_bits_into_buffer(buffer, write_position_in_bits + node->depth - 1, &node->is_right_child, 1);
				node = node->previous;
			}

			return write_position_in_bits + total_code_length;
		}

		uint64_t get_serialized_size_for_subtree(Node* node){
			if(!(node->left || node->right))
				return 1 + 8; //1 bit for saving that this is a leaf node, 8 bit for storing the original data value
			else
				return 1 + get_serialized_size_for_subtree(node->left) + get_serialized_size_for_subtree(node->right);  
		}

		uint64_t get_table_size_when_serialized(){
			return get_serialized_size_for_subtree(root_node) + sizeof(uint64_t);
		}

		uint64_t serialize_subtree(char* buffer, uint64_t write_position_in_bits, Node* node){
			if(node->left && node->right){
				bool value_to_write = 0;
				write_position_in_bits = write_bits_into_buffer(buffer, write_position_in_bits, &value_to_write, 1);
				write_position_in_bits = serialize_subtree(buffer, write_position_in_bits, node->left);
				return serialize_subtree(buffer, write_position_in_bits, node->left);
			}

			bool value_to_write = 1;
			write_position_in_bits = write_bits_into_buffer(buffer, write_position_in_bits, &value_to_write, 1);
			unsigned char value = (unsigned char)(node - nodes); //leaf nodes are in the node array. their index is the value of the data byte they encode. So we just need to subtract their address from the base address of the array to get their index and in turn their value
			return write_bits_into_buffer(buffer, write_position_in_bits, &value, sizeof(value));
		}

		uint64_t serialize_table(char* buffer, uint64_t write_position_in_bits){
			uint64_t table_size = get_table_size_when_serialized();
			write_position_in_bits = write_bits_into_buffer(buffer, write_position_in_bits, &table_size, sizeof(table_size));
			write_position_in_bits = serialize_subtree(buffer, write_position_in_bits, root_node);
		}

		void destroy(){
			for(auto internal_node : internal_nodes){
				delete internal_node;
			}
		}

	};

	//this is just so you know what buffer size you need when serializing a snapshot. This caculates the size of a serialization that just literaly serialized all the information in the snapshot without doing anything to them + some more to be sure. This is meant to always overestimate, never underestimate.
	uint64_t get_conservative_size_estimate_for_serialized_snapshot(tip_Snapshot snapshot){
		return snapshot.thread_ids.size * sizeof(uint32_t) // all the factors that can scale
		     + snapshot.names.name_buffer.size * sizeof(char)
		     + snapshot.names.name_indices.size * sizeof(uint64_t)
		     + snapshot.number_of_events * sizeof(tip_Event)
		     + sizeof(tip_Snapshot) + 2048; // accounting for constant sizes. The 2048 is just there so we can be absolutely sure we never underestimate
	}

	//THIS IS OLD, DONT USE IT!!
	//this is a short description of the format tip compressed binary version 3 (tcb3):
	//this exporter does not account for endianness, so if you are on x86 multi-byte values are probably stored little endian
	//100 bytes: null terminated ascii text header + padding, so you know what this file is if you open it in a text editor
	//8 bytes / 64-bit unsigned integer: format version number (this must always be 3 for this format)
	//8 bytes / 64-bit double precision floating point: clocks_per_second
	//4 bytes / 32-bit unsigned integer: process_id
	//8 bytes / 64-bit unsigned integer: total number of events
	//8 bytes / 64-bit unsigned integer: size of the name buffer in bytes
	//name_buffer_size: name_buffer, contains the names of all events packed as utf8. Every name in this namebuffer is terminated with a 0 byte
	//8 bytes / 64-bit unsigned integer: number of threads recorded
	//4 bytes * number of threads: id_buffer contains an ID for each thread as 4 byte / 32-bit unsigned integer
	//8 bytes / 64-bit unsigned integer: contains the number of names in the name buffer
	//variable: offset_buffer contains an offset for each name as 8 byte / 64-bit unsigned integer
	//    If an event says it has name #X, you will need to reference offset #X in this buffer. 
	//    This offset then points to the starting character of that name in the name buffer.
	//    For example: event has name #0, offset #0 is 15, the name starts with the 16th (counted from 1) byte of the name_buffer
	//for each thread:
	//    8 bytes / 64-bit unsigned integer: number of events in that thread
	//    for each event in that thread:
	//        3-bit unsigned integer: an index in the length table.
	//            The corresponding value in the length table is the size of the following timestamp-diff in bit
	//            The length table: [7-bit, 8-bit, 9-bit, 10-bit, 12-bit, 16-bit, 22-bit, 64-bit]
	//            For example: if the 3-bit index is 2, the following timestamp-diff will be 9 bits long
	//        per value variable: the difference, in clock cycles to the previous event in this thread as an unsigned integer
	//            The size of this value is given with the previous index.
	//            The first event does not give this value in realtion to a previous event, but as an absolute.
	//            For example: if this is 926, this event has occured 926 clocks cycles after the previous event
	//                         if the event is the first event, the event has occured at 926 clock cycles
	//        per file variable: the name index as an unsigned integer
	//            The size of this value is the size of bits needed to represent the highest possible index, which in turn is the number of names - 1
	//                if this is a synchronous stop event, this value is ommited, since it has to be the same name, of the last synchronous start event on this thread (otherwise the snapshot was malformed)
	//            For example: if there are 256 names, this value is 8 bits long, because 8 bits is the smallest number of bits needed to represent 265 values. if there are 257 names, this value is 9 bits long.
	//                if this value is 20, the offset at which to find the first character of this name is the 21st (counted from 1) in the offset_buffer

	tip_Dynamic_Array<uint64_t> get_default_diff_sizes(){
		uint64_t diff_sizes_buffer[] = {7, 8, 9, 10, 12, 16, 22, 64};
		uint64_t number_of_diff_sizes = sizeof(diff_sizes_buffer) / sizeof(uint64_t);

		tip_Dynamic_Array<uint64_t> default_diff_sizes;
		default_diff_sizes.init(number_of_diff_sizes);
		default_diff_sizes.insert(diff_sizes_buffer, number_of_diff_sizes);
		return default_diff_sizes;
	}

	void write_entire_file(char* file_name, tip_Dynamic_Array<char> file_buffer){
		FILE* file = nullptr;
		fopen_s(&file, file_name, "wb");
		fwrite(file_buffer.buffer, file_buffer.size, 1, file);
		fclose(file);
	}

	uint64_t export_snaphsot(char* file_name, tip_Snapshot snapshot, tip_Dynamic_Array<uint64_t>* diff_sizes = nullptr){
		tip_Dynamic_Array<uint64_t> default_diff_sizes; //this variable is here to go out of scope because we use a pointer to that
		
		((void)file_name);
		if(!diff_sizes){
			default_diff_sizes = get_default_diff_sizes();
			diff_sizes = &default_diff_sizes;
		}

		uint64_t buffer_size = get_conservative_size_estimate_for_serialized_snapshot(snapshot);
		printf("conservative size estimate is %lluB\n", buffer_size);
		char* buffer = (char*)malloc(buffer_size);
		char* initial_buffer_position = buffer;

		{//text header, padding and version
			char* text_header = "This is the tcb3 file format! (tip compressed binary format version 3)\n";
			uint64_t text_header_size = tip_strlen(text_header);
			assert(text_header_size < 99);
			uint64_t text_header_padding_size = 100 - text_header_size;
			uint64_t version = 3;

			serialize_range_byte_aligned(&buffer, text_header, text_header_size);
			serialize_zeros_byte_aligned(&buffer, text_header_padding_size);
			serialize_value_byte_aligned(&buffer, version);
		}

		serialize_value_byte_aligned(&buffer, snapshot.clocks_per_second); 
		serialize_value_byte_aligned(&buffer, snapshot.process_id); 
		serialize_value_byte_aligned(&buffer, snapshot.number_of_events);
		serialize_dynamic_array_byte_aligned(&buffer, snapshot.names.name_buffer);
		serialize_dynamic_array_byte_aligned(&buffer, snapshot.thread_ids);
		

		tip_Dynamic_Array<uint64_t> name_index_encoding_table;
		tip_Dynamic_Array<uint64_t> name_index_decoding_table;

		{//populating the tables above
			name_index_encoding_table.insert(uint64_t(0), snapshot.names.name_buffer.size);
			name_index_decoding_table.init(snapshot.names.count);
			
			auto& name_indices = snapshot.names.name_indices;

			for(uint64_t i = 0; i < name_indices.size; i++){
				if(name_indices[i] != -1){
					name_index_encoding_table[name_indices[i]] = name_index_decoding_table.size;
					name_index_decoding_table.insert(name_indices[i]);
				}
			}
		}

		serialize_dynamic_array_byte_aligned(&buffer, name_index_decoding_table);

		uint64_t substitue_name_index_size = get_number_bits_needed_to_represent_number(name_index_decoding_table.size);
		uint64_t event_type_size = get_number_bits_needed_to_represent_number(uint64_t(tip_Event_Type::enum_size) - 1);
		uint64_t diff_size_index_size = get_number_bits_needed_to_represent_number(diff_sizes->size - 1);


		serialize_value_byte_aligned(&buffer, substitue_name_index_size);
		serialize_value_byte_aligned(&buffer, event_type_size);
		serialize_dynamic_array_byte_aligned(&buffer, *diff_sizes);
		serialize_value_byte_aligned(&buffer, diff_size_index_size);
		serialize_value_byte_aligned(&buffer, snapshot.names.name_indices.size);


		printf("event type size is %llu bit\n", event_type_size);
		printf("substitue name index size is %llu bit\n", substitue_name_index_size);
		printf("diff size index size is %llu bit\n", diff_size_index_size);

		uint64_t total_bytes_written = buffer - initial_buffer_position;
		{
			uint64_t write_position_in_bits = 0;

			auto& events = snapshot.events;

			for(uint64_t i = 0; i < events.size; i++){
				serialize_range_bit_aligned_bit_length(buffer, &write_position_in_bits, &events[i].size, sizeof(events[i].size) * 8);

				if(events[i].size == 0){
					continue;
				}

				uint64_t prev_timestamp = 0;

				for(uint64_t j = 0; j < events[i].size; j++){
					tip_Event event = events[i][j];

					uint64_t diff = event.timestamp - prev_timestamp;
					uint64_t min_needed_diff_size = get_number_bits_needed_to_represent_number(diff);
					uint64_t selected_diff_size_index = 0;

					for(auto diff_size : *diff_sizes){
						if(diff_size >= min_needed_diff_size)
							break;
						else
							selected_diff_size_index++;
					}

					serialize_range_bit_aligned_bit_length(buffer, &write_position_in_bits, &selected_diff_size_index, diff_size_index_size);
					serialize_range_bit_aligned_bit_length(buffer, &write_position_in_bits, &diff, (*diff_sizes)[selected_diff_size_index]);
					prev_timestamp = event.timestamp;

					serialize_range_bit_aligned_bit_length(buffer, &write_position_in_bits, &event.type, event_type_size);
					if(event.type != tip_Event_Type::stop) //names of stop events have to be the same as the last start event on the stack
						serialize_range_bit_aligned_bit_length(buffer, &write_position_in_bits, &name_index_encoding_table[event.name_id], substitue_name_index_size);
				}
			}

			total_bytes_written += (write_position_in_bits + 7) / 8; //the + 7 effectively rounds up in the integer division
		}

		assert(total_bytes_written <= buffer_size);

		diff_sizes->destroy();
		name_index_encoding_table.destroy();
		name_index_decoding_table.destroy();

		{
			tip_Dynamic_Array<char> file_buffer;
			file_buffer.buffer = initial_buffer_position;
			file_buffer.size = total_bytes_written;
			file_buffer.capacity = buffer_size;
			write_entire_file(file_name, file_buffer);
		}
		
		free(initial_buffer_position);
		return total_bytes_written;
	}

	tip_Dynamic_Array<char> read_entire_file(char* file_name){
		tip_Dynamic_Array<char> file_buffer;
		FILE* file;

		fopen_s(&file, file_name, "rb");
		fseek(file, 0, SEEK_END);
		uint64_t file_size = ftell(file); 
		rewind(file);

		file_buffer.buffer = (char*)malloc(file_size);
		file_buffer.size = file_size;
		file_buffer.capacity = file_size;

		fread(file_buffer.buffer, file_size, 1, file);
		fclose(file);

		return file_buffer;
	}


	tip_Snapshot import_snaphsot(char* file_name){
		tip_Snapshot snapshot;
		tip_Dynamic_Array<char> file_buffer = read_entire_file(file_name);
		char* buffer = file_buffer.buffer;

		{//text header, padding and version
		/*
			char* text_header = "This is the tcb3 file format! (tip compressed binary format version 3)\n";
			uint64_t text_header_size = tip_strlen(text_header);
			assert(text_header_size < 99);
			uint64_t text_header_padding_size = 100 - text_header_size;
			uint64_t version = 3;

			serialize_range_byte_aligned(&buffer, text_header, text_header_size);
			serialize_zeros_byte_aligned(&buffer, text_header_padding_size);
		*/
			buffer += 100;
			uint64_t file_version;
			serialize_value_byte_aligned(&buffer, &file_version);
		}

		tip_Dynamic_Array<uint64_t> name_index_decoding_table;
		uint64_t substitue_name_index_size;
		uint64_t event_type_size;
		uint64_t diff_size_index_size;
		tip_Dynamic_Array<uint64_t> diff_sizes;

		{
			deserialize_value_byte_aligned(&buffer, &snapshot.clocks_per_second); 
			deserialize_value_byte_aligned(&buffer, &snapshot.process_id); 
			deserialize_value_byte_aligned(&buffer, &snapshot.number_of_events); 
			deserialize_dynamic_array_byte_aligned(&buffer, &snapshot.names.name_buffer);
			deserialize_dynamic_array_byte_aligned(&buffer, &snapshot.thread_ids);
			deserialize_dynamic_array_byte_aligned(&buffer, &name_index_decoding_table);
			deserialize_value_byte_aligned(&buffer, &substitue_name_index_size);
			deserialize_value_byte_aligned(&buffer, &event_type_size);
			deserialize_dynamic_array_byte_aligned(&buffer, &diff_sizes);
			deserialize_value_byte_aligned(&buffer, &diff_size_index_size);
		}

		{//restoring the snapshot.names.name_indices array. This is not needed if you only want to read the snapshot (which is the most likely use case), but it would leave the interning table in an inconsistent state. And it only costs 8 bytes in a multi-kb file, so we might aswell do it
			uint64_t name_indices_array_size;
			deserialize_value_byte_aligned(&buffer, &name_indices_array_size);

			snapshot.names.name_indices.insert(-1, name_indices_array_size);
			snapshot.names.count = name_index_decoding_table.size;

			char* name = snapshot.names.name_buffer.buffer;
			while(name < snapshot.names.name_buffer.buffer + snapshot.names.name_buffer.size){
				uint64_t name_length = tip_strlen(name);
				uint64_t name_indices_position = tip_String_Interning_Hash_Table::fvn_hash(name, name_length) % snapshot.names.name_indices.size;
				snapshot.names.name_indices[name_indices_position] = name - snapshot.names.name_buffer.buffer;
				name += name_length + 1;
			}
		}

		printf("event type size is %llu bit\n", event_type_size);
		printf("substitue name index size is %llu bit\n", substitue_name_index_size);
		printf("diff size index size is %llu bit\n", diff_size_index_size);

		uint64_t total_bytes_written = buffer - file_buffer.buffer;

		{
			uint64_t write_position_in_bits = 0;

			auto number_of_threads = snapshot.thread_ids.size;

			for(uint64_t i = 0; i < number_of_threads; i++){ //for each thread
				uint64_t number_of_events;
				deserialize_range_bit_aligned_bit_length(buffer, &write_position_in_bits, &number_of_events, sizeof(number_of_events) * 8);

				snapshot.events.insert({});

				if(number_of_events == 0){
					continue;
				}

				tip_Dynamic_Array<uint64_t> last_start_event_name_index_stack = {};

				uint64_t prev_timestamp = 0;
				for(uint64_t j = 0; j < number_of_events; j++){ // for each event in a thread
					tip_Event event = {};

					uint64_t diff_size_index = 0;
					deserialize_range_bit_aligned_bit_length(buffer, &write_position_in_bits, &diff_size_index, diff_size_index_size);
					
					uint64_t diff_size = diff_sizes[diff_size_index];
					uint64_t diff = 0;
					deserialize_range_bit_aligned_bit_length(buffer, &write_position_in_bits, &diff, diff_size);

					event.timestamp = prev_timestamp + diff;
					prev_timestamp = event.timestamp;

					deserialize_range_bit_aligned_bit_length(buffer, &write_position_in_bits, &event.type, event_type_size);

					if(event.type == tip_Event_Type::stop){ //has to have the same name as the last start event on the stack
						assert(last_start_event_name_index_stack.size > 0);
						event.name_id = last_start_event_name_index_stack[last_start_event_name_index_stack.size - 1];
						last_start_event_name_index_stack.delete_last();
					}
					else{
						uint64_t substitute_index = 0;
						deserialize_range_bit_aligned_bit_length(buffer, &write_position_in_bits, &substitute_index, substitue_name_index_size);
						event.name_id = name_index_decoding_table[substitute_index];
					}
					
					if(event.type == tip_Event_Type::start){ //put name on the stack for matching stop event
						last_start_event_name_index_stack.insert(event.name_id);
					}

					snapshot.events[i].insert(event);
				}

				last_start_event_name_index_stack.destroy();
			}

			total_bytes_written += (write_position_in_bits + 7) / 8; //the + 7 effectively rounds up in the integer division
		}
		
		assert(total_bytes_written == file_buffer.size);
		file_buffer.destroy();
		return snapshot;
	}

}



#endif