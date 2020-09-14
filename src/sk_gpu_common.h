#pragma once
///////////////////////////////////////////
// API independant functions             //
///////////////////////////////////////////

typedef enum skr_shader_lang_ {
	skr_shader_lang_hlsl,
	skr_shader_lang_spirv,
	skr_shader_lang_glsl,
	skr_shader_lang_glsl_web,
} skr_shader_lang_;

typedef struct skr_shader_file_stage_t {
	skr_shader_lang_ language;
	skr_stage_       stage;
	uint32_t         code_size;
	void            *code;
} skr_shader_file_stage_t;

typedef struct skr_shader_file_t {
	skr_shader_meta_t       *meta;
	uint32_t                 stage_count;
	skr_shader_file_stage_t *stages;
} skr_shader_file_t;

///////////////////////////////////////////

void               skr_log                     (skr_log_ level, const char *text);
uint64_t           skr_hash                    (const char *string);

bool               skr_shader_file_verify      (const void *file_memory, size_t file_size, uint16_t *out_version, char *out_name, size_t out_name_size);
bool               skr_shader_file_load_memory (const void *file_memory, size_t file_size, skr_shader_file_t *out_file);
bool               skr_shader_file_load        (const char *file, skr_shader_file_t *out_file);
skr_shader_stage_t skr_shader_file_create_stage(const skr_shader_file_t *file, skr_stage_ stage);
void               skr_shader_file_destroy     (      skr_shader_file_t *file);

void               skr_shader_meta_reference   (skr_shader_meta_t *meta);
void               skr_shader_meta_release     (skr_shader_meta_t *meta);
