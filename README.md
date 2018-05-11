# tip
This is a tiny instrumented profiler that can be used with google chrome's profiling frontend (chrome://tracing). It focusses on ease of use and performance. 


## setting it up
How to use it:

You can start profiling once you called "tip_global_init" and "tip_thread_init"
Call tip_global_init once in your entire programm.
Call tip_thread_init once in every thread you want to profile.

## making measurements

After that you are ready to go. You can profile your programm in sereveral different ways

use TIP_PROFILE_SCOPE to measure entering and leaving scopes

use TIP_PROFILE_START to manually start, and TIP_PROFILE_STOP to manually end a measurement.

use TIP_PROFILE_ASYNC_START to manually start, and TIP_PROFILE_ASYNC_STOP to manually stop an asynchronous measurement.

asynchronous measurements are usefull, when you are dealing with measurements, that don't have a "call-graph-like" nested structure. Example:

<------------A------------>
               <-----------B---------->
              
this corrensponds to:

TIP_PROFILE_ASYNC_START(A);
TIP_PROFILE_ASYNC_START(B);
TIP_PROFILE_ASYNC_STOP(A);
TIP_PROFILE_ASYNC_STOP(B);

## exporting the measurements
### to chrome's frontend

If you don't care about the inner workings, just put this in your code, and you will get a file that you can open with chrome://tracing
```cpp
tip_export_snapshot_to_chrome_json(tip_create_snapshot(), "your_file_name.xyz");
```

### to intermediary format

tip exports all measurements to an intermediary format, called `tip_Snapshot`. Snapshots are meant to be easily transformable to any format, that the profiler front-end of your choice requires. You can create a snapshot of the current profiler-state by calling `tip_create_snapshot`. Currently, tip only converts snapshot to a format, used by chrome's profiling frontend chrome://tracing by calling `tip_export_snapshot_to_chrome_json`. Feel free to contribute any new exporters you might create for tip. 
