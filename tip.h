#pragma once
#include <stdint.h>
#include <vector>
#include "Windows.h"
#include <assert.h>
using u64 = uint64_t;
using u32 = uint32_t;
using s64 = int64_t;
using s32 = int32_t;
using f64 = double;

namespace nbui{
	struct Mutex{HANDLE handle;};
	Mutex create_mutex();
	void lock_mutex(Mutex mutex, double timeout_in_seconds = 0);
	void unlock_mutex(Mutex mutex);

	namespace profiler{
		#define PROFILER_ACTIVATED
		#define PROFILER_USE_RDTSC
		#define PROFILE_FUNCTION PROFILE_FUNCTION_VERBOSE
		//#define PROFILE_FRAME_ONLY

#ifdef PROFILER_ACTIVATED
		#define PROFILE_SCOPE(name) nbui::profiler::Scope_Profiler profauto##name (#name, nbui::profiler::length_including_null(#name));

		#define PROFILE_SCOPE_START(name) nbui::profiler::Scope_Profiler profman##name (#name, nbui::profiler::length_including_null(#name));
		#define PROFILE_SCOPE_STOP(name) profman##name .~Scope_Profiler();

		#define PROFILE_START_ASYNC(name) nbui::profiler::start_async(#name, nbui::profiler::length_including_null(#name));
		#define PROFILE_STOP_ASYNC(name) nbui::profiler::stop_async(#name, nbui::profiler::length_including_null(#name));

		#define PROFILE_FUNCTION_VERBOSE nbui::profiler::Scope_Profiler proffunc##__LINE__ (__FUNCSIG__, nbui::profiler::length_including_null(__FUNCSIG__));
		#define PROFILE_FUNCTION_SHORT nbui::profiler::Scope_Profiler proffunc##__LINE__ (__FUNCTION__, nbui::profiler::length_including_null(__FUNCTION__));
#else
		#define PROFILE_SCOPE(name)

		#define PROFILE_SCOPE_START(name)
		#define PROFILE_SCOPE_STOP(name)

		#define PROFILE_START_ASYNC(name)
		#define PROFILE_STOP_ASYNC(name)

		#define PROFILE_FUNCTION_VERBOSE
		#define PROFILE_FUNCTION_SHORT
#endif

		
		void start_async(char* name, int name_size);
		void stop_async(char* name, int name_size);
	
		struct tip_Thread_State


		struct Buffer{
			char* base_address = nullptr;
			char* current_address = nullptr;
			char* max_address = nullptr;
		};

		struct Serialized_Event{
			s64 timestamp;
			s64 duration;
			u64 name_index;
			u64 thread_index;
			char type;
		};
		bool operator==(Serialized_Event &e1, Serialized_Event &e2);
		bool operator!=(Serialized_Event &e1, Serialized_Event &e2);

		struct Serialized_State{
			std::vector<s32> threads;
			std::vector<std::string> names;
			std::vector<Serialized_Event> events;
			s32 process_id;
			f64 clocks_per_second;
		};

		bool operator==(Serialized_State &s1, Serialized_State &s2);

		struct Profiler_Event{
			s64 timestamp;
			s64 duration;
			char* name;
			s32 thread_id;
			char type;
		};

		bool is_event_newer(Profiler_Event l, Profiler_Event r);

		extern std::vector<Buffer*> event_buffer_list;
		extern Mutex event_buffer_list_mutex;

		const s64 name_buffer_size = 1024 * 1024 * sizeof(char);
		const s64 event_buffer_size = 1024 * 1024 * sizeof(Profiler_Event);

		struct Thread_State{
			Buffer name_buffer;
			Buffer* event_buffer;
			s32 thread_id;
			bool initialized = false;
		};

		struct Global_State{
			s32 process_id;
			f64 clocks_per_second;
			bool initialized = false;
		};

		extern thread_local Thread_State thread_state;
		extern Global_State global_state;

		Serialized_State serialize_state();
		u64 output_serialized_state_to_json(Serialized_State serialized_state, char* file_name = "profile.json");


		void print_results(char* file_name = "profiler.json");
		void output_results_own_format(char* file_name = "profiler.prof");
		void thread_init();
		void global_init();
		s64 get_timestamp();
		void set_name_and_copy_to_buffer(Profiler_Event* event, char* name, int name_size);


		int constexpr length_including_null(const char* str)
		{
			return (*str ? 1 + length_including_null(str + 1) : 0) + 1;
		}
		void expand_name_buffer_if_necessairy(int name_size);
		void expand_event_buffer_if_necessairy();

		struct Scope_Profiler{
			bool stopped = false;
			char* name;
			s64 start_timestamp;
			Scope_Profiler(char* name, int name_size);
			void stop();
			~Scope_Profiler();
		};
	}
}


#ifdef TIP_IMPLEMENTATION

#include <set>
#include <algorithm>
#include <unordered_map>
namespace nbui{

	Mutex create_mutex(){
		Mutex mutex;
		mutex.handle = CreateMutex(0, false, 0);
		return mutex;
	}

	void lock_mutex(Mutex mutex, double timeout_in_seconds){
		if (timeout_in_seconds == 0)
			WaitForSingleObject(mutex.handle, INFINITE);
		else
			WaitForSingleObject(mutex.handle, (DWORD)(timeout_in_seconds * 1000.));

	}

	void unlock_mutex(Mutex mutex){
		ReleaseMutex(mutex.handle);
	}

	std::vector<profiler::Buffer*> profiler::event_buffer_list;
	Mutex profiler::event_buffer_list_mutex = create_mutex();
	thread_local profiler::Thread_State profiler::thread_state;
	profiler::Global_State profiler::global_state;
	u64 profiler::time_diff_size_sum = 0;
	u64 profiler::duration_size_sum = 0;
	u64 profiler::duration_count = 0;
	u64 profiler::event_size_sum = 0;
	u64 profiler::event_count = 0;
	u64 profiler::duration_size_counts[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	u64 profiler::time_diff_size_counts[8] = {0, 0, 0, 0, 0, 0, 0, 0};

	bool profiler::is_event_newer(Profiler_Event l, Profiler_Event r){
		return l.timestamp < r.timestamp;
	}

	bool profiler::operator==(Serialized_State &s1, Serialized_State &s2){
		if(s1.process_id != s2.process_id || s1.clocks_per_second != s2.clocks_per_second){
			return false;
		}
		if(s1.threads.size() != s2.threads.size() || s1.names.size() != s2.names.size() || s1.events.size() != s2.events.size()){
			return false;
		}

		for(int i = 0; i < s1.threads.size(); i++){
			if(s1.threads[i] != s2.threads[i]){
				return false;
			}
		}
		for(int i = 0; i < s1.names.size(); i++){
			if(s1.names[i] != s2.names[i]){
				return false;
			}
		}
		for(int i = 0; i < s1.events.size(); i++){
			if(s1.events[i] != s2.events[i]){
				return false;
			}
		}
		return true;
	}

	bool profiler::operator==(Serialized_Event &e1, Serialized_Event &e2){
		return 
		e1.timestamp == e2.timestamp &&
		e1.duration == e2.duration &&
		e1.name_index == e2.name_index &&
		e1.thread_index == e2.thread_index &&
		e1.type == e2.type;
	}
	bool profiler::operator!=(Serialized_Event &e1, Serialized_Event &e2){
		return !(e1 == e2);
	}

	profiler::Serialized_State profiler::serialize_state(){
		printf("serializing profiler state...");
		PROFILE_SCOPE(serialize_state);
		u64 t1 = get_timestamp();

		Serialized_State serialized_state;

		serialized_state.process_id = global_state.process_id;
		serialized_state.clocks_per_second = global_state.clocks_per_second;

		std::set<std::string> names_set;
		std::set<s32> threads_set;
		std::vector<Profiler_Event> events;

		PROFILE_SCOPE_START(initial_read);
		lock_mutex(event_buffer_list_mutex);
		for(int i = 0; i < event_buffer_list.size(); i++){
			Buffer* buffer = event_buffer_list[i];
			for(char* address = buffer->base_address; address < buffer->current_address; address += sizeof(Profiler_Event)){
				Profiler_Event* event = reinterpret_cast<Profiler_Event*>(address);
				events.push_back(*event);
				names_set.insert(event->name);
				threads_set.insert(event->thread_id);
			}
		}
		unlock_mutex(event_buffer_list_mutex);
		PROFILE_SCOPE_STOP(initial_read);

		PROFILE_SCOPE_START(sort);
		std::sort(events.begin(), events.end(), profiler::is_event_newer);
		PROFILE_SCOPE_STOP(sort);

		PROFILE_SCOPE_START(set_to_vector);
		std::unordered_map<std::string, u64> name_index_lookup_table;

		for(std::string name : names_set){
			name_index_lookup_table[name] = serialized_state.names.size();
			serialized_state.names.push_back(name);
		}
		for(s32 thread : threads_set)
			serialized_state.threads.push_back(thread);

		PROFILE_SCOPE_STOP(set_to_vector);

		PROFILE_SCOPE_START(resolve_names_and_threads);
		for(int i = 0; i < events.size(); i++){
			Serialized_Event serialized_event;
			serialized_event.timestamp = events[i].timestamp;
			serialized_event.duration = events[i].duration;
			serialized_event.type = events[i].type;

			serialized_event.name_index = name_index_lookup_table[events[i].name];

			for(int j = 0; j < serialized_state.threads.size(); j++){
				if(serialized_state.threads[j] == events[i].thread_id){
					serialized_event.thread_index = j;
					break;
				}
			}

			serialized_state.events.push_back(serialized_event);
		}
		PROFILE_SCOPE_STOP(resolve_names_and_threads);
		u64 t2 = get_timestamp();
		printf("finished! (%.1fs)\n", f64(t2 - t1) / global_state.clocks_per_second);
		return serialized_state;
	}

	u64 profiler::output_serialized_state_to_json(Serialized_State serialized_state, char* file_name){
		printf("generating json file from profiler state...");
		PROFILE_SCOPE(output_serialized_state_to_json);
		u64 t1 = get_timestamp();

		FILE* file = fopen(file_name, "w+");
		fprintf(file, "{\"traceEvents\": [\n");
		bool first = true;

		for(auto event : serialized_state.events){
			if(first)
				first = false;
			else
				fprintf(file, ",\n");

			f64 start_time = (f64(event.timestamp) / serialized_state.clocks_per_second) * 1000000.;

			fprintf(file,"  {\"name\":\"%s\","
							"\"cat\":\"PERF\","
							"\"ph\":\"%c\","
							"\"pid\":%d,"
							"\"tid\":%d,"
							"\"id\":100,"
							"\"ts\":%f", serialized_state.names[event.name_index].c_str(), event.type, serialized_state.process_id, serialized_state.threads[event.thread_index], start_time);

			if(event.type == 'X'){
				f64 duration = (f64(event.duration) / serialized_state.clocks_per_second) * 1000000.;
				fprintf(file, ",\"dur\":%f", duration);
			}
			fprintf(file, "}");

		}

		fprintf(file, "\n],\n\"displayTimeUnit\": \"ns\"\n}");
		u64 t2 = get_timestamp();
		printf("finished! (%.1fs, %.2f MiB)\n", f64(t2 - t1) / global_state.clocks_per_second, f64(ftell(file) / f64(1024 * 1024)));
		u64 size = u64(ftell(file));
		fclose(file);
		return size;
	}

	void profiler::thread_init(){
#ifdef PROFILER_ACTIVATED
		assert(global_state.initialized);

		if(thread_state.initialized)
			return;

		Buffer* buffer = new Buffer();

		thread_state.name_buffer.base_address = (char*) malloc(name_buffer_size);
		thread_state.name_buffer.current_address = thread_state.name_buffer.base_address;
		thread_state.name_buffer.max_address = thread_state.name_buffer.base_address + name_buffer_size;

		buffer->base_address = (char*) malloc(event_buffer_size);
		buffer->current_address = buffer->base_address;
		buffer->max_address = buffer->base_address + event_buffer_size;

		lock_mutex(event_buffer_list_mutex);
		event_buffer_list.push_back(buffer);
		unlock_mutex(event_buffer_list_mutex);

		thread_state.thread_id = GetCurrentThreadId();
		thread_state.event_buffer = buffer;

		thread_state.initialized = true;
#endif
	}

	void profiler::global_init(){
#ifdef PROFILER_ACTIVATED
		if(global_state.initialized)
			return;
		global_state.process_id = GetCurrentProcessId();
#ifdef PROFILER_USE_RDTSC
		LARGE_INTEGER temp;
		QueryPerformanceFrequency(&temp);
		f64 qpc_clocks_per_second = f64(temp.QuadPart);
		
		QueryPerformanceCounter(&temp);
		s64 qpc_start = temp.QuadPart;
		u64 rdtsc_start = __rdtsc();

		Sleep(10);

		QueryPerformanceCounter(&temp);
		s64 qpc_diff = temp.QuadPart - qpc_start;
		u64 rdtsc_diff = __rdtsc() - rdtsc_start;

		f64 time_passed = f64(qpc_diff) / qpc_clocks_per_second;
		global_state.clocks_per_second = rdtsc_diff / time_passed;

		printf("Profiler uses rdtsc with a resolution of %.2f ns (MAY CAUSE BUGS/BE UNRELIABLE ON OLDER PROCESSORS (ballpark of sandybridge))\n", 1000000000. / global_state.clocks_per_second);
#else
		LARGE_INTEGER temp;
		QueryPerformanceFrequency(&temp);
		global_state.clocks_per_second = f64(temp.QuadPart);
		printf("Profiler uses qpc with a resolution of %.2f ns (slower than using rdtsc with '#define PROFILER_USE_RDTSC', but works on any processor)\n", 1000000000. / global_state.clocks_per_second);
#endif
		global_state.initialized = true;
#endif
	}

	void profiler::expand_name_buffer_if_necessairy(int name_size){
		if(thread_state.name_buffer.current_address + name_size > thread_state.name_buffer.max_address){
			thread_state.name_buffer.base_address = (char*) malloc(name_buffer_size);
			thread_state.name_buffer.current_address = thread_state.name_buffer.base_address;
			thread_state.name_buffer.max_address = thread_state.name_buffer.base_address + name_buffer_size;
		}
	}

	void profiler::expand_event_buffer_if_necessairy(){
		if(thread_state.event_buffer->current_address + sizeof(Profiler_Event) > thread_state.event_buffer->max_address){
			Buffer* buffer = new Buffer();

			buffer->base_address = (char*) malloc(event_buffer_size);
			buffer->current_address = buffer->base_address;
			buffer->max_address = buffer->base_address + event_buffer_size;

			lock_mutex(event_buffer_list_mutex);
			event_buffer_list.push_back(buffer);
			unlock_mutex(event_buffer_list_mutex);
			thread_state.event_buffer = buffer;
		}
	}

	s64 profiler::get_timestamp(){
#ifdef PROFILER_USE_RDTSC
		return s64(__rdtsc());
#else
		LARGE_INTEGER temp;
		QueryPerformanceCounter(&temp);
		return temp.QuadPart;
#endif
	}

	void profiler::set_name_and_copy_to_buffer(Profiler_Event* event, char* name, int name_size){
		memcpy(thread_state.name_buffer.current_address, name, name_size);
		event->name = thread_state.name_buffer.current_address;
		thread_state.name_buffer.current_address += name_size;
	}

	void profiler::start_async(char* name, int name_size){
		assert(thread_state.initialized);

		profiler::expand_name_buffer_if_necessairy(name_size);
		profiler::expand_event_buffer_if_necessairy();

		Profiler_Event* event = (Profiler_Event*) thread_state.event_buffer->current_address;

		set_name_and_copy_to_buffer(event, name, name_size);

		name = event->name;

		event->thread_id = thread_state.thread_id;
		event->type = 'b';
		event->timestamp = get_timestamp();

		thread_state.event_buffer->current_address += sizeof(Profiler_Event); //putting this at the end makes it threadsafe
	}

	void profiler::stop_async(char* name, int name_size){
		assert(thread_state.initialized);

		profiler::expand_name_buffer_if_necessairy(name_size);
		profiler::expand_event_buffer_if_necessairy();

		Profiler_Event* event = (Profiler_Event*) thread_state.event_buffer->current_address;

		set_name_and_copy_to_buffer(event, name, name_size);

		event->name = name;

		event->thread_id = thread_state.thread_id;
		event->type = 'e';
		event->timestamp = get_timestamp();

		thread_state.event_buffer->current_address += sizeof(Profiler_Event); //putting this at the end makes it threadsafe
	}

	profiler::Scope_Profiler::Scope_Profiler(char* name, int name_size){
		this->start_timestamp = get_timestamp();

		assert(thread_state.initialized);
		profiler::expand_name_buffer_if_necessairy(name_size);

		memcpy(thread_state.name_buffer.current_address, name, name_size);
		this->name = thread_state.name_buffer.current_address;
		thread_state.name_buffer.current_address += name_size;
	}

	void profiler::Scope_Profiler::stop(){
		assert(thread_state.initialized);
		if(this->stopped)
			return;
		profiler::expand_event_buffer_if_necessairy();

		Profiler_Event* event = (Profiler_Event*) thread_state.event_buffer->current_address;

		event->name = this->name;
		event->type = 'X';
		event->timestamp = this->start_timestamp;
		event->duration = get_timestamp() - this->start_timestamp;
		event->thread_id = thread_state.thread_id;

		thread_state.event_buffer->current_address += sizeof(Profiler_Event); //putting this at the end makes printing threadsafe
		this->stopped = true;
	}
	profiler::Scope_Profiler::~Scope_Profiler(){
		this->stop();
	}

}


#ifdef TIP_IMPLEMENTATION