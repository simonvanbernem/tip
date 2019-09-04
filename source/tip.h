//Tiny Instrumented Profiler (TIP) - v0.9 - MIT licensed
//authored from 2018-2019 by Simon van Bernem
// 
// This library gathers information about the execution time of a program and
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
//    @FEATURE TOGGLES: Lists all macros that can alter the behaviour of TIP.
// 
//    @API DOCUMENTATION is where you can find the full API documentation.
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
  tip_export_current_state_to_chrome_json("profiling_data.json");
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

  tip_export_current_state_to_chrome_json("profiling_data.json");
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
// You can define the following values to alter the behaviour of TIP. If you
// don't define a feature toggle, the code for that feature will not be compiled
// and thus has no impact on the runtime performance.

// #define TIP_AUTO_INIT: Makes tip take care of initialization. tip_global_init and
// tip_thread_init will be automatically called, when necessairy.
// 
// #define TIP_USE_RDTSC: Makes TIP use the RDTSC instruction to measure time. This is
// the most accurate and performant way to record the timestamps. Currently,
// this is only supported when compiling for Win32.
// 
// #define TIP_MEMORY_LIMIT: Allows for the specification of a memory limit via
// tip_set_memory_limit that TIP will not violate. Also lets TIP handle out-of-
// memory gracefully, once the global state has been successfully initialized.

// #define TIP_GLOBAL_TOGGLE: Recording of profiling-events can be enabled and
// disabled by setting a global toggle via tip_set_global_toggle. The toggle
// affects all threads.

//------------------------------------------------------------------------------
//----------------------------@INTEGRATION--------------------------------------
//------------------------------------------------------------------------------
// To make the integration of TIP into your codebase easier, you can replace functionality like memory allocation and asserts here. You can also set the size of TIPs event buffers. These buffers will contain the profiling events at runtime.

#ifndef TIP_API
#define TIP_API // Use this to define function prefixes for API functions like dll export/import.
#endif

#ifndef TIP_ASSERT
#include <assert.h>
#define TIP_ASSERT assert
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
//------------------------------------------------------------------------------


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

#ifdef TIP_MEMORY_LIMIT
void* tip_try_realloc_with_respect_to_memory_limit(void* previous_allocation, uint64_t allocation_size, uint64_t difference_to_old_allocation_size);
#endif


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
        break;

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
// thread. If the thread-local state was already initialized, this function does
// nothing. If TIP_AUTO_INIT is defined, there is no need to call this fuction.
// If TIP_MEMORY_LIMIT is defined and initializing the thread state would have
// violated the limit, the function returns false. Otherwise, it returns true.

TIP_API void tip_clear();
// Deletes all recorded events from the internal event buffers. Does not free
// any memory. Threads and global state stays initialized. Existing Snapshots
// are not affected in any way.


TIP_API void tip_reset();
// Resets the internal state completly: All recorded events are deleted, thread
// and global state is de-initialized and needs to be initialized again before
// use. Any dynamically allocated memory is freed, except any memory occupied by
// snapshots. Existings Snapshots are not affected in any way.
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

const uint64_t tip_info_category = 1llu << 63;
// This value represents the category that TIP will use to record profiling
// events concerning its internals, like buffer-allocation or snapshot creation.

const uint64_t tip_all_categories = UINT64_MAX;
// This value represents all categories.

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
// profiling information in. This will distort the accuracy of the data. If this
// distortion becomes too big, consider increasing TIP_EVENT_BUFFER_SIZE to
// allocate fewer, bigger buffers. TIPs memory footprint and in turn the
// frequency of buffer allocations scales with the number of profiling events
// and the length of the event names.
// 
// Most functions take a name argument. This name can be displayed in a frontend
// and is also used to match the start- and stop-call of async profiling zones.
// 
// All functions take a categories argument. Categories are always powers of two
// and can be given names by calling tip_set_category_name. The argument can
// consist of one ore more categories OR-ed together. A category filter can be
// set by calling tip_set_category_filter, that will discard any events that
// don't belong to any category present in the filter. If a single category of
// an event is present in the filter, the event will be recorded. This way you
// can reduce the amount of data recorded and focus on a specific area of code,
// without needing to remove the profiling functions in other areas.

//------------------------------------------------------------------------------
// Manual profilers: Start and stop a zone manually.

TIP_API void tip_zone_start(const char* name, uint64_t categories);
// Starts a profiling zone with the given name and the given categories. Use
// tip_zone_stop to stop the zone.

TIP_API void tip_zone_stop(uint64_t categories);
// Stops the most recently started profiling zone, that was started by
// tip_zone_start or a tip_zone...-macro.

TIP_API void tip_async_zone_start(const char* name, uint64_t categories);
// Starts an async profiling zone with the given name and the given categories.
// Async zones don't have to adhere to the stack-like nature of normal zones. To
// put it formally: The number of zone-starts and -stops between the start and
// stop of a an async zone does not have to be equal. You can for example start
// an async zone on one thread and end it on another, or start two different
// async zones 1 and 2 and end zone 1 before 2. This is not possible with normal
// zones.

TIP_API void tip_async_zone_stop(const char* name, uint64_t categories);
// Stops an async profiling zone with the same name, that was started with
// tip_async_zone_start.

//------------------------------------------------------------------------------
// Scoped profilers: These profilers will start a zone on the line they are
// declared on, and end it at the end of the scope (in usual destructor-order)


//for tip_zone("test", 1), placed on line 201, this will generate: tip_Scope_Profiler profauto201("test", true, 1);
#define tip_zone(/*const char* */ name, /*uint64_t*/ categories)\
  tip_Conditional_Scope_Profiler TIP_CONCAT_STRINGS(profauto, __LINE__)(name, true, categories);
// Starts a profiling zone with a given name and the given categories on the
// line it is declared and automatically stops that zone on scope-exit (in usual
// destructor order). Internally, this starts the zone in its constructor and
// stops in it in its destructor. The name parameter only needs to be valid
// during the initial call, not for the entire scope.

#define tip_zone_cond(/*const char* */ name, /*uint64_t*/ categories, /*bool*/ condition)\
  tip_Conditional_Scope_Profiler TIP_CONCAT_STRINGS(profauto, __LINE__)(name, condition, categories);
// Behaves like tip_zone if the condition is true. Does nothing otherwise.

#define tip_zone_function(/*uint64_t*/ categories)\
  tip_Conditional_Scope_Profiler TIP_CONCAT_STRINGS(profauto, __LINE__)(__FUNCTION__, true, categories);
// Behaves like tip_zone with the name-argument set to the name of the surrounding function.


//------------------------------------------------------------------------------
// Utility: These function provide various utility functionality.

TIP_API const char* tip_tprintf(const char* format, ...);
// Provides sprintf-like functionality with a buffer that is managed by tip. The
// string returned by this stays valid until the next call to tip_tprintf on the
// same thread. The buffer is a static buffer that is of size TIP_EVENT_BUFFER_SIZE,
// since longer names cannot be recorded into a TIP event buffer anyway. This
// function is intended to be used to generate dynamic zone-names, like so:
// 
// tip_zone(tip_tprintf("%s: %s,%d", "hi", dynamic_string, i), tip_info_category);
// tip_zone_start(tip_tprintf("zone number %d", counter), 1);
//------------------------------------------------------------------------------

TIP_API double tip_measure_average_duration_of_recording_a_single_profiling_event(uint64_t sample_size = 100000);
// Measures the average duration of a single profiling event (a call to
// tip_zone_start or tip_zone_async_start for example) over a given sample size.
// This includes any time spend on buffer allocation and additional functionality
// like checks for the memory limit or global toggle. If the memory limit is hit
// during this test, the results might be drastically off, since events take a
// lot less time when they are discarded.
// The measurement uses a control group to gain accurate results. If you have
// ideas on how to improve this, tell me! (I am no expert on micro-profiling).
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

TIP_API void tip_free_snapshot(tip_Snapshot snapshot);
// Frees the memory of a snapshot

TIP_API bool operator==(tip_Snapshot& lhs, tip_Snapshot& rhs);
// Compares two snapshot for equality
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Exporting: Once a Snapshot is made, it can be exported to a file format.
// Currently, the only output format besides an experimental binary compressed
// format is a JSON-format that can be read by the chrome profiling frontend.

TIP_API int64_t tip_export_snapshot_to_chrome_json(tip_Snapshot snapshot, const char* file_name, bool profile_export_and_include_in_the_output = true);
// Outputs the given snapshot to a file, that can be read by the chrome
// profiling-frontend. To use the chrome profiling frontend, navigate to the URL
// "chrome://tracing" in a chrome browser.

TIP_API tip_Dynamic_Array<char> tip_export_snapshot_to_chrome_json(tip_Snapshot snapshot, bool profile_export_and_include_in_the_output = true);
// Outputs the given snapshot to a memory buffer and returns the buffer as a
// tip_Dynamic_Array.

TIP_API int64_t tip_export_current_state_to_chrome_json(const char* file_name, bool profile_export_and_include_in_the_output = true);
// Creates a snapshot, exports it using tip_export_snapshot_to_chrome_json and
// frees it. Simply a shortcut

TIP_API uint64_t tip_get_chrome_json_size_estimate(tip_Snapshot snapshot, float percentage_of_async_events = 0.01f, uint64_t average_name_length = 10);
// Calculates a size estimate of the json file, that would be generated by
// tip_export_snapshot_to_chrome_json for a given snapshot.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Low level: In case you find the interface of TIP lacking, you can create your
// own ways of saving profiling events using these low level functions.

TIP_API void tip_save_profile_event(uint64_t timestamp, const char* name, uint64_t name_length_including_terminator, tip_Event_Type type, uint64_t categories);
// Saves a profiling event to the buffer.
// If TIP_GLOBAL_TOGGLE is defined and the global toggle is disabled, this
// function does nothing.
// If TIP_MEMORY_LIMIT is defined and saving this event would violate the memory
// limit, this function does nothing.
// If the categories argument does not pass the category filter, this function
// does nothing.

TIP_API void tip_save_profile_event(const char* name, tip_Event_Type type, uint64_t categories);
// Behaves like the previous function. The timestamp argument is set to the
// current time, and the name_length_including_terminator argument is calculated
// based off of the name argument.

TIP_API uint64_t tip_get_timestamp();
// Returns the current time as a timestamp. The value of the timestamp is NOT in
// seconds, but some arbitrary frequency depending on the internal clock used.
//------------------------------------------------------------------------------

// Here comes more implementation.
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

void tip_destroy_mutex(Mutex mutex){
  BOOL success = CloseHandle(mutex.handle);
  (void) success;
  TIP_ASSERT(success != 0);
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

void tip_destroy_mutex(Mutex mutex){
  delete mutex.mutex;
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

  bool toggle = false;

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
thread_local char tip_tprintf_buffer[TIP_EVENT_BUFFER_SIZE]; //we use this buffer for the tprint function. If vsnprintf fails due to buffer size, we know that we don't need to record that event, since it won't fit into the event buffer anyway. So we just assert and/or discard the event. It would be nicer if we could put this in the threadstate, but unfortunately this whould make it so big that a local variable thread state causes stack-overflow. That means we could not do "thread_state = {}" for example, which we need to do in tip_reset. The alternative would be to reset the members individually, which would be prone to bugs when changing the members down the line.
static tip_Global_State tip_global_state;

// void tip_clear(){
//   //@TODO make this thread-safe
// }

//@TODO make this thread-safe
void tip_reset(){
  for(auto thread_state : tip_global_state.thread_states){
    tip_Event_Buffer* buffer = thread_state->first_event_buffer;
    while(buffer){
      auto next_buffer = buffer->next_buffer;
      TIP_FREE(buffer);
      buffer = next_buffer;
    }

    *thread_state = {};
  }

  tip_global_state.category_name_buffer.destroy();

#ifdef TIP_MEMORY_LIMIT
  tip_destroy_mutex(tip_global_state.record_memory_limit_events_mutex);
#endif
  tip_destroy_mutex(tip_global_state.thread_states_mutex);

  tip_global_state.thread_states.destroy();
  tip_global_state = {};
}


#include "stdarg.h"

const char* tip_tprintf(const char* format, ...){
  va_list args;
  va_start(args, format);
  int characters_printed = vsnprintf(tip_tprintf_buffer, TIP_EVENT_BUFFER_SIZE, format, args);
  va_end(args);

  TIP_ASSERT(characters_printed > 0 && "vsnprintf returned an error. Please check your format string and arguments!");
  TIP_ASSERT(characters_printed < TIP_EVENT_BUFFER_SIZE && "This name is too long to fit in an event buffer! To fix this error, pick a shorter name or increase the event buffer size using TIP_EVENT_BUFFER_SIZE.");

  if(characters_printed < 0 || characters_printed > TIP_EVENT_BUFFER_SIZE)
    tip_tprintf_buffer[0] = '\0';

  return tip_tprintf_buffer;
}

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

bool tip_set_category_name(uint64_t category_id, const char* category_name){
  tip_assert_state_is_initialized_or_auto_initialize_if_TIP_AUTO_INIT_is_defined();

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
  tip_Event_Buffer* new_buffer = (tip_Event_Buffer*) TIP_REALLOC(nullptr, sizeof(tip_Event_Buffer));
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

    while (event_buffer) {
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

int64_t tip_export_current_state_to_chrome_json(const char* file_name, bool profile_export_and_include_in_the_output){
  auto snapshot = tip_create_snapshot();
  auto file_size = tip_export_snapshot_to_chrome_json(snapshot, file_name, profile_export_and_include_in_the_output);
  tip_free_snapshot(snapshot);
  return file_size;
}


int64_t tip_export_snapshot_to_chrome_json(tip_Snapshot snapshot, const char* file_name, bool profile_export_and_include_in_the_output){
  FILE* file = nullptr;
  fopen_s(&file, file_name, "w+");
  TIP_ASSERT(file && "Failed to open the file.");
  if(!file)
    return -1;

  auto file_buffer = tip_export_snapshot_to_chrome_json(snapshot, profile_export_and_include_in_the_output);
  fputs(file_buffer.data, file);
  file_buffer.destroy();
  uint64_t file_size = uint64_t(ftell(file));
  fclose(file);
  return file_size;
}

#include "stdarg.h"

tip_Dynamic_Array<char> tip_export_snapshot_to_chrome_json(tip_Snapshot snapshot, bool profile_export_and_include_in_the_output){
  if(snapshot.number_of_events == 0)
    return {};

  uint64_t export_start_time = tip_get_timestamp();

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
      int printed_characters = vsnprintf(file_buffer.data + file_buffer.size, file_buffer.capacity - file_buffer.size, format, args);
      va_end(args);

      (void) printed_characters; //suppress warning when compiling in release
      TIP_ASSERT(printed_characters > 0 && printed_characters == characters_to_print && (printed_characters + 1) <= (file_buffer.capacity - file_buffer.size));
    }

    file_buffer.size += characters_to_print;
  };

  auto copy_string_to_file_buffer = [&](const char* s){
    file_buffer.insert((char*) s, tip_strlen(s));
  };
  
  auto print_event_to_buffer = [&](int64_t name_id, uint64_t categories, char ph, double ts, int32_t tid, double dur = 0, const char* name_override = nullptr){
    tip_zone("print_event_to_buffer", 1);

    if(!name_override)
      tip_escape_string_for_json(snapshot.names.get_string(name_id), &escaped_name_buffer);

    if(!first) copy_string_to_file_buffer(",\n");
    first = false;

    copy_string_to_file_buffer("  {\"name\":\"");

    if(name_override)
      copy_string_to_file_buffer(name_override);
    else
      copy_string_to_file_buffer(escaped_name_buffer.data);

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
    else if(ph == 'b' || ph == 'e') printf_to_file_buffer("\",\"id\":%lld}", name_id);
    else                            {copy_string_to_file_buffer("\"}"         );}
  };

  copy_string_to_file_buffer("{\"traceEvents\": [\n");

  //the frontend wants timestamps and durations in units of microseconds. We convert our timestamp by normalizing to units of seconds (with clocks_per_second) and then mulitplying by 10^6. Printing the numbers with 3 decimal places effectively yields a resolution of one nanosecond
  auto timestamp_to_microseconds = [&](uint64_t timestamp){return double(timestamp - first_timestamp) * (1000000 / snapshot.clocks_per_second);};

  for(int thread_index = 0; thread_index < snapshot.thread_ids.size; thread_index++){
    int32_t thread_id = snapshot.thread_ids[thread_index];

    for(tip_Event event : snapshot.events[thread_index]){
      double timestamp_in_ms = timestamp_to_microseconds(event.timestamp);

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
            print_event_to_buffer(-1, event.categories, 'i', timestamp_in_ms, thread_id, 0, "UNKNOWN STOP EVENT");
            continue; //there is no matching event
          }
          
          auto start_event = event_stack.pop_last();
          double start_time_in_ms = timestamp_to_microseconds(start_event.timestamp);
          print_event_to_buffer(start_event.name_id, event.categories, 'X', start_time_in_ms, thread_id, timestamp_in_ms - start_time_in_ms);
        } break;

        case tip_Event_Type::start_async:
        case tip_Event_Type::tip_recording_halted_because_of_memory_limit_start:{
          print_event_to_buffer(event.name_id, event.categories, 'b', timestamp_in_ms, thread_id);
        } break;

        case tip_Event_Type::stop_async:
        case tip_Event_Type::tip_recording_halted_because_of_memory_limit_stop:{
          print_event_to_buffer(event.name_id, event.categories, 'e', timestamp_in_ms, thread_id);
        } break;

        case tip_Event_Type::event:{
          print_event_to_buffer(event.name_id, event.categories, 'i', timestamp_in_ms, thread_id);
        } break;

        default:{
          TIP_ASSERT(false && "Unhandled event type!");
        }
      }
    }

    //print all start and stop events that don't have a corresponding event they could form a duration event with
    for(tip_Event event : event_stack){
      double timestamp_in_ms = timestamp_to_microseconds(event.timestamp);

      switch(event.type){
        case tip_Event_Type::start:
        case tip_Event_Type::tip_get_new_buffer_start:{
          print_event_to_buffer(event.name_id, event.categories, 'B', timestamp_in_ms, thread_id);
        } break;

        case tip_Event_Type::stop:
        case tip_Event_Type::tip_get_new_buffer_stop:{
          print_event_to_buffer(event.name_id, event.categories, 'E', timestamp_in_ms, thread_id);
        } break;

        default:{
          TIP_ASSERT(false && "unhandled event type on the stack!");
        }
      }
    }

    event_stack.clear();
  }

  if(profile_export_and_include_in_the_output){
    auto start_time_in_ms = timestamp_to_microseconds(export_start_time);
    auto end_time_in_ms = timestamp_to_microseconds(tip_get_timestamp());
    print_event_to_buffer(0, tip_info_category, 'X', start_time_in_ms, tip_thread_state.thread_id, end_time_in_ms - start_time_in_ms, "TIP export snapshot to chrome JSON");
  }


  copy_string_to_file_buffer("\n],\n\"displayTimeUnit\": \"ns\"\n}");
  file_buffer.insert('\0');

  event_stack.destroy();
  escaped_name_buffer.destroy();

  return file_buffer;
}

void tip_free_snapshot(tip_Snapshot snapshot){
  snapshot.names.name_buffer.destroy();
  snapshot.names.name_indices.destroy();
  snapshot.thread_ids.destroy();
  for(int i = 0; i < snapshot.events.size; i++){
    snapshot.events[i].destroy();
  }
  snapshot.events.destroy();
  snapshot.category_name_buffer.destroy();
}

#endif //TIP_IMPLEMENTATION

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
