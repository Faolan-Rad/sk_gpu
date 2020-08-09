// https://simoncoenen.com/blog/programming/graphics/DxcRevised.html

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SKSC_IMPL
#include "sksc.h"

///////////////////////////////////////////

void            get_folder    (char *filename, char *out_dest,  size_t dest_size);
bool            read_file     (char *filename, char **out_text, size_t *out_size);
void            iterate_files (char *input_name, sksc_settings_t *settings);
sksc_settings_t check_settings(int32_t argc, char **argv);

///////////////////////////////////////////

int main(int argc, char **argv) {
	sksc_settings_t settings = check_settings(argc, argv);
	sksc_init();

	iterate_files(argv[argc - 1], &settings);

	sksc_shutdown();
}

///////////////////////////////////////////

sksc_settings_t check_settings(int32_t argc, char **argv) {
	sksc_settings_t result = {};
	result.debug         = true;
	result.optimize      = 3;
	result.ps_entrypoint = L"ps";
	result.vs_entrypoint = L"vs";
	result.shader_model  = L"6_0";

	// Get the inlcude folder
	char folder[512];
	get_folder(argv[argc-1], folder, sizeof(folder));
	mbstowcs_s(nullptr, result.folder, _countof(result.folder), folder, sizeof(folder));

	for (int32_t i=1; i<argc-1; i++) {

	}

	return result;
}

///////////////////////////////////////////

void iterate_files(char *input_name, sksc_settings_t *settings) {
	HANDLE           handle;
	WIN32_FIND_DATAA file_info;

	char folder[512] = {};
	get_folder(input_name, folder, sizeof(folder));

	if((handle = FindFirstFileA(input_name, &file_info)) != INVALID_HANDLE_VALUE) {
		do {
			char filename[1024];
			sprintf_s(filename, "%s%s", folder, file_info.cFileName);

			char  *file_text;
			size_t file_size;
			if (read_file(filename, &file_text, &file_size)) {
				sksc_compile(filename, file_text, settings);
				free(file_text);
			}
		} while(FindNextFileA(handle, &file_info));
		FindClose(handle);
	}
}

///////////////////////////////////////////

bool read_file(char *filename, char **out_text, size_t *out_size) {
	*out_text = nullptr;
	*out_size = 0;

	FILE *fp;
	if (fopen_s(&fp, filename, "rb") != 0 || fp == nullptr) {
		return false;
	}

	fseek(fp, 0L, SEEK_END);
	*out_size = ftell(fp);
	rewind(fp);

	*out_text = (char*)malloc(*out_size+1);
	if (*out_text == nullptr) { *out_size = 0; fclose(fp); return false; }
	fread (*out_text, 1, *out_size, fp);
	fclose(fp);

	(*out_text)[*out_size] = 0;
	return true;
}

///////////////////////////////////////////

void get_folder(char *filename, char *out_dest, size_t dest_size) {
	char drive[16];
	char dir  [512];
	_splitpath_s(filename,
		drive, sizeof(drive),
		dir,   sizeof(dir),
		nullptr, 0, nullptr, 0); 

	sprintf_s(out_dest, dest_size, "%s%s", drive, dir);
}