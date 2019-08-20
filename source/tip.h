//Tiny Instrumented Profiler (TIP) - v0.9 - MIT licensed
//authored from 2018-2019 by Simon van Bernem
// 
// This library gathers information about the execution time of a programm and
// exports it into an intermediary format and/or a JSON format that can be used
// with the chrome profiler frontend (URL "chrome://tracing" in any chrome browser).
// 
// 
// QUICKSTART: If you just want to get started and don't care about anything
//             else, search for @EXAMPLES and look at the first one.
// 
// 
// LICENSE: TIP is licensed under the MIT License. The full license agreement
//          can be found at the end of this file.
// 
// 
// STRUCTURE: This file contains several sections:
//    @EXAMPLES contains example programms that use various parts of TIP.
//    If you just want to get it up and running, look here!
// 
//    @API DOCUMENTATION is where you can find the full API documentation.
// This is a tiny instrumenting profiler (TIP). You can use this library to
// measure how long certain parts of your code took to execute, by using
// provided profiling macros. You can profile scopes, functions, set manual
// zones and record asynchronous zones. At any time, you can query the gathered
// profiling-data into a binary snapshot, which can be exported to a file
// readable by the chrome profiling frontend (URL chrome://tracing in any chrome
// browser), or you can write a custom export function for it.
// 
// TIP is shipped as a single-header library: The header and implementation are
// both contained in this file

// This file contains:

//
//

// USAGE
//
// Include this file in whatever places need to refer to it. In ONE C/C++ file, write:
// #define STB_TRUETYPE_IMPLEMENTATION
// before the #include of this file. This expands out the actual implementation into that C/C++ file.
//
//



//------------------------------------------------------------------------------
//----------------------------------@EXAMPLES-----------------------------------
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Example 1: Minimal example that records one profiling zone and saves it to a
// file. This file can then be viewed with the chrome profiling frontend
#if 0

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
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Example 2: Using all available profiling macros with a complex control flow
// to generate some actually interesting profiling data. You might want to
// execute this example and open the json file in the chrome frontend, to better
// understand the control flow and the purpose of each profiling macro.
#if 0

#define TIP_AUTO_INIT
#define TIP_IMPLEMENTATION
#include "tip.h"

void burn_cpu(int index){
  TIP_PROFILE_FUNCTION();
  for(int dummy = 0; dummy < index * 1000 + 1000; dummy++){}
}

void do_stuff(int index);

void main(){
  TIP_PROFILE_ASYNC_START("Time until 17"); //opening an async zone that will be closed in do_stuff
  TIP_PROFILE_FUNCTION(); //profile this scope with the name of the function
  TIP_PROFILE_START("manual_main"); //open a manual zone

  for(int i = 0; i < 20; i++){
    TIP_PROFILE_SCOPE("scope1"); //profile this scope
    do_stuff(i);
  }

  TIP_PROFILE_ASYNC_STOP("Time from 5"); //close an async zone that will be started in do_stuff
  TIP_PROFILE_STOP("manual_main"); //close a manual zone

  tip_export_state_to_chrome_json("profiling_data.json");
  //open this file with a chrome browser at the URL chrome://tracing
}

void do_stuff(int index){
  TIP_PROFILE_FUNCTION();

  if(index == 5)
    TIP_PROFILE_ASYNC_START("Time from 5"); //close the async zone that was started in main

  if(index == 17)
    TIP_PROFILE_ASYNC_STOP("Time until 17"); //open an async zone that will be closed in main

  {
    TIP_PROFILE_SCOPE_COND("If even, profile this scope.", index % 2 == 0);
    burn_cpu(index);
  }
  burn_cpu(index);
}
#endif
//------------------------------------------------------------------------------


#ifndef TIP_HEADER
#define TIP_HEADER


//------------------------------------------------------------------------------
//--------------------------@FEATURE TOGGLES------------------------------------
//------------------------------------------------------------------------------

// TIP_AUTO_INIT
// TIP_WINDOWS
// TIP_USE_RDTSC
// TIP_MEMORY_LIMIT
// TIP_GLOBAL_TOGGLE

//------------------------------------------------------------------------------
//------------------------------@DEFINES----------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//Codebase integration: TIP provides the option to replace functionality like asserts and memory allocation.

#ifndef TIP_API
#define TIP_API // Use this to define function prefixes for API functions like dll export/import.
#endif

#ifndef TIP_ASSERT
#include <assert.h>
#define TIP_ASSERT assert
#endif

#ifndef TIP_MALLOC
#define TIP_MALLOC malloc
#endif

#ifndef TIP_REALLOC
#define TIP_REALLOC realloc
#endif

#ifndef TIP_FREE
#define TIP_FREE free
#endif

#ifndef TIP_EVENT_BUFFER_SIZE
#define TIP_EVENT_BUFFER_SIZE 1024 * 1024
#endif

#include <stdint.h>


// used internally
TIP_API uint32_t tip_strlen(const char* string);
TIP_API bool tip_string_is_equal(char* string1, char* string2);

#define TIP_CONCAT_STRINGS_HELPER(x, y) x ## y
#define TIP_CONCAT_STRINGS(x, y) TIP_CONCAT_STRINGS_HELPER(x, y) // this concats x and y. Why you need two macros for this in C++, I do not know.

template<typename T>
T tip_min(T v0, T v1){
  if(v0 < v1)
    return v0;
  else
    return v1;
}

template<typename T>
T tip_max(T v0, T v1){
  if(v0 > v1)
    return v0;
  else
    return v1;
}

template<typename T>
struct tip_Dynamic_Array{
  T* data = nullptr;
  uint64_t size = 0;
  uint64_t capacity = 0;
  uint64_t max_growth_size = 0; //0 means unlimited growth and this array wont count towards the memory limit

  bool init(uint64_t initial_capacity, uint64_t initial_max_growth_size = 0){
    max_growth_size = initial_max_growth_size;
    size = 0;
    return grow_to_exact_capacity(initial_capacity);
  }

  bool reserve(uint64_t new_capacity){
    return grow_to_exact_capacity(new_capacity);
  }


  bool grow_to_exact_capacity(uint64_t new_capacity){
    if(capacity >= new_capacity)
      return true;

#ifdef TIP_MEMORY_LIMIT
    //why this works without taking a critical section is documented in tip_Global_State
    uint64_t growth_size = new_capacity - capacity;

    if (max_growth_size == 0) {
      //max_growth_size of 0 means that we don't have to care about the memory limit and can grow in arbitrarily big steps
      data = (T*) TIP_REALLOC(data, sizeof(T) * new_capacity);
      capacity = new_capacity;
    }
    else {
      while (growth_size > 0) {
        uint64_t growth_size_this_step = tip_min(growth_size, max_growth_size);
        data = (T*)tip_try_realloc_with_respect_to_memory_limit(data, (capacity + growth_size_this_step) * sizeof(T), growth_size_this_step * sizeof(T));

        if (!data)
          return false;

        growth_size -= growth_size_this_step;
        capacity = new_capacity;
      }
    }

#else
    data = (T*) TIP_REALLOC(data, sizeof(T) * new_capacity);
    capacity = new_capacity;
#endif

    return true;
  }

  bool grow_if_necessairy(uint64_t count){
    if(size + count <= capacity)
      return true;

    if(size == 0)
      return grow_to_exact_capacity(tip_max(count * 2, 512llu));

    return grow_to_exact_capacity(tip_max(size * 2, size + count));
  }

  bool insert(T element, uint64_t count = 1){
    if(!grow_if_necessairy(count))
      return false;

    uint64_t new_size = size + count;

    for(uint64_t i = size; i < new_size; i++){
      data[i] = element;
    }

    size = new_size;
    return true;
  }

  tip_Dynamic_Array<T> get_copy(){
    tip_Dynamic_Array<T> copy;
    copy.init(size, max_growth_size);
    copy.insert(data, size);
    return copy;
  }

  bool insert(T* elements, uint64_t number_of_elements){
    if(!grow_if_necessairy(number_of_elements))
      return false;

    uint64_t new_size = size + number_of_elements;

    memcpy(data + size, elements, number_of_elements * sizeof(T));

    size = new_size;
    return true;
  }

  T& operator[](uint64_t index){
    return data[index];
  }

  void clear_last(uint64_t count = 1){
    if(size >= count)
      size -= count;
  }

  void ordered_remove(uint64_t index, uint64_t count = 1){
    if(count == 0)
      return;

    assert((index + count <= size) && "You cannot remove past the end of the array!");

    if(index != size - count)
      memcpy(data + index, data + index + count, ((size - count) - index) * sizeof(T));

    size -= count;
  }

  T pop_last(){
    TIP_ASSERT(size > 0);
    size--;
    return data[size];
  }

  T* begin(){
    return data;
  }

  T* end(){
    return data + size;
  }

  void clear(){
    size = 0;
  }

  void destroy(){
    TIP_FREE(data);
    data = nullptr;
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

      char* string = name_buffer.data + name_indices[i];
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
    if(name_buffer.data == nullptr)
      init(64);

    uint64_t string_length = tip_strlen(string);
    uint64_t hash_index = fvn_hash(string, string_length) % name_indices.size;


    while(name_indices[hash_index] != -1){ //linear probing
      char* found_string = name_buffer.data + name_indices[hash_index];
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
    return name_buffer.data + id;
  }

  void destroy(){
    name_buffer.destroy();
    name_indices.destroy();
  }
};

TIP_API bool operator==(tip_String_Interning_Hash_Table& lhs, tip_String_Interning_Hash_Table& rhs);

enum class tip_Event_Type{
  start = 0,
  stop = 1,
  start_async = 2,
  stop_async = 3,
  event = 4,

  //tip info events
  tip_get_new_buffer_start = 5,
  tip_get_new_buffer_stop = 6,
  tip_recording_halted_because_of_memory_limit_start = 7,
  tip_recording_halted_because_of_memory_limit_stop = 8,
  tip_creating_snapshot_start = 9,
  tip_creating_snapshot_stop = 10,
  enum_size = 11
};

struct tip_Event{
  uint64_t timestamp;
  int64_t name_id = -1; //I think this cant be unsigned, because of the string interning hashtable, I don't know though, might be wrong on this.
  uint64_t categories;
  tip_Event_Type type;
};

TIP_API bool operator==(tip_Event& lhs, tip_Event& rhs);

struct tip_Snapshot{
  double clocks_per_second;
  uint32_t process_id;
  uint64_t number_of_events;

  tip_String_Interning_Hash_Table names;
  tip_Dynamic_Array<uint32_t> thread_ids;
  tip_Dynamic_Array<tip_Dynamic_Array<tip_Event>> events; // the inner array contains the events of one thread.
  
  int64_t category_name_indices[64];
  tip_Dynamic_Array<char> category_name_buffer;

};

//------------------------------------------------------------------------------
//------------------------------@API DOCUMENTATION------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// State initialization: TIP keeps global and thread local state to record
// profiling events. This state needs to be intialized. The usual way this
// happens is by first calling tip_global_init ONCE on application startup,
// and then call tip_thread_init once in each thread that you want to profile.
// 
// If you don't have control over thread-creation or programm startup, or simply
// don't want to be bothered by this, you can define TIP_AUTO_INIT. If you do
// this, initialization will be automatically taken care of. Note that this
// incurs a runtime cost (if-check on recording a profiling event)

TIP_API double tip_global_init();
// Initializes the global state of TIP and returns the resolution of the clock
// as the smallest representable interval in seconds. This function should be
// the first call to the TIP-API that you make. If the global state was already
// initialized, this function does nothing. If TIP_AUTO_INIT is defined, there
// is no need to call this fuction.

TIP_API bool tip_thread_init();
// Initializes the thread-local state of TIP and should be called once, after
// tip_global_init but before attempting to record profiling events in that
// thread. If the thread-local state was already initialized, this
// function does nothing. If TIP_AUTO_INIT is defined, there is no need to
// call this fuction. If TIP_MEMORY_LIMIT is defined and initializing the thread
// state would have violated the limit, the function return false. Otherwise, it
// returns true.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Global toggle: if TIP_GLOBAL_TOGGLE is defined, a bool will be
// added to the global state, that controls whether profiling events are
// recorded. The toggle starts deactivated (false), so no profiling events will
// be recorded until it is activated. This can be usefull if you only want to
// profile parts of your application, or to deactivated profiling before you had
// the chance to initialize TIP's global and thread state. Note that setting and
// querying the global toggle is valid BEFORE calling tip_global_init.

TIP_API void tip_set_global_toggle(bool toggle);
// If TIP_GLOBAL_TOGGLE is defined, this function sets the state
// global toggle. Otherwise, this function does nothing.
// The global state doesn't need to be initialized to set the global toggle.

TIP_API bool tip_set_global_toggle_after_delay(bool toggle, double delay_in_seconds);
// If TIP_GLOBAL_TOGGLE is defined, this function sets the state of the global
// toggle on the first attempt to record a profiling zone, after the delay has
// passed. Otherwise, this function does nothing. The zone that triggers this
// will be evaluated against the new value of the toggle. The global state 
// doesn't need to be initialized to set the global toggle after a delay.

TIP_API bool tip_get_global_toggle();
// If TIP_GLOBAL_TOGGLE is defined, this function returns the
// current state of the global toggle. Otherwise, this function return true.
// The global state doesn't need to be initialized to query the global toggle.

//------------------------------------------------------------------------------
TIP_API void tip_set_memory_limit(uint64_t limit_in_bytes);
// If TIP_MEMORY_LIMIT is defined, this function sets a memory-limit that
// TIP will not exceed. Otherwise this function does nothing. Any profiling
// events that attempt to be recorded, but would exceed this limit will not be
// recorded. A value of 0 means no limit.
// In order to begin recording profiling data again in such a sitation, either
// the memory limit has to be increased, or the internal profiling data has to
// be cleared. This is possible by either calling tip_create_snapshot with the
// argument erase_snapshot_data_from_internal_state set to true, or calling
// tip_clear_internal_profiling_data.
// Note that no data will be deleted, if the limit is set below the current
// memory footprint.

TIP_API uint64_t tip_get_memory_limit();
// If TIP_MEMORY_LIMIT is defined, this function returns the memory limit, that
// TIP will not exceed. Otherwise, this function returns 0. A return value of 0
// means that there is no limit.

TIP_API uint64_t tip_get_current_memory_footprint();
// Returns the current memory footprint of TIP. This includes any allocated or
// global data, but excludes any objects on the stack.
// If TIP_MEMORY_LIMIT is not defined, this function has to take a lock for each
// thread that initialized thread state and may take longer to execute as a result.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Categories: You can group profiling zones into categories and instruct TIP to
// only record profiling zones of some categories, by setting a filter. To
// determine what categories to include, TIP performs a bitwise AND on the
// category filter and the category value. Each power of two/each bit in the
// filter can be used for a seperate category, that can have a name associated
// to it. Bitwise OR-ing different categories can be used to create filters that
// let multiple categories pass, or create events that are associated to
// multiple categories. Categories may also be included when exporting.
//
// TIP uses the highest category (2^64-1 or 1 << 63, global constant:
// tip_info_category) for events that inform about the internals of the profiler.
// These can, for example, be zones that show when TIP allocates a new buffer
// for events, or is blocked by the memory limit. You are free to issue events
// with this category, or to change its name.

TIP_API void tip_set_category_filter(uint64_t bitmask);
// Sets the category filter. Any attempt to record a profiling zone will be
// discarded, if its category does not pass the category filter. A category
// passes, if the value of the category ANDed with the value of the category
// filter is non-zero.

TIP_API void tip_add_category_filter(uint64_t bitmask);
// Sets the given category-bits in the category filter, making events with these
// categories pass the filter.

TIP_API void tip_remove_category_filter(uint64_t bitmask);
// Unsets the given category-bits in the category filter, making events with
// these categories not pass the filter.

TIP_API uint64_t tip_get_category_filter();
// Gets the current category filter.

TIP_API bool tip_does_category_pass_filter(uint64_t category);
// Returns true if the category passes the current category filter,
// returns false otherwise. This is simply a bitwise AND.

TIP_API bool tip_set_category_name(uint64_t category, const char* category_name);
// Sets the name of a certain category. This name may be usefull for a frontend.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Profiling functions: (tip_zone, tip_zone_cond and tip_zone_function are
// actually macros, but to simplify things, I will call them all functions)
// These functions are used to record profiling data into a buffer, which
// can be queried into a snapshot using tip_create_snapshot. Use them to mark
// zones that you want profile.
// If TIP_GLOBAL_TOGGLE is defined and the global toggle is disable, calling
// these functions does nothing.
// If TIP_MEMORY_LIMIT is defined, the memory limit is non-zero and recording
// more profiling data would violate the defined limit, calling these function
// does nothing.
// 
// Every once in a while, a call to these functions will take signifcantly
// longer than usual, because TIP has to allocate a new buffer to store the
// profiling information in. This will distort the accuracy of the data, and you
// may consider increasing TIP_EVENT_BUFFER_SIZE, so fewer, bigger buffers will
// be allocated. TIPs memory footprint and in turn the frequency of buffer
// allocations scales with the number of profiling events and the length of the
// event names.
// 
// Most macros take a name argument. This name is used for display in frontends
// and to match openening and closing profiling sections.

// Manual profilers:

TIP_API void tip_zone_start(const char* name, uint64_t categories);
//Starts a profiling zone with the given name and the given categories. Use
// tip_zone_stop to stop the zone.

TIP_API void tip_zone_stop(uint64_t categories);
//Stops the most recently started profiling zone, that was started by
// tip_zone_start or a tip_zone...-macro.

TIP_API void tip_async_zone_start(const char* name, uint64_t categories);
//Starts an async profiling zone with the given name and the given categories.
// Async zones don't have to adhere to the stack-like nature of normal zones.
// To put it formally: The number of zone starts and stops between the begin and
// end of a an async zone does not have to be equal. You can for example start
// an async zone on one thread and end it on another, or start two different
// async zones 1 and 2 and end zone 1 before 2. This is not possible with normal
// zones.

TIP_API void tip_async_zone_stop(const char* name, uint64_t categories);
//Stops an async profiling zone with the same name, that was started with
// tip_async_zone_start.

//for tip_zone("test", 1), placed on line 201, this will generate: tip_Scope_Profiler profauto201("test", true, 1);
#define tip_zone(/*const char* */ name, /*uint64_t*/ categories)\
  tip_Conditional_Scope_Profiler TIP_CONCAT_STRINGS(profauto, __LINE__)(name, true, categories);

#define tip_zone_cond(/*const char* */ name, /*uint64_t*/ categories, condition)\
  tip_Conditional_Scope_Profiler TIP_CONCAT_STRINGS(profauto, __LINE__)(name, condition, categories);

#define tip_zone_function(/*uint64_t*/ categories)\
  tip_Conditional_Scope_Profiler TIP_CONCAT_STRINGS(profauto, __LINE__)(__FUNCTION__, true, categories);

TIP_API double tip_measure_average_duration_of_recording_a_single_profiling_event(uint64_t sample_size = 100000);
//


// The following macros are available:
// 
// Scoped Profilers:
// 
// TIP_PROFILE_SCOPE(name)
// Opens a profiling section in its constructor and closes it in it's destructor.
// 
// TIP_PROFILE_SCOPE_COND(name, condition)
// Opens a profiling section in its constructor and closes it in it's destructor
// if condition evaluates to true
// 
// TIP_PROFILE_FUNCTION()
// Opens a profiling section in its constructor and closes it in it's destructor.
// Uses the name of the enclosing function.
// 
// 
// Manual profilers:
// 
// TIP_PROFILE_START(name)
// Opens a profiling section-
// Use this in conjunction with TIP_PROFILE_STOP (match the names).
// 
// TIP_PROFILE_STOP(name)
// Closes a profiling section.
// Use this in conjunction with TIP_PROFILE_START (match the names).
// 
// 
// Async profilers:
// 
// TIP_PROFILE_ASYNC_START(name)
// Opens an async profiling section.
// Use this in conjunction with TIP_PROFILE_ASYNC_STOP (match the names).
// 
// TIP_PROFILE_ASYNC_STOP(name)
// Closes an async profiling section.
// Use this in conjunction with TIP_PROFILE_ASYNC_START (match the names).
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Snapshots: TIP uses a binary interchange format for storing profiling event
// information called a "snapshot", to allow for easier implementation changes,
// creation of custom exporters and conversion between file formats.
// 
// A snapshot can be created from the internal state of the profiler, and
// contains all information in a condensed and easy to process form. It is its
// own seperate copy, and is independent of the profiler state.

TIP_API tip_Snapshot tip_create_snapshot(bool erase_snapshot_data_from_internal_state = false, bool prohibit_simultaneous_events = true);
// Creates a snapshot from of all profiling events recorded up to that point. A
// snapshot contains all the profiling information in a compact format, that can
// be output to a file.
// 
// If erase_snapshot_data_from_internal_state is true, all profiling events that
// are part of the snapshot will be erased from the internal state. That means
// that subsequent calls to tip_create_snapshot will not include these events,
// and memory for these events will be released if possible.
// 
// If prohibit_simultaneous_events is true, profiling events that happend on the
// same clock-cycle will be spread out to subsequent clock cycles in the
// snapshot. This can prevent issues when visualizing the data in a frontend.
// 
// (Events happen on the same cycle may happen more often than you think, since
// most clocks I've observed report a much higher resolution than they actually
// have)

TIP_API void tip_clear_internal_profiling_data();

TIP_API void tip_free_snapshot(tip_Snapshot snapshot);
// Frees the memory of a snapshot

TIP_API bool operator==(tip_Snapshot& lhs, tip_Snapshot& rhs);
// Compares two snapshot for equality
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Exporting: Once a Snapshot is made, it can be exported to a file format.
// Currently, the only output format besides an experimental binary compressed
// format is a JSON-format that can be read by the chrome profiling frontend.

TIP_API int64_t tip_export_snapshot_to_chrome_json(tip_Snapshot snapshot, char* file_name, bool profile_export_and_include_in_the_output = true);
// Outputs the a given snapshot to a file, that can be read by the chrome
// profiling-frontend. To use the chrome profiling frontend, navigate to the URL
// "chrome://tracing" in a chrome browser.

TIP_API int64_t tip_export_state_to_chrome_json(char* file_name, bool profile_export_and_include_in_the_output = true);
// Creates a snapshot, exports it using tip_export_snapshot_to_chrome_json and
// frees it. Simply a shortcut

TIP_API uint64_t tip_get_chrome_json_size_estimate(tip_Snapshot snapshot, float percentage_of_async_events = 0.01f, uint64_t average_name_length = 10);
// Calculates a size estimate of the json file, that would be generated by
// tip_export_snapshot_to_chrome_json for a given snapshot.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Misc:

const uint64_t tip_info_category = 1llu << 63;
const uint64_t tip_all_categories = UINT64_MAX;

TIP_API void tip_save_profile_event(const char* name, tip_Event_Type type, uint64_t categories); //uses the current time, calculates strlen
TIP_API void tip_save_profile_event(uint64_t timestamp, const char* name, uint64_t name_length_including_terminator, tip_Event_Type type, uint64_t categories);
TIP_API uint64_t tip_get_timestamp();

TIP_API int64_t tip_scoped_profiler_push_name(const char* data, uint64_t size);
TIP_API char* tip_scoped_profiler_pop_name(uint64_t index);

struct tip_Conditional_Scope_Profiler{
  int64_t condition = false;
  uint64_t categories;
  tip_Conditional_Scope_Profiler(const char* event_name, bool new_condition, uint64_t new_categories){

#ifdef TIP_GLOBAL_TOGGLE
    new_condition = new_condition && tip_get_global_toggle();
#endif
    if(new_condition){
      tip_save_profile_event(event_name, tip_Event_Type::start, new_categories);
      condition = true;
      categories = new_categories;
    }
  }

  ~tip_Conditional_Scope_Profiler(){
    if(condition)
      tip_save_profile_event(nullptr, tip_Event_Type::stop, categories);
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


#endif //END TIP_HEADER



#ifdef TIP_IMPLEMENTATION

  void tip_zone_start(const char* name, uint64_t categories){
#ifdef TIP_GLOBAL_TOGGLE
    if(tip_get_global_toggle())
#endif
    tip_save_profile_event(name, tip_Event_Type::start, categories);
  }

  void tip_zone_stop(uint64_t categories){
#ifdef TIP_GLOBAL_TOGGLE
    if(tip_get_global_toggle())
#endif
     tip_save_profile_event(nullptr, tip_Event_Type::stop, categories);
  }

  void tip_async_zone_start(const char* name, uint64_t categories){
#ifdef TIP_GLOBAL_TOGGLE
    if(tip_get_global_toggle())
#endif
    tip_save_profile_event(name, tip_Event_Type::start_async, categories);
  }

  void tip_async_zone_stop(const char* name, uint64_t categories){
#ifdef TIP_GLOBAL_TOGGLE
    if(tip_get_global_toggle())
#endif
    tip_save_profile_event(name, tip_Event_Type::stop_async, categories);
  }


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

bool is_type_info_event(tip_Event_Type type){
  return type >= tip_Event_Type::tip_get_new_buffer_start;
}


#ifdef TIP_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif

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
#include <intrin.h>

struct tip_Event_Buffer{
  uint8_t* current_position;
  uint8_t* position_of_first_event; //this is used to delete events from a buffer when making a snapshot
  tip_Event_Buffer* next_buffer;
  uint8_t data[TIP_EVENT_BUFFER_SIZE];
};

struct tip_Thread_State{
  bool initialized = false;
  int32_t thread_id;

#ifdef TIP_MEMORY_LIMIT
  int64_t event_balance_during_blocked = 0; //Since we can't record any events if we are blocked by the memory limit (or actual memory), but we don't pass names on the close-events anymore, the whole timeline after a memory block would be completly unusable since the "event stack" so to speak would containt the wrong number of start and stop events, if a different number of start and stop events happen during the block. We wont be able to record the timestamps or names of any events during the block, but we can keep count on how many start and stop events we saw during the block, to not mess up our stack. A start event increments the balance by one, a stop events decrements. If we are positive at the end of the block, we generate dummy starts, and if we are negative, we generate dummy ends.
#endif

  tip_Event_Buffer* first_event_buffer;
  tip_Event_Buffer* current_event_buffer;
};

struct tip_Global_State{
  bool initialized = false;

  uint64_t category_filter = UINT64_MAX;
  tip_Dynamic_Array<char> category_name_buffer;
  int64_t category_name_indices[64]; //for each category possible (64 bits so 64 categories), contains the index to the start of this categories name in the category_name_buffer (or -1 if no name)

#ifdef TIP_GLOBAL_TOGGLE
  bool toggle = false;
#endif

#ifdef TIP_MEMORY_LIMIT
  //memory limiting strategy: There is a hard limit and a soft lift. The hard limit is a value, that is set by the user and must not be exceeded. If we were to guarantee this by comparing the occupied memory against the hard limit, we would have to take a critical section each time we do the comparison, to guarantee that no race-condition happens (a thread might allocate memory right after an other thread has done the check but before it allocated, leading to two threads allocating memory).
  //To work around this, we use a second limit, the soft limit, that is lower than the hard limit. The core idea is that there is a maximum upper bound of how much we would overrun the limit, if the worst-case race-condition happened. That worst case race-condition is: we have exactly enough memory so that one thread can allocate a buffer, each thread does the check but gets swapped out for another one before it does the allocation, so that all threads end up thinking they can allocate, even though there is only enough room for one more allocation. So the maximum amount of memory we would overshoot the limit in the worst case race-condition is ((#threads - 1) * maximum allocation size). If we subtract that amount from the hard limit, we get our soft limit that guarantees us that we will not overrund the hard one, even in the worst case race condition. That way we avoid taking a critical section.
  //To actually make this happen, we have to know what the maximum allocation size can be. We only do two types of allocations: allocating an event buffer, of which the size is known and growing a dynamic array. To put an upper bound on the dynamic array growth, which normaly grows by a factor and thus has none, we introduce a limit in the data structure itsself.
  //Once an attempt to save a profiling zone hits the limit, we set a flag that prevents any other events from beeing recorded. Since each thread has different buffers, many other events might still be able to record, but it would be very confusing to just "miss" the events of one thread while the others keep going.
  volatile uint64_t occupied_memory;
  uint64_t hard_memory_limit;
  int64_t soft_memory_limit; //this can go negative if the hard limit is really low, and a new thread gets initialized. it will subtract TIP_EVENT_BUFFER_SIZE from this, which might be bigger than the hard limit itsself
  bool blocked_by_memory_limit = false; //if the soft limit is hit, we stop recording of any events, including those that wouldn't need an allocation, because it would be super confusing if some threads would continue on recording while another one doesn't
  Mutex record_memory_limit_events_mutex;
#endif

  int32_t process_id;
  double clocks_per_second;

  Mutex thread_states_mutex;
  tip_Dynamic_Array<tip_Thread_State*> thread_states;
};

thread_local tip_Thread_State tip_thread_state;
static tip_Global_State tip_global_state;



void tip_set_memory_limit(uint64_t limit_in_bytes){
#ifdef TIP_MEMORY_LIMIT
  tip_lock_mutex(tip_global_state.record_memory_limit_events_mutex);
  
  tip_global_state.hard_memory_limit = limit_in_bytes;
  tip_global_state.soft_memory_limit = tip_global_state.hard_memory_limit - TIP_EVENT_BUFFER_SIZE * (tip_global_state.thread_states.size - 1);

  if(tip_global_state.blocked_by_memory_limit && int64_t(tip_global_state.occupied_memory + TIP_EVENT_BUFFER_SIZE) <= tip_global_state.soft_memory_limit){
      tip_global_state.blocked_by_memory_limit = false;
      tip_save_profile_event(tip_get_timestamp(), nullptr, 0, tip_Event_Type::tip_recording_halted_because_of_memory_limit_stop, tip_info_category);
  }

  tip_unlock_mutex(tip_global_state.record_memory_limit_events_mutex);
#else
  (void) limit_in_bytes;
#endif
}

uint64_t tip_get_memory_limit(){
#ifdef TIP_MEMORY_LIMIT
  return tip_global_state.hard_memory_limit;
#else
  return 0;
#endif
}

TIP_API uint64_t tip_get_current_memory_footprint(){
#ifdef TIP_MEMORY_LIMIT
  return tip_global_state.occupied_memory;
#else
  uint64_t occupied_memory = 0;

  tip_lock_mutex(tip_global_state.thread_states_mutex);

  for(auto thread_state : tip_global_state.thread_states){
    for(auto event_buffer = thread_state->first_event_buffer; event_buffer; event_buffer = event_buffer->next_buffer)
      occupied_memory += TIP_EVENT_BUFFER_SIZE;
  }

  tip_unlock_mutex(tip_global_state.thread_states_mutex);

  return occupied_memory;
#endif
}


void tip_set_global_toggle(bool toggle) {
#ifdef TIP_GLOBAL_TOGGLE
  tip_global_state.toggle = toggle;
#else
  (void) toggle;
#endif
}

bool tip_get_global_toggle() {
#ifdef TIP_GLOBAL_TOGGLE
  return tip_global_state.toggle;
#else
  return true;
#endif
}

void tip_set_category_filter(uint64_t bitmask){
  tip_global_state.category_filter = bitmask;
}

void tip_add_category_filter(uint64_t bitmask){
  tip_global_state.category_filter = tip_global_state.category_filter | bitmask;
}

void tip_remove_category_filter(uint64_t bitmask){
  tip_global_state.category_filter = tip_global_state.category_filter & ~bitmask;
}

uint64_t tip_get_category_filter(){
  return tip_global_state.category_filter;
}

bool tip_does_category_pass_filter(uint64_t category){
  return category & tip_global_state.category_filter;
}

bool tip_set_category_name(uint64_t category_id, const char* category_name){
  for(uint64_t i = 0; i < 64; i++){
    if(1llu << i == category_id){
      
      //remove previous name from buffer and fix the other names indices since they would maybe move around in the buffer
      int64_t previous_name_index = tip_global_state.category_name_indices[i];
      if(previous_name_index != -1){
        auto previous_name_length = strlen(&tip_global_state.category_name_buffer[previous_name_index]) + 1;
        tip_global_state.category_name_buffer.ordered_remove(previous_name_index, previous_name_length);

        for(uint64_t j = 0; j < 64; j++){
          if(tip_global_state.category_name_indices[j] > previous_name_index)
            tip_global_state.category_name_indices[j] -= previous_name_length;
        }

        tip_global_state.category_name_indices[i] = -1;
      }

      uint64_t name_index = tip_global_state.category_name_buffer.size;
     
      if(!tip_global_state.category_name_buffer.insert((char*) category_name, strlen(category_name) + 1))
        return false;

      tip_global_state.category_name_indices[i] = name_index;
      return true;
    }
  }

  TIP_ASSERT(false && "Category-IDs have to be powers of two! They will be bitmasked later, and that way you can OR them together in your events or filters");
  return false;
}



void tip_save_profile_event_without_checks(uint64_t timestamp, const char* name, uint64_t name_length_including_terminator, tip_Event_Type type, uint64_t categories);

#ifdef TIP_MEMORY_LIMIT
void* tip_try_realloc_with_respect_to_memory_limit(void* previous_allocation, uint64_t allocation_size, uint64_t difference_to_old_allocation_size){
  if(tip_global_state.blocked_by_memory_limit)
    return nullptr;

  if(tip_global_state.hard_memory_limit != 0 && int64_t(tip_global_state.occupied_memory + difference_to_old_allocation_size) > tip_global_state.soft_memory_limit){
    if(tip_thread_state.initialized){

      tip_lock_mutex(tip_global_state.record_memory_limit_events_mutex);
       //we check again because the value of blocked_by_memory_limit might have changed (race condition). The reason why we check for it twice anyway is so that we only have to take the lock when the race condition occurs (which will happen only very rarely if ever) or we legitly can record the even
      if(!tip_global_state.blocked_by_memory_limit && (tip_global_state.category_filter & tip_info_category))
        tip_save_profile_event_without_checks(tip_get_timestamp(), nullptr, 0, tip_Event_Type::tip_recording_halted_because_of_memory_limit_start, tip_info_category);

      tip_global_state.blocked_by_memory_limit = true; //this gets set here to avoid the race condition
      tip_unlock_mutex(tip_global_state.record_memory_limit_events_mutex);
    }

    tip_global_state.blocked_by_memory_limit = true; //we still want to set this even if the thread is not initialized to make the second time hitting this quicker
    return nullptr;
  }

  _InterlockedExchangeAdd64((volatile int64_t*) &tip_global_state.occupied_memory, difference_to_old_allocation_size);
  return TIP_REALLOC(previous_allocation, allocation_size);
}

void* tip_try_malloc_with_respect_to_memory_limit(uint64_t allocation_size){
  return tip_try_realloc_with_respect_to_memory_limit(nullptr, allocation_size, allocation_size);
}

#endif

bool tip_get_new_event_buffer(){
  auto get_buffer_start_time = tip_get_timestamp();

#ifdef TIP_MEMORY_LIMIT
  tip_Event_Buffer* new_buffer = (tip_Event_Buffer*) tip_try_malloc_with_respect_to_memory_limit(sizeof(tip_Event_Buffer));
  if(!new_buffer)
    return false;
#else
  tip_Event_Buffer* new_buffer = (tip_Event_Buffer*) TIP_MALLOC(sizeof(tip_Event_Buffer));
#endif
  new_buffer->current_position = new_buffer->data;
  new_buffer->position_of_first_event = new_buffer->data;
  new_buffer->next_buffer = nullptr;

  if (tip_thread_state.current_event_buffer)
    tip_thread_state.current_event_buffer->next_buffer = new_buffer;
  else
    tip_thread_state.first_event_buffer = new_buffer;

  tip_thread_state.current_event_buffer = new_buffer;

  auto get_buffer_end_time = tip_get_timestamp();

  if(tip_global_state.category_filter & tip_info_category){
    tip_save_profile_event_without_checks(get_buffer_start_time, nullptr, 0, tip_Event_Type::tip_get_new_buffer_start, tip_info_category);
    tip_save_profile_event_without_checks(get_buffer_end_time  , nullptr, 0, tip_Event_Type::tip_get_new_buffer_stop, tip_info_category);
  }

  return true;
}

bool tip_assert_state_is_initialized_or_auto_initialize_if_TIP_AUTO_INIT_is_defined(){
  #ifdef TIP_AUTO_INIT
    if(!tip_thread_state.initialized){
      if(!tip_global_state.initialized){
        tip_global_init();
      }
      return tip_thread_init();
    }
    return true;
  #else
    TIP_ASSERT(tip_thread_state.initialized && "TIP tried to record a profiling event, before the thread state was initialized! To get rid of this error, you can either: 1) call tip_thread_init on this thread before starting to record profiling events on it, 2) #define TIP_AUTO_INIT, which will automatically take care of state initialization, 3) #define TIP_GLOBAL_TOGGLE and use tip_set_global_toggle to prevent recording of any profiling events until you can ensure that tip_thread_init was called. Solution 2) and 3) incur runtime cost (some more if-checks per profiling event), solution 1) does not.");
    return tip_thread_state.initialized;
  #endif
}

void tip_save_profile_event_without_checks(uint64_t timestamp, const char* name, uint64_t name_length_including_terminator, tip_Event_Type type, uint64_t categories) {
  tip_Event_Buffer* buffer = tip_thread_state.current_event_buffer;
  
  TIP_ASSERT(buffer);

  uint8_t* data_pointer = buffer->current_position;
  *((uint64_t*)data_pointer) = timestamp;
  data_pointer += sizeof(timestamp);
  *((tip_Event_Type*)data_pointer) = type;
  data_pointer += sizeof(type);
  *((uint64_t*)data_pointer) = categories;
  data_pointer += sizeof(categories);

  if(!is_type_info_event(type) && type != tip_Event_Type::stop){
    *((uint64_t*)data_pointer) = name_length_including_terminator;
    data_pointer += sizeof(name_length_including_terminator);
    memcpy(data_pointer, name, name_length_including_terminator);
    data_pointer += name_length_including_terminator;
  }

  buffer->current_position = data_pointer;

  TIP_ASSERT(buffer->current_position <= buffer->data + TIP_EVENT_BUFFER_SIZE);
}

double tip_measure_average_duration_of_recording_a_single_profiling_event(uint64_t sample_size){
  tip_assert_state_is_initialized_or_auto_initialize_if_TIP_AUTO_INIT_is_defined();
  
  volatile uint64_t dummy_variable_for_measurement = 0;

  for(int tries = 0; tries < 10; tries++){
    auto control_group_start = tip_get_timestamp();
    for(uint64_t i = 0; i < sample_size / 2; i++){
      for(uint64_t j = 0; j < 1000; j++)
        dummy_variable_for_measurement++;
    }
    auto control_group_stop = tip_get_timestamp();
    auto control_group_duration = control_group_stop - control_group_start;

    auto measurement_start = tip_get_timestamp();
    for(uint64_t i = 0; i < sample_size / 2; i++){
      tip_zone("TIP test measurement", tip_info_category);
      for(uint64_t j = 0; j < 1000; j++)
        dummy_variable_for_measurement++;
    }
    auto measurement_stop = tip_get_timestamp();
    auto measurement_duration = measurement_stop - measurement_start;

    if(control_group_duration > measurement_duration)
      continue; //I have no idea why, but every once in a while the control group would be slower on my machine. More than 10%. This happend even with >10 sec runtime, where we shouldn't be at the mercy of the os scheduler anymore I think. Strange

    return (measurement_duration - control_group_duration) / (tip_global_state.clocks_per_second * double(sample_size));
  }

  return -1;
}

void tip_save_profile_event(const char* name, tip_Event_Type type, uint64_t categories){
  auto timestamp = tip_get_timestamp();
  uint64_t name_length_including_terminator = 0;
  if(name)
    name_length_including_terminator = strlen(name) + 1;

  tip_save_profile_event(timestamp, name, name_length_including_terminator, type, categories);
}

void tip_save_profile_event(uint64_t timestamp, const char* name, uint64_t name_length_including_terminator, tip_Event_Type type, uint64_t categories){
  if(!(categories & tip_global_state.category_filter))
    return;

  const uint64_t tip_info_event_size = sizeof(timestamp) + sizeof(type) + sizeof(categories);

  if(!tip_assert_state_is_initialized_or_auto_initialize_if_TIP_AUTO_INIT_is_defined())
    return;

#ifdef TIP_MEMORY_LIMIT
  if(tip_global_state.blocked_by_memory_limit){
    if(type == tip_Event_Type::start)
      tip_thread_state.event_balance_during_blocked++;
    else if(type == tip_Event_Type::stop)
      tip_thread_state.event_balance_during_blocked--;
    return;
  }

  if(tip_thread_state.event_balance_during_blocked != 0){ //@BUG @TODO @RACE CONDITION
    int64_t balance = tip_thread_state.event_balance_during_blocked;
    tip_thread_state.event_balance_during_blocked = 0;
    if(balance > 0){
      for(int64_t i = 0; i < balance; i++)
        tip_save_profile_event(timestamp, nullptr, 0, tip_Event_Type::stop, tip_all_categories);
    }
    else{
      const char* temp_name = "UNKNOWN ZONE";
      for(int64_t i = 0; i > balance; i--)
        tip_save_profile_event(timestamp, temp_name, tip_strlen(temp_name) + 1, tip_Event_Type::start, tip_all_categories);
    }
  }
#endif

  uint64_t event_size;

  if(is_type_info_event(type))
    event_size = tip_info_event_size;
  else
    event_size = sizeof(timestamp) + sizeof(type) + sizeof(categories) + sizeof(name_length_including_terminator) + name_length_including_terminator;

  // we need enough space in each new buffer to store at least four info events (if TIP_MEMORY_LIMIT is defined, 2 otherwise), besides the user-event in it:
  // tip_get_new_buffer_start, tip_get_new_buffer_stop, tip_recording_halted_because_of_memory_limit_start and tip_recording_halted_because_of_memory_limit_stop
  TIP_ASSERT(event_size + tip_info_event_size * 4 < TIP_EVENT_BUFFER_SIZE && "This name is too long for the current TIP_EVENT_BUFFER_SIZE. To fix this, increase TIP_EVENT_BUFFER_SIZE or choose a shorter name. Trying to save this event would cause an infinite loop, because there wouldn't be enough space left in a newly allocated buffer to store the event, after TIP has put its profiling information about allocating the buffer into it.");

  //we add tip_info_event_size here, because we will need to be able to record a tip_recording_halted_because_of_memory_limit_start info event into this buffer, if the allocation of the next one fails
  if(event_size + tip_info_event_size + tip_thread_state.current_event_buffer->current_position > tip_thread_state.current_event_buffer->data + TIP_EVENT_BUFFER_SIZE){

#ifdef TIP_MEMORY_LIMIT
    bool got_new_buffer = tip_get_new_event_buffer();
    if(!got_new_buffer){
      tip_thread_state.event_balance_during_blocked++;
      return;
    }
#else
    tip_get_new_event_buffer();
#endif
  }

  tip_save_profile_event_without_checks(timestamp, name, name_length_including_terminator, type, categories);
}

bool tip_thread_init(){
  TIP_ASSERT(tip_global_state.initialized && "tip_thread_init was called, before the global state was initialized! To get rid of this error, you can either 1) call tip_global_init before this function, or 2) #define TIP_AUTO_INIT, which will automatically take care of state initialization. Solution 2) incurs runtime cost (an if-check per profiling event), solution 1) does not.");

  if(tip_thread_state.initialized)
    return true;

  tip_thread_state.thread_id = tip_get_thread_id();

  if(!tip_get_new_event_buffer())
    return false;

  tip_lock_mutex(tip_global_state.thread_states_mutex);

#ifdef TIP_MEMORY_LIMIT
  if(!tip_global_state.thread_states.insert(&tip_thread_state)){
    tip_unlock_mutex(tip_global_state.thread_states_mutex);
    return false;
  }

  tip_global_state.soft_memory_limit = tip_global_state.hard_memory_limit - TIP_EVENT_BUFFER_SIZE * (tip_global_state.thread_states.size - 1);
#else
  tip_global_state.thread_states.insert(&tip_thread_state);
#endif

  tip_unlock_mutex(tip_global_state.thread_states_mutex);

  tip_thread_state.initialized = true;
  return true;
}

double tip_global_init(){
  static_assert(TIP_EVENT_BUFFER_SIZE > 128, "TIP_EVENT_BUFFER_SIZE must be at least 128 bytes big. TIP makes various assumptions that rely on that, for example, that a newly allocated buffer is always big enough to hold two events that record that buffers creation time.");

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

  TIP_ASSERT(rdtsc_diff > 0 && reliable_diff > 0 && "We got a zero or negative time interval on trying to determine the frequency of RDTSC");

  double time_passed = double(reliable_diff) / double(tip_get_reliable_timestamp_frequency());
  tip_global_state.clocks_per_second = rdtsc_diff / time_passed;
#else
  tip_global_state.clocks_per_second = double(tip_get_reliable_timestamp_frequency());
#endif

#ifdef TIP_MEMORY_LIMIT
  tip_global_state.occupied_memory = 0;
  tip_global_state.soft_memory_limit = 0;
  tip_global_state.hard_memory_limit = 0;
  tip_global_state.thread_states.init(50, TIP_EVENT_BUFFER_SIZE); //if we fail to allocate here, we will notice on thread init
  tip_global_state.category_name_buffer.init(500, TIP_EVENT_BUFFER_SIZE); //if we fail to allocate here, we will notice when trying to insert the name
  
  tip_global_state.record_memory_limit_events_mutex = tip_create_mutex();
#endif

  for(uint64_t i = 0; i < 64; i++)
    tip_global_state.category_name_indices[i] = -1;

  //default category is 1 << 63, so the highest bit.
  if(tip_global_state.category_name_buffer.insert("TIP info", tip_strlen("TIP info") + 1))
    tip_global_state.category_name_indices[63] = 0;

  tip_global_state.process_id = tip_get_process_id();
  tip_global_state.initialized = true;
  tip_global_state.thread_states_mutex = tip_create_mutex();
  return 1. / tip_global_state.clocks_per_second;
}

const char* tip_get_new_buffer_string = "TIP get new buffer";
const char* tip_recording_halted_string = "TIP recording halted because memory limit was hit";

tip_Snapshot tip_create_snapshot(bool erase_snapshot_data_from_internal_state, bool prohibit_simultaneous_events) {
  if(!tip_assert_state_is_initialized_or_auto_initialize_if_TIP_AUTO_INIT_is_defined())
    return {};

  tip_Snapshot snapshot = {};
  snapshot.clocks_per_second = tip_global_state.clocks_per_second;
  snapshot.process_id = tip_global_state.process_id;
  snapshot.names.init(256);
  memcpy(snapshot.category_name_indices, tip_global_state.category_name_indices, sizeof(tip_global_state.category_name_indices));
  snapshot.category_name_buffer.insert(tip_global_state.category_name_buffer.data, tip_global_state.category_name_buffer.size);

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
        event.categories = *((uint64_t*)data_pointer);
        data_pointer += sizeof(event.categories);

        if(!is_type_info_event(event.type)){
          if (event.type != tip_Event_Type::stop) {
            uint64_t name_length_including_terminator = *((uint64_t*)data_pointer);
            data_pointer += sizeof(name_length_including_terminator);
            event.name_id = snapshot.names.intern_string((char*) data_pointer);
            data_pointer += name_length_including_terminator;
          }
        }
        else{

          switch(event.type){
            case tip_Event_Type::tip_get_new_buffer_start:
            case tip_Event_Type::tip_get_new_buffer_stop:
              event.name_id = snapshot.names.intern_string((char*) tip_get_new_buffer_string); break;
            case tip_Event_Type::tip_recording_halted_because_of_memory_limit_start:
            case tip_Event_Type::tip_recording_halted_because_of_memory_limit_stop:
              event.name_id = snapshot.names.intern_string((char*) tip_recording_halted_string); break;
            default: 
              TIP_ASSERT(false && "This is a bug in the library. Sorry!");
          }
        }

        if(prohibit_simultaneous_events && thread_events.size > 0){
          auto previous_timestamp = thread_events[thread_events.size - 1].timestamp;
          //it can happen that two events have the exact same timestamp, which can cause issues for a frontend (like the chrome-frontend for example). We detect here if this is the case, and delay the current timestamp by 1 clock cycle in time. As long as the clock resolution is high enough, this does not distort the measurings in a significant way
          //we additionally have to check if the current timestamp is smaller than the previous: If 3 or more events have the same timestamp, we would advanced the second one. In this case, the third timestamp would be smaller than the second timestamp, so we have to advance it by 2 clock cycles, and so on.
          if(event.timestamp <= previous_timestamp)
            event.timestamp = previous_timestamp + 1;
        }
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
          TIP_FREE(event_buffer);
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
        array->insert('"');
        break;
      case '\r':
        array->insert('\\');
        array->insert('r');
        break;
      case '\b':
        array->insert('\\');
        array->insert('b');
        break;
      case '\f':
        array->insert('\\');
        array->insert('f');
        break;
      default:
        array->insert(string[i]);
    }
  }
  array->insert('\0');
}

uint64_t tip_get_chrome_json_size_estimate(tip_Snapshot snapshot, float percentage_of_async_events, uint64_t average_name_length){
  auto async_events = percentage_of_async_events * (float)snapshot.number_of_events;
  auto non_async_events = (1.f - percentage_of_async_events) * (float)snapshot.number_of_events;
  auto total_events = async_events + non_async_events / 2; //we can pack a pair of non async begin and end into one event
  return uint64_t(total_events) * (126 + average_name_length); //126 was measured as the typical length of serialized event (without the name).
}

int64_t tip_export_state_to_chrome_json(char* file_name, bool profile_export_and_include_in_the_output){
  auto snapshot = tip_create_snapshot();
  auto file_size = tip_export_snapshot_to_chrome_json(snapshot, file_name, profile_export_and_include_in_the_output);
  tip_free_snapshot(snapshot);
  return file_size;
}

#include "stdarg.h"

int64_t tip_export_snapshot_to_chrome_json(tip_Snapshot snapshot, char* file_name, bool profile_export_and_include_in_the_output){
  if(snapshot.number_of_events == 0)
    return 0;

  uint64_t export_start_time = tip_get_timestamp();

  FILE* file = nullptr;
  fopen_s(&file, file_name, "w+");

  uint64_t first_timestamp = snapshot.events[0][0].timestamp;

  for(int thread_index = 0; thread_index < snapshot.thread_ids.size; thread_index++){
    if(snapshot.events[thread_index].size > 0)
      first_timestamp = tip_min(first_timestamp, snapshot.events[thread_index][0].timestamp);
  }

  tip_Dynamic_Array<tip_Event> event_stack;
  tip_Dynamic_Array<char> escaped_name_buffer;
  tip_Dynamic_Array<char> file_buffer;
  file_buffer.init(tip_get_chrome_json_size_estimate(snapshot, 0.1f, 30)); //just a random guess, but better than nothing

  bool first = true;

  auto printf_to_file_buffer = [&](const char* format, ...){
    va_list args;
    va_start(args, format);
    int characters_to_print = vsnprintf(file_buffer.data + file_buffer.size, file_buffer.capacity - file_buffer.size, format, args);
    va_end(args);

    TIP_ASSERT(characters_to_print > 0);
    
    if(characters_to_print + 1 > file_buffer.capacity - file_buffer.size){
      file_buffer.reserve(file_buffer.size + characters_to_print + 1);

      va_start(args, format);
      int printed_characters = vsnprintf(file_buffer.data + file_buffer.size, file_buffer.capacity - file_buffer.size, format, args) + 1;
      va_end(args);

      TIP_ASSERT(printed_characters > 0 && printed_characters == characters_to_print && (printed_characters + 1) <= (file_buffer.capacity - file_buffer.size));
    }

    file_buffer.size += characters_to_print;
  };

  auto copy_string_to_file_buffer = [&](const char* s){
    file_buffer.insert((char*) s, tip_strlen(s));
  };
  
  auto print_event_to_buffer = [&](const char* name, uint64_t categories, char ph, double ts, int32_t tid, double dur = 0){
    tip_zone("print_event_to_buffer", 1);

    if(!first) copy_string_to_file_buffer(",\n");
    first = false;

    copy_string_to_file_buffer("  {\"name\":\"");
    copy_string_to_file_buffer((char*) name);
    copy_string_to_file_buffer("\",\"ph\":\"");
    file_buffer.insert(ph);
    copy_string_to_file_buffer("\",\"ts\":");
    printf_to_file_buffer("%.3f,\"pid\":%d,\"tid\":%d", ts, snapshot.process_id, tid);
    copy_string_to_file_buffer(",\"cat\":\"");

    bool first_category = true;

    for(uint64_t i = 0; categories >= (1llu << i) && i < 64; i++){
      if((1llu << i) & categories){

        if(first_category)
          first_category = false;
        else
          file_buffer.insert(',');

        if(snapshot.category_name_indices[i] != -1){
          tip_escape_string_for_json(&snapshot.category_name_buffer[snapshot.category_name_indices[i]], &escaped_name_buffer);
          file_buffer.insert(escaped_name_buffer.data, escaped_name_buffer.size - 1);
        }
        else{
          printf_to_file_buffer("%llu", i);
        }
      }
    }

    if(ph == 'X')                   printf_to_file_buffer("\",\"dur\":%.3f}", dur);
    else if(ph == 'b' || ph == 'e') {copy_string_to_file_buffer("\",\"id\":1}");}
    else                            {copy_string_to_file_buffer("\"}"         );}
  };

  copy_string_to_file_buffer("{\"traceEvents\": [\n");

  //the frontend wants timestamps and durations in units of microseconds. We convert our timestamp by normalizing to units of seconds (with clocks_per_second) and then mulitplying by 10^6. Printing the numbers with 3 decimal places effectively yields a resolution of one nanosecond
  auto timestamp_to_microseconds = [&](uint64_t timestamp){return double(timestamp - first_timestamp) * (1000000 / snapshot.clocks_per_second);};

  for(int thread_index = 0; thread_index < snapshot.thread_ids.size; thread_index++){
    int32_t thread_id = snapshot.thread_ids[thread_index];

    for(tip_Event event : snapshot.events[thread_index]){
      double timestamp_in_ms = timestamp_to_microseconds(event.timestamp);
      tip_escape_string_for_json(snapshot.names.get_string(event.name_id), &escaped_name_buffer);
      const char* name = &escaped_name_buffer[0];

      switch(event.type){
        case tip_Event_Type::start:
        case tip_Event_Type::tip_get_new_buffer_start:{
          event_stack.insert(event); //we put this event on the stack, so we can check if we can form duration events using this and its corresponding close event later
        } break;

        case tip_Event_Type::stop:
        case tip_Event_Type::tip_get_new_buffer_stop:{
        //we check if the last thing on the stack is the corresponding start event for this stop event. if so, we merge both into a duration event and print it
          if(event_stack.size == 0
            || (event.type == tip_Event_Type::stop                    && event_stack[event_stack.size - 1].type != tip_Event_Type::start                   )
            || (event.type == tip_Event_Type::tip_get_new_buffer_stop && event_stack[event_stack.size - 1].type != tip_Event_Type::tip_get_new_buffer_start)){
            print_event_to_buffer("UNKNOWN STOP EVENT", event.categories, 'i', timestamp_in_ms, thread_id);
            continue; //there is no matching event
          }
          
          auto start_event = event_stack.pop_last();
          tip_escape_string_for_json(snapshot.names.get_string(start_event.name_id), &escaped_name_buffer);
          name = &escaped_name_buffer[0];

          double start_time_in_ms = timestamp_to_microseconds(start_event.timestamp);
          print_event_to_buffer(name, event.categories, 'X', start_time_in_ms, thread_id, timestamp_in_ms - start_time_in_ms);
        } break;

        case tip_Event_Type::start_async:
        case tip_Event_Type::tip_recording_halted_because_of_memory_limit_start:{
          print_event_to_buffer(name, event.categories, 'b', timestamp_in_ms, thread_id);
        } break;

        case tip_Event_Type::stop_async:
        case tip_Event_Type::tip_recording_halted_because_of_memory_limit_stop:{
          print_event_to_buffer(name, event.categories, 'e', timestamp_in_ms, thread_id);
        } break;

        case tip_Event_Type::event:{
          print_event_to_buffer(name, event.categories, 'i', timestamp_in_ms, thread_id);
        } break;

        default:{
          TIP_ASSERT(false && "Unhandled event type!");
        }
      }
    }

    //print all start and stop events that don't have a corresponding event they could form a duration event with
    for(tip_Event event : event_stack){
      double timestamp_in_ms = timestamp_to_microseconds(event.timestamp);
      tip_escape_string_for_json(snapshot.names.get_string(event.name_id), &escaped_name_buffer);
      char* name = escaped_name_buffer.data;

      switch(event.type){
        case tip_Event_Type::start:
        case tip_Event_Type::tip_get_new_buffer_start:{
          print_event_to_buffer(name, event.categories, 'B', timestamp_in_ms, thread_id);
        } break;

        case tip_Event_Type::stop:
        case tip_Event_Type::tip_get_new_buffer_stop:{
          print_event_to_buffer(name, event.categories, 'E', timestamp_in_ms, thread_id);
        } break;

        default:{
          TIP_ASSERT(false && "unhandled event type on the stack!");
        }
      }
    }

    event_stack.clear();
  }

  event_stack.destroy();
  escaped_name_buffer.destroy();

  if(profile_export_and_include_in_the_output){
    auto start_time_in_ms = timestamp_to_microseconds(export_start_time);
    auto end_time_in_ms = timestamp_to_microseconds(tip_get_timestamp());
    print_event_to_buffer("TIP export snapshot to chrome JSON", tip_info_category, 'X', start_time_in_ms, tip_thread_state.thread_id, end_time_in_ms - start_time_in_ms);
  }


  copy_string_to_file_buffer("\n],\n\"displayTimeUnit\": \"ns\"\n}");
  file_buffer.insert('\0');
  fputs(file_buffer.data, file);
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

#ifdef TIP_INCLUDE_EXPERIMENTAL_TCB3

#ifndef TIP_FILE_FORMAT_COMPRESSED_BINARY_V3_HEADER
#define TIP_FILE_FORMAT_COMPRESSED_BINARY_V3_HEADER
namespace tip_file_format_tcb3{
  uint64_t export_snapshot(tip_Snapshot snapshot, char* file_name, tip_Dynamic_Array<uint64_t>* diff_sizes = nullptr);
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

#endif //TIP_FILE_FORMAT_COMPRESSED_BINARY_V3_HEADER

#ifdef TIP_IMPLEMENTATION

namespace tip_file_format_tcb3{
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
    uint64_t diff_sizes_buffer[] = {6, 7, 8, 10, 12, 16, 22, 64};
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

  uint64_t export_snapshot(tip_Snapshot snapshot, char* file_name, tip_Dynamic_Array<uint64_t>* diff_sizes){
    tip_Dynamic_Array<uint64_t> default_diff_sizes; //this variable is here to go out of scope because we use a pointer to that

    ((void)file_name);
    if(!diff_sizes){
      default_diff_sizes = get_default_diff_sizes();
      diff_sizes = &default_diff_sizes;
    }

    uint64_t buffer_size = get_conservative_size_estimate_for_serialized_snapshot(snapshot);
    printf("\nSize Max Estimate:       %10.3f KB\n\n", double(buffer_size) / 1024.);
    char* buffer = (char*)TIP_MALLOC(buffer_size);
    char* initial_buffer_position = buffer;
    
    char* debug_position = buffer;

    {//text header, padding and version
      char* text_header = "This is the tcb3 file format! (tip compressed binary format version 3)\n";
      uint64_t text_header_size = tip_strlen(text_header);
      TIP_ASSERT(text_header_size < 99);
      uint64_t text_header_padding_size = 100 - text_header_size;
      uint64_t version = 3;

      serialize_range_byte_aligned(&buffer, text_header, text_header_size);
      serialize_zeros_byte_aligned(&buffer, text_header_padding_size);
      serialize_value_byte_aligned(&buffer, version);
    }

    serialize_value_byte_aligned(&buffer, snapshot.clocks_per_second); 
    serialize_value_byte_aligned(&buffer, snapshot.process_id); 
    serialize_value_byte_aligned(&buffer, snapshot.number_of_events);


    printf("Static Header Part:      %10lld B\n", buffer - debug_position); debug_position = buffer;

    serialize_dynamic_array_byte_aligned(&buffer, snapshot.names.name_buffer);
    serialize_dynamic_array_byte_aligned(&buffer, snapshot.thread_ids);
    printf("Name Buffer+Thread Ids:  %10lld B\n", buffer - debug_position); debug_position = buffer;

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
    printf("Decoding Table:          %10lld B\n", buffer - debug_position); debug_position = buffer;


    uint64_t substitue_name_index_size = get_number_bits_needed_to_represent_number(name_index_decoding_table.size);
    uint64_t event_type_size = get_number_bits_needed_to_represent_number(uint64_t(tip_Event_Type::enum_size) - 1);
    uint64_t diff_size_index_size = get_number_bits_needed_to_represent_number(diff_sizes->size - 1);


    serialize_value_byte_aligned(&buffer, substitue_name_index_size);
    serialize_value_byte_aligned(&buffer, event_type_size);
    serialize_dynamic_array_byte_aligned(&buffer, *diff_sizes);
    serialize_value_byte_aligned(&buffer, diff_size_index_size);
    serialize_value_byte_aligned(&buffer, snapshot.names.name_indices.size);

    printf("Auxilliary:              %10lld B\n", buffer - debug_position); debug_position = buffer;
    printf("Header Total:            %10lld B\n", buffer - initial_buffer_position);

    uint64_t debug_size_event_type = 0;
    uint64_t debug_size_name_indices = 0;
    uint64_t debug_size_timestamps = 0;
    uint64_t debug_size_diff_size = 0;

    /*
    printf("event type size is %llu bit\n", event_type_size);
    printf("substitue name index size is %llu bit\n", substitue_name_index_size);
    printf("diff size index size is %llu bit\n", diff_size_index_size);
  */
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
          debug_size_diff_size += diff_size_index_size;

          serialize_range_bit_aligned_bit_length(buffer, &write_position_in_bits, &diff, (*diff_sizes)[selected_diff_size_index]);
          prev_timestamp = event.timestamp;
          debug_size_timestamps += (*diff_sizes)[selected_diff_size_index];

          serialize_range_bit_aligned_bit_length(buffer, &write_position_in_bits, &event.type, event_type_size);
          debug_size_event_type += event_type_size;

          if(event.type != tip_Event_Type::stop){ //names of stop events have to be the same as the last start event on the stack
            serialize_range_bit_aligned_bit_length(buffer, &write_position_in_bits, &name_index_encoding_table[event.name_id], substitue_name_index_size);
            debug_size_name_indices += substitue_name_index_size;
          }
        }
      }

    printf("\nNumber of Events:        %10llu\n", snapshot.number_of_events);
    printf("Events Total:            %10llu B\n", write_position_in_bits / 8);
    printf("Per Event:               %10.3f B\n\n", double(write_position_in_bits) / double(snapshot.number_of_events) / 8.);
      total_bytes_written += (write_position_in_bits + 7) / 8; //the + 7 effectively rounds up in the integer division
    }

    printf("debug_size_event_type:   %10llu B\n", debug_size_event_type / 8);
    printf("debug_size_name_indices: %10llu B\n", debug_size_name_indices / 8);
    printf("debug_size_timestamps:   %10llu B\n", debug_size_timestamps / 8);
    printf("debug_size_diff_size:    %10llu B\n\n", debug_size_diff_size / 8);

    printf("Total Size:              %10.3f KB\n", double(total_bytes_written) / 1024.);
    printf("Total per Event:         %10.3f B\n\n", double(total_bytes_written) / double(snapshot.number_of_events));


    printf("Event Type Size:         %10llu b\n", event_type_size);
    printf("Diff Size Index Size:    %10llu b\n", diff_size_index_size);
    printf("Name Index Size:         %10llu b\n", substitue_name_index_size);

    TIP_ASSERT(total_bytes_written <= buffer_size);

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
    
    TIP_FREE(initial_buffer_position);
    return total_bytes_written;
  }

  tip_Dynamic_Array<char> read_entire_file(char* file_name){
    tip_Dynamic_Array<char> file_buffer;
    FILE* file;

    fopen_s(&file, file_name, "rb");
    fseek(file, 0, SEEK_END);
    uint64_t file_size = ftell(file); 
    rewind(file);

    file_buffer.buffer = (char*)TIP_MALLOC(file_size);
    file_buffer.size = file_size;
    file_buffer.capacity = file_size;

    fread(file_buffer.buffer, file_size, 1, file);
    fclose(file);

    return file_buffer;
  }

  tip_Snapshot import_snapshot(char* file_name){
    tip_Snapshot snapshot;
    tip_Dynamic_Array<char> file_buffer = read_entire_file(file_name);
    char* buffer = file_buffer.buffer;

    {//text header, padding and version
    /*
      char* text_header = "This is the tcb3 file format! (tip compressed binary format version 3)\n";
      uint64_t text_header_size = tip_strlen(text_header);
      TIP_ASSERT(text_header_size < 99);
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
            TIP_ASSERT(last_start_event_name_index_stack.size > 0);
            event.name_id = last_start_event_name_index_stack[last_start_event_name_index_stack.size - 1];
            last_start_event_name_index_stack.clear_last();
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
    
    TIP_ASSERT(total_bytes_written == file_buffer.size);
    file_buffer.destroy();
    return snapshot;
  }

}
#endif //TIP_IMPLEMENTATION
#endif //TIP_INCLUDE_EXPERIMENTAL_TCB3




// MIT License
// 
// Copyright (c) 2018-2019 Simon van Bernem
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
