#define TIP_IMPLEMENTATION
#define TIP_WINDOWS
#include "tip.h"


int version = 0;
int sub_version = 1;


int main(int argc, char* argv[]){
	printf("THIS IS NOT A FINISHED VERSION (it will just crash if the format is wrong an such funny stuff)! THIS IS NOT FOR PUBLIC RELEASE!!!\n\n");
	printf("This is the TCB%llu to JSON converter version %d.%d\n\n", tip_compressed_binary_version, version, sub_version);
	if(argc == 1){
		printf("I need arguments to work. Type \"%s --help\" to see how to use me!", argv[0]);
	}

	if(argc == 2){
		if(!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h") || !strcmp(argv[1], "/?")){
			printf("Usage: %s [input file] [output file]\n\n", argv[0]);
			printf("This is a small tool to let you convert profiling records in the TCB%llu format (Tiny instrumented profiler Compressed Binary version %lld) to JSON, in a format that the Google Chrome Browser can read with its built-in profiler (chrome://tracing/). If you are not sure if the file you want to convert is in TCB%llu format, just open it in a text editor (maybe you need to set the charset from HEX to utf-8/ascii/whatever) and check that there is this text at the beginning of the file:\n\n%s", tip_compressed_binary_version, tip_compressed_binary_version, tip_compressed_binary_version, tip_compressed_binary_text_header);
		}
	}

	if(argc == 3){
		bool abort = false;

		FILE* test_file;
		fopen_s(&test_file, argv[1], "rb");
		if(!test_file){
			printf("Could not open the input file \"%s\"!\n\n", argv[1]);
			abort = true;
		}
		else{
			fclose(test_file);
		}

		fopen_s(&test_file, argv[2], "wb");
		if(!test_file){
			printf("Could not open the output file \"%s\"!\n\n", argv[2]);
			abort = true;
		}
		else{
			fclose(test_file);
		}

		if(abort){
			printf("There were errors! Converting aborted.");
			return -1;
		}
		/*
		auto snapshot = tip_import_snapshot_from_compressed_binary(argv[1]);
		int64_t size = tip_export_snapshot_to_chrome_json(snapshot, argv[2]);

		printf("Converting was sucessfull. Output file is %s (", argv[2]);

		if(size < 1024 * 1024)
			printf("%.2f KiB", double(size) / 1024.);
		else
			printf("%.2f MiB", double(size) / (1024. * 1024));
		*/

		printf(")");
	}

	return 0;
}