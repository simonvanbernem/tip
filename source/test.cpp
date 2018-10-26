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
	tip_export_snapshot_to_chrome_json(snapshot, "json1.snapshot");
	tip_export_snapshot_to_binary_uncompressed(snapshot, "binary.snapshot");
	tip_export_snapshot_to_chrome_json(tip_import_binary_uncompressed_to_snapshot("binary.snapshot"), "json2.snapshot");

	tip_free_snapshot(snapshot);

}