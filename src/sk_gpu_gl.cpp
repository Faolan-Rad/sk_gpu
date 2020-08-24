#ifdef SKR_OPENGL
#include "sk_gpu_dev.h"
///////////////////////////////////////////
// OpenGL Implementation                 //
///////////////////////////////////////////

#include <malloc.h>
#include <stdio.h>

///////////////////////////////////////////

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl32.h>
#elif __ANDROID__
#include <EGL/egl.h>
#include <EGL/eglext.h>
//#include <GLES/gl.h>
//#include <GLES3/gl3.h>
//#include <GLES3/gl3ext.h>

EGLDisplay egl_display;
EGLSurface egl_surface;
EGLContext egl_context;
EGLConfig  egl_config;
#elif _WIN32
#pragma comment(lib, "opengl32.lib")

#define EMSCRIPTEN_KEEPALIVE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

HWND  gl_hwnd;
HDC   gl_hdc;
HGLRC gl_hrc;
#endif

///////////////////////////////////////////
// GL loader                             //
///////////////////////////////////////////

// Reference from here:
// https://github.com/ApoorvaJ/Papaya/blob/3808e39b0f45d4ca4972621c847586e4060c042a/src/libs/gl_lite.h

#define WGL_DRAW_TO_WINDOW_ARB            0x2001
#define WGL_SUPPORT_OPENGL_ARB            0x2010
#define WGL_DOUBLE_BUFFER_ARB             0x2011
#define WGL_ACCELERATION_ARB              0x2003
#define WGL_FULL_ACCELERATION_ARB         0x2027
#define WGL_PIXEL_TYPE_ARB                0x2013
#define WGL_COLOR_BITS_ARB                0x2014
#define WGL_DEPTH_BITS_ARB                0x2022
#define WGL_STENCIL_BITS_ARB              0x2023
#define WGL_TYPE_RGBA_ARB                 0x202B
#define WGL_SAMPLE_BUFFERS_ARB            0x2041
#define WGL_SAMPLES_ARB                   0x2042
#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
#define WGL_CONTEXT_FLAGS_ARB             0x2094
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001

#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_UNIFORM_BUFFER 0x8A11
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_TRIANGLES 0x0004
#define GL_DEBUG_OUTPUT 0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#define GL_VERSION 0x1F02
#define GL_CULL_FACE 0x0B44
#define GL_BACK 0x0405
#define GL_FRONT 0x0404
#define GL_FRONT_AND_BACK 0x0408
#define GL_LINE 0x1B01
#define GL_FILL 0x1B02
#define GL_DEPTH_TEST 0x0B71
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_CUBE_MAP_SEAMLESS 0x884F
#define GL_TEXTURE_CUBE_MAP_ARRAY 0x9009
#define GL_TEXTURE_BINDING_CUBE_MAP 0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X 0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y 0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y 0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z 0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z 0x851A
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_MIN_LOD 0x813A
#define GL_TEXTURE_MAX_LOD 0x813B
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_TEXTURE_WIDTH 0x1000
#define GL_TEXTURE_HEIGHT 0x1001
#define GL_TEXTURE_INTERNAL_FORMAT 0x1003
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_MIRRORED_REPEAT 0x8370
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#define GL_TEXTURE0 0x84C0
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A

#define GL_BLEND 0x0BE2

#define GL_RED 0x1903
#define GL_RGBA 0x1908
#define GL_DEPTH_COMPONENT 0x1902
#define GL_DEPTH_STENCIL 0x84F9

#define GL_R8_SNORM 0x8F94
#define GL_RG8_SNORM 0x8F95
#define GL_RGB8_SNORM 0x8F96
#define GL_RGBA8_SNORM 0x8F97
#define GL_R16_SNORM 0x8F98
#define GL_RG16_SNORM 0x8F99
#define GL_RGB16_SNORM 0x8F9A
#define GL_RGBA16_SNORM 0x8F9B
#define GL_RG 0x8227
#define GL_RG_INTEGER 0x8228
#define GL_R8 0x8229
#define GL_RG8 0x822B
#define GL_RG16 0x822C
#define GL_R16F 0x822D
#define GL_R32F 0x822E
#define GL_RG16F 0x822F
#define GL_RG32F 0x8230
#define GL_R8I 0x8231
#define GL_R8UI 0x8232
#define GL_R16I 0x8233
#define GL_R16UI 0x8234
#define GL_R32I 0x8235
#define GL_R32UI 0x8236
#define GL_RG8I 0x8237
#define GL_RG8UI 0x8238
#define GL_RG16I 0x8239
#define GL_RG16UI 0x823A
#define GL_RG32I 0x823B
#define GL_RG32UI 0x823C
#define GL_RGBA8 0x8058
#define GL_RGBA16 0x805B
#define GL_BGRA 0x80E1
#define GL_SRGB8_ALPHA8 0x8C43
#define GL_RGBA32F 0x8814
#define GL_RGBA16F 0x881A
#define GL_RGBA16UI 0x8D76
#define GL_COMPRESSED_RGB8_ETC2 0x9274
#define GL_COMPRESSED_SRGB8_ETC2 0x9275
#define GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 0x9276
#define GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2 0x9277
#define GL_COMPRESSED_RGBA8_ETC2_EAC 0x9278
#define GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC 0x9279
#define GL_COMPRESSED_R11_EAC 0x9270
#define GL_COMPRESSED_SIGNED_R11_EAC 0x9271
#define GL_COMPRESSED_RG11_EAC 0x9272
#define GL_COMPRESSED_SIGNED_RG11_EAC 0x9273
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_DEPTH24_STENCIL8 0x88F0

#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_DOUBLE 0x140A

#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84

#define GL_DEBUG_SEVERITY_NOTIFICATION 0x826B
#define GL_DEBUG_SEVERITY_HIGH 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM 0x9147
#define GL_DEBUG_SEVERITY_LOW 0x9148

//#endif

#if defined(_WIN32) || defined(__ANDROID__)

#ifdef _WIN32
#define GLDECL __stdcall
#else
#define GLDECL
#endif

typedef void (GLDECL *GLDEBUGPROC)(uint32_t source, uint32_t type, uint32_t id, int32_t severity, int32_t length, const char* message, const void* userParam);

#define GL_API \
    GLE(void,     LinkProgram,             uint32_t program) \
	GLE(void,     ClearColor,              float r, float g, float b, float a) \
	GLE(void,     Clear,                   uint32_t mask) \
	GLE(void,     Enable,                  uint32_t cap) \
	GLE(void,     Disable,                 uint32_t cap) \
	GLE(void,     PolygonMode,             uint32_t face, uint32_t mode) \
	GLE(uint32_t, GetError,                ) \
    GLE(void,     GetProgramiv,            uint32_t program, uint32_t pname, int32_t *params) \
    GLE(uint32_t, CreateShader,            uint32_t type) \
    GLE(void,     ShaderSource,            uint32_t shader, int32_t count, const char* const *string, const int32_t *length) \
    GLE(void,     CompileShader,           uint32_t shader) \
    GLE(void,     GetShaderiv,             uint32_t shader, uint32_t pname, int32_t *params) \
    GLE(void,     GetShaderInfoLog,        uint32_t shader, int32_t bufSize, int32_t *length, char *infoLog) \
	GLE(void,     GetProgramInfoLog,       uint32_t program, int32_t maxLength, int32_t *length, char *infoLog) \
    GLE(void,     DeleteShader,            uint32_t shader) \
    GLE(uint32_t, CreateProgram,           void) \
    GLE(void,     AttachShader,            uint32_t program, uint32_t shader) \
    GLE(void,     DetachShader,            uint32_t program, uint32_t shader) \
    GLE(void,     UseProgram,              uint32_t program) \
    GLE(void,     DeleteProgram,           uint32_t program) \
    GLE(void,     GenVertexArrays,         int32_t n, uint32_t *arrays) \
    GLE(void,     BindVertexArray,         uint32_t array) \
    GLE(void,     BufferData,              uint32_t target, int32_t size, const void *data, uint32_t usage) \
    GLE(void,     GenBuffers,              int32_t n, uint32_t *buffers) \
    GLE(void,     BindBuffer,              uint32_t target, uint32_t buffer) \
    GLE(void,     DeleteBuffers,           int32_t n, const uint32_t *buffers) \
	GLE(void,     GenTextures,             int32_t n, uint32_t *textures) \
	GLE(void,     GenFramebuffers,         int32_t n, uint32_t *ids) \
	GLE(void,     DeleteFramebuffers,      int32_t n, uint32_t *ids) \
	GLE(void,     FramebufferTexture2D,    uint32_t target, uint32_t attachment, uint32_t textarget, uint32_t texture, int32_t level) \
	GLE(void,     DeleteTextures,          int32_t n, const uint32_t *textures) \
	GLE(void,     BindTexture,             uint32_t target, uint32_t texture) \
	GLE(void,     BindFramebuffer,         uint32_t target, uint32_t framebuffer) \
	GLE(void,     FramebufferTexture,      uint32_t target, uint32_t attachment, uint32_t texture, int32_t level) \
    GLE(void,     TexParameteri,           uint32_t target, uint32_t pname, int32_t param) \
	GLE(void,     GetInternalformativ,     uint32_t target, uint32_t internalformat, uint32_t pname, int32_t bufSize, int32_t *params)\
	GLE(void,     GetTexLevelParameteriv,  uint32_t target, int32_t level, uint32_t pname, int32_t *params) \
	GLE(void,     TexParameterf,           uint32_t target, uint32_t pname, float param) \
	GLE(void,     TexImage2D,              uint32_t target, int32_t level, int32_t internalformat, int32_t width, int32_t height, int32_t border, uint32_t format, uint32_t type, const void *data) \
    GLE(void,     ActiveTexture,           uint32_t texture) \
	GLE(void,     GenerateMipmap,          uint32_t target) \
    GLE(void,     BindAttribLocation,      uint32_t program, uint32_t index, const char *name) \
    GLE(int32_t,  GetUniformLocation,      uint32_t program, const char *name) \
    GLE(void,     Uniform4f,               int32_t location, float v0, float v1, float v2, float v3) \
    GLE(void,     Uniform4fv,              int32_t location, int32_t count, const float *value) \
    GLE(void,     DeleteVertexArrays,      int32_t n, const uint32_t *arrays) \
    GLE(void,     EnableVertexAttribArray, uint32_t index) \
    GLE(void,     VertexAttribPointer,     uint32_t index, int32_t size, uint32_t type, uint8_t normalized, int32_t stride, const void *pointer) \
    GLE(void,     Uniform1i,               int32_t location, int32_t v0) \
	GLE(void,     DrawElementsInstanced,   uint32_t mode, int32_t count, uint32_t type, const void *indices, int32_t primcount) \
	GLE(void,     DrawElements,            uint32_t mode, int32_t count, uint32_t type, const void *indices) \
	GLE(void,     DebugMessageCallback,    GLDEBUGPROC callback, const void *userParam) \
	GLE(void,     BindBufferBase,          uint32_t target, uint32_t index, uint32_t buffer) \
	GLE(void,     BufferSubData,           uint32_t target, int64_t offset, int32_t size, const void *data) \
	GLE(void,     Viewport,                int32_t x, int32_t y, int32_t width, int32_t height) \
	GLE(void,     CullFace,                uint32_t mode) \
	GLE(const char *, GetString,           uint32_t name)

#define GLE(ret, name, ...) typedef ret GLDECL name##proc(__VA_ARGS__); static name##proc * gl##name;
GL_API
#undef GLE

#ifdef _WIN32

// from https://www.khronos.org/opengl/wiki/Load_OpenGL_Functions
// Some GL functions can only be loaded with wglGetProcAddress, and others
// can only be loaded by GetProcAddress.
void *gl_get_func(const char *name, HMODULE module) {
	void *f = (void *)wglGetProcAddress(name);
	if(f == 0 || (f == (void*)0x1) || (f == (void*)0x2) || (f == (void*)0x3) || (f == (void*)-1) ) {
		f = (void *)GetProcAddress(module, name);
	}
	return f;
}
static void gl_load_extensions( ) {
	HMODULE dll = LoadLibraryA("opengl32.dll");
#define GLE(ret, name, ...) gl##name = (name##proc *) gl_get_func("gl" #name, dll); if (gl##name == nullptr) skr_log("Couldn't load gl function gl" #name);
	GL_API
#undef GLE
}

#else

static void gl_load_extensions( ) {
#define GLE(ret, name, ...) gl##name = (name##proc *) eglGetProcAddress("gl" #name); if (gl##name == nullptr) skr_log("Couldn't load gl function gl" #name);
	GL_API
#undef GLE
}

#endif

#endif // _WIN32 or __ANDROID__

///////////////////////////////////////////

int32_t    gl_width  = 0;
int32_t    gl_height = 0;
skr_tex_t *gl_active_rendertarget = nullptr;
uint32_t   gl_current_framebuffer = 0;

///////////////////////////////////////////

uint32_t skr_buffer_type_to_gl   (skr_buffer_type_ type);
uint32_t skr_tex_fmt_to_gl_type  (skr_tex_fmt_ format);
uint32_t skr_tex_fmt_to_gl_layout(skr_tex_fmt_ format);

///////////////////////////////////////////

// Some nice reference: https://gist.github.com/nickrolfe/1127313ed1dbf80254b614a721b3ee9c
int32_t gl_init_win32(void *app_hwnd) {
#ifdef _WIN32
	///////////////////////////////////////////
	// Dummy initialization for pixel format //
	///////////////////////////////////////////

	WNDCLASSA dummy_class = { 0 };
	dummy_class.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	dummy_class.lpfnWndProc   = DefWindowProcA;
	dummy_class.hInstance     = GetModuleHandle(0);
	dummy_class.lpszClassName = "DummyGLWindow";
	if (!RegisterClassA(&dummy_class))
		return false;

	HWND dummy_window = CreateWindowExA(0, dummy_class.lpszClassName, "Dummy GL Window", 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, dummy_class.hInstance, 0);
	if (!dummy_window)
		return false;
	HDC dummy_dc = GetDC(dummy_window);

	PIXELFORMATDESCRIPTOR format_desc = { sizeof(PIXELFORMATDESCRIPTOR) };
	format_desc.nVersion     = 1;
	format_desc.iPixelType   = PFD_TYPE_RGBA;
	format_desc.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	format_desc.cColorBits   = 32;
	format_desc.cAlphaBits   = 8;
	format_desc.iLayerType   = PFD_MAIN_PLANE;
	format_desc.cDepthBits   = 24;
	format_desc.cStencilBits = 8;

	int pixel_format = ChoosePixelFormat(dummy_dc, &format_desc);
	if (!pixel_format) {
		skr_log("Failed to find a suitable pixel format.");
		return false;
	}
	if (!SetPixelFormat(dummy_dc, pixel_format, &format_desc)) {
		skr_log("Failed to set the pixel format.");
		return false;
	}
	HGLRC dummy_context = wglCreateContext(dummy_dc);
	if (!dummy_context) {
		skr_log("Failed to create a dummy OpenGL rendering context.");
		return false;
	}
	if (!wglMakeCurrent(dummy_dc, dummy_context)) {
		skr_log("Failed to activate dummy OpenGL rendering context.");
		return false;
	}

	// Load the function pointers we need to actually initialize OpenGL
	typedef BOOL  (*wglChoosePixelFormatARB_proc)    (HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
	typedef HGLRC (*wglCreateContextAttribsARB_proc) (HDC hDC, HGLRC hShareContext, const int *attribList);
	wglChoosePixelFormatARB_proc    wglChoosePixelFormatARB    = (wglChoosePixelFormatARB_proc   )wglGetProcAddress("wglChoosePixelFormatARB");
	wglCreateContextAttribsARB_proc wglCreateContextAttribsARB = (wglCreateContextAttribsARB_proc)wglGetProcAddress("wglCreateContextAttribsARB");

	// Shut down the dummy so we can set up OpenGL for real
	wglMakeCurrent  (dummy_dc, 0);
	wglDeleteContext(dummy_context);
	ReleaseDC       (dummy_window, dummy_dc);
	DestroyWindow   (dummy_window);

	///////////////////////////////////////////
	// Real OpenGL initialization            //
	///////////////////////////////////////////

	if (app_hwnd == nullptr) {
		WNDCLASSA win_class = { 0 };
		win_class.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
		win_class.lpfnWndProc   = DefWindowProcA;
		win_class.hInstance     = GetModuleHandle(0);
		win_class.lpszClassName = "SKGPUWindow";
		if (!RegisterClassA(&win_class))
			return false;

		app_hwnd = CreateWindowExA(0, win_class.lpszClassName, "sk_gpu Window", 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, win_class.hInstance, 0);
		if (!app_hwnd)
			return false;
	}

	gl_hwnd = (HWND)app_hwnd;
	gl_hdc  = GetDC(gl_hwnd);

	RECT bounds;
	GetWindowRect(gl_hwnd, &bounds);
	gl_width  = bounds.right  - bounds.left;
	gl_height = bounds.bottom - bounds.top;

	// Find a pixel format
	const int format_attribs[] = {
		WGL_DRAW_TO_WINDOW_ARB, true,
		WGL_SUPPORT_OPENGL_ARB, true,
		WGL_DOUBLE_BUFFER_ARB,  true,
		WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
		WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
		WGL_COLOR_BITS_ARB,     32,
		WGL_DEPTH_BITS_ARB,     24,
		WGL_STENCIL_BITS_ARB,   8,
		WGL_SAMPLE_BUFFERS_ARB, 1,
		WGL_SAMPLES_ARB,        4,
		0 };

	pixel_format = 0;
	UINT num_formats = 0;
	if (!wglChoosePixelFormatARB(gl_hdc, format_attribs, nullptr, 1, &pixel_format, &num_formats)) {
		skr_log("Couldn't find pixel format!");
		return false;
	}

	memset(&format_desc, 0, sizeof(format_desc));
	DescribePixelFormat(gl_hdc, pixel_format, sizeof(format_desc), &format_desc);
	if (!SetPixelFormat(gl_hdc, pixel_format, &format_desc)) {
		skr_log("Couldn't set pixel format!");
		return false;
	}

	// Create an OpenGL context
	int attributes[] = {
		WGL_CONTEXT_MAJOR_VERSION_ARB, 3, 
		WGL_CONTEXT_MINOR_VERSION_ARB, 3, 
		WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		0 };
	gl_hrc = wglCreateContextAttribsARB( gl_hdc, 0, attributes );
	if (!gl_hrc) {
		skr_log("Couldn't create GL context!");
		return false;
	}
	if (!wglMakeCurrent(gl_hdc, gl_hrc)) {
		skr_log("Couldn't activate GL context!");
		return false;
	}
#endif // _WIN32
	return 1;
}

///////////////////////////////////////////

int32_t gl_init_emscripten() {
#ifdef __EMSCRIPTEN__
	EmscriptenWebGLContextAttributes attrs;
	emscripten_webgl_init_context_attributes(&attrs);
	attrs.alpha = true;
	attrs.depth = true;
	attrs.enableExtensionsByDefault = true;
	attrs.majorVersion = 2;
	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("canvas", &attrs);
	emscripten_webgl_make_context_current(ctx);
#endif // __EMSCRIPTEN__
	return 1;
}

///////////////////////////////////////////

int32_t gl_init_android(void *native_window) {
#ifdef __ANDROID__
	const EGLint attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_CONFORMANT,   EGL_OPENGL_ES3_BIT_KHR,
		EGL_BLUE_SIZE,  8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE,   8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 16,
		EGL_NONE
	};
	EGLint context_attribs[] = { 
		EGL_CONTEXT_CLIENT_VERSION, 3, 
		EGL_NONE, EGL_NONE };
	EGLint format;
	EGLint numConfigs;

	egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (eglGetError() != EGL_SUCCESS) skr_log("sk_gpu: err eglGetDisplay");

	int32_t major, minor;
	eglInitialize     (egl_display, &major, &minor);
	if (eglGetError() != EGL_SUCCESS) skr_log("sk_gpu: err eglInitialize");
	eglChooseConfig   (egl_display, attribs, &egl_config, 1, &numConfigs);
	if (eglGetError() != EGL_SUCCESS) skr_log("sk_gpu: err eglChooseConfig");
	eglGetConfigAttrib(egl_display, egl_config, EGL_NATIVE_VISUAL_ID, &format);
	if (eglGetError() != EGL_SUCCESS) skr_log("sk_gpu: err eglGetConfigAttrib");
	
	egl_surface = eglCreateWindowSurface(egl_display, egl_config, (EGLNativeWindowType)native_window, nullptr);
	if (eglGetError() != EGL_SUCCESS) skr_log("sk_gpu: err eglCreateWindowSurface");
	egl_context = eglCreateContext      (egl_display, egl_config, nullptr, context_attribs);
	if (eglGetError() != EGL_SUCCESS) skr_log("sk_gpu: err eglCreateContext");

	if (eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context) == EGL_FALSE) {
		skr_log("sk_gpu: Unable to eglMakeCurrent");
		return -1;
	}

	eglQuerySurface(egl_display, egl_surface, EGL_WIDTH,  &gl_width);
	eglQuerySurface(egl_display, egl_surface, EGL_HEIGHT, &gl_height);
#endif // __ANDROID__
	return 1;
}

///////////////////////////////////////////


int32_t skr_init(const char *app_name, void *app_hwnd, void *adapter_id) {
#if defined(_WIN32)
	int32_t result = gl_init_win32(app_hwnd);
#elif defined(__ANDROID__)
	int32_t result = gl_init_android(app_hwnd);
#elif defined(__EMSCRIPTEN__)
	int32_t result = gl_init_emscripten();
#endif
	if (!result)
		return result;

	// Load OpenGL function pointers
	gl_load_extensions();

	skr_log("sk_gpu: Using OpenGL");
	skr_log(glGetString(GL_VERSION));

#if _DEBUG
	skr_log("sk_gpu: Debug info enabled.");
	// Set up debug info for development
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback([](uint32_t source, uint32_t type, uint32_t id, int32_t severity, int32_t length, const char *message, const void *userParam) {
		if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
			//skr_log(message);
		} else {
			skr_log(message);
		} }, nullptr);
#endif // _DEBUG
	
	// Some default behavior
	glEnable  (GL_DEPTH_TEST);  
	glEnable  (GL_CULL_FACE);
	glCullFace(GL_BACK);
	
	return 1;
}

///////////////////////////////////////////

void skr_shutdown() {
#ifdef _WIN32
	wglMakeCurrent(NULL, NULL);
	ReleaseDC(gl_hwnd, gl_hdc);
	wglDeleteContext(gl_hrc);
#elif __ANDROID__
	if (egl_display != EGL_NO_DISPLAY) {
		eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (egl_context != EGL_NO_CONTEXT) eglDestroyContext(egl_display, egl_context);
		if (egl_surface != EGL_NO_SURFACE) eglDestroySurface(egl_display, egl_surface);
		eglTerminate(egl_display);
	}
	egl_display = EGL_NO_DISPLAY;
	egl_context = EGL_NO_CONTEXT;
	egl_surface = EGL_NO_SURFACE;
#endif
}

///////////////////////////////////////////

void skr_draw_begin() {
}

///////////////////////////////////////////

void skr_set_render_target(float clear_color[4], bool clear, skr_tex_t *render_target) {
	gl_active_rendertarget = render_target;
	gl_current_framebuffer = render_target == nullptr ? 0 : render_target->framebuffer;

	glBindFramebuffer(GL_FRAMEBUFFER, gl_current_framebuffer);
	if (render_target) {
		glViewport(0, 0, render_target->width, render_target->height);
	} else {
		glViewport(0, 0, gl_width, gl_height);
	}

	if (clear) {
		glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
}

///////////////////////////////////////////

skr_tex_t *skr_get_render_target() {
	return gl_active_rendertarget;
}

///////////////////////////////////////////

skr_platform_data_t skr_get_platform_data() {
	skr_platform_data_t result = {};
#ifdef _WIN32
	result.gl_hdc = gl_hdc;
	result.gl_hrc = gl_hrc;
#elif __ANDROID__
	result.egl_display = egl_display;
	result.egl_config  = egl_config;
	result.egl_context = egl_context;
#endif
	return result;
}

///////////////////////////////////////////

void skr_draw(int32_t index_start, int32_t index_count, int32_t instance_count) {
	glDrawElementsInstanced(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, (void*)(index_start*sizeof(uint32_t)), instance_count);
}

///////////////////////////////////////////

skr_buffer_t skr_buffer_create(const void *data, uint32_t size_bytes, skr_buffer_type_ type, skr_use_ use) {
	skr_buffer_t result = {};
	result.use  = use;
	result.type = skr_buffer_type_to_gl(type);

	glGenBuffers(1, &result.buffer);
	glBindBuffer(result.type, result.buffer);
	glBufferData(result.type, size_bytes, data, use == skr_use_static ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);

	return result;
}

/////////////////////////////////////////// 

bool skr_buffer_is_valid(const skr_buffer_t *buffer) {
	return buffer->buffer != 0;
}

/////////////////////////////////////////// 

void skr_buffer_update(skr_buffer_t *buffer, const void *data, uint32_t size_bytes) {
	if (buffer->use != skr_use_dynamic)
		return;

	glBindBuffer(buffer->type, buffer->buffer);
	glBufferSubData(buffer->type, 0, size_bytes, data);
}

/////////////////////////////////////////// 

void skr_buffer_set(const skr_buffer_t *buffer, skr_shader_bind_t bind, uint32_t stride, uint32_t offset) {
	if (buffer->type == GL_UNIFORM_BUFFER)
		glBindBufferBase(buffer->type, bind.slot, buffer->buffer); 
	else
		glBindBuffer(buffer->type, buffer->buffer);
}

/////////////////////////////////////////// 

void skr_buffer_destroy(skr_buffer_t *buffer) {
	glDeleteBuffers(1, &buffer->buffer);
	*buffer = {};
}

/////////////////////////////////////////// 

skr_mesh_t skr_mesh_create(const skr_buffer_t *vert_buffer, const skr_buffer_t *ind_buffer) {
	skr_mesh_t result = {};
	result.index_buffer = ind_buffer->buffer;

	// Create a vertex layout
	glGenVertexArrays(1, &result.layout);
	glBindVertexArray(result.layout);
	// enable the vertex data for the shader
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);
	// tell the shader how our vertex data binds to the shader inputs
	glVertexAttribPointer(0, 3, GL_FLOAT, 0, sizeof(skr_vert_t), nullptr);
	glVertexAttribPointer(1, 3, GL_FLOAT, 0, sizeof(skr_vert_t), (void*)(sizeof(float) * 3));
	glVertexAttribPointer(2, 2, GL_FLOAT, 0, sizeof(skr_vert_t), (void*)(sizeof(float) * 6));
	glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, 1, sizeof(skr_vert_t), (void*)(sizeof(float) * 8));

	return result;
}

/////////////////////////////////////////// 

void skr_mesh_set(const skr_mesh_t *mesh) {
	glBindVertexArray(mesh->layout);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_buffer);
}

/////////////////////////////////////////// 

void skr_mesh_destroy(skr_mesh_t *mesh) {
	glDeleteVertexArrays(1, &mesh->layout);
	*mesh = {};
}

/////////////////////////////////////////// 

skr_shader_stage_t skr_shader_stage_create(const void *file_data, size_t shader_size, skr_shader_ type) {
	const char *file_chars = (const char *)file_data;

	skr_shader_stage_t result = {}; 
	result.type = type;

	// Include terminating character
	if (shader_size > 0 && file_chars[shader_size-1] != '\0')
		shader_size += 1;

	uint32_t gl_type = 0;
	switch (type) {
	case skr_shader_pixel:  gl_type = GL_FRAGMENT_SHADER; break;
	case skr_shader_vertex: gl_type = GL_VERTEX_SHADER;   break;
	}

	// Convert the prefix if it doesn't match the GL version we're using
	const char   *prefix_gl      = "#version 450";
	const int32_t prefix_gl_size = strlen(prefix_gl);
	const char   *prefix_es      = "#version 300 es";
	const int32_t prefix_es_size = strlen(prefix_es);
	char         *final_data = (char*)file_chars;
	bool          needs_free = false;
#if __ANDROID__
	if (shader_size >= prefix_gl_size && memcmp(prefix_gl, file_chars, prefix_gl_size) == 0) {
		final_data = (char*)malloc(sizeof(char) * ((shader_size-prefix_gl_size)+prefix_es_size));
		memcpy(final_data, prefix_es, prefix_es_size);
		memcpy(&final_data[prefix_es_size], &file_chars[prefix_gl_size], shader_size - prefix_gl_size);
		needs_free = true;
	}
#else
	if (shader_size >= prefix_es_size && memcmp(prefix_es, file_chars, prefix_es_size) == 0) {
		final_data = (char*)malloc(sizeof(char) * ((shader_size-prefix_es_size)+prefix_gl_size));
		memcpy(final_data, prefix_gl, prefix_gl_size);
		memcpy(&final_data[prefix_gl_size], &file_chars[prefix_es_size], shader_size - prefix_es_size);
		needs_free = true;
	}
#endif

	// create and compile the vertex shader
	result.shader = glCreateShader(gl_type);
	glShaderSource (result.shader, 1, &final_data, NULL);
	glCompileShader(result.shader);

	// check for errors?
	int32_t err, length;
	glGetShaderiv(result.shader, GL_COMPILE_STATUS, &err);
	if (err == 0) {
		char *log;

		glGetShaderiv(result.shader, GL_INFO_LOG_LENGTH, &length);
		log = (char*)malloc(length);
		glGetShaderInfoLog(result.shader, length, &err, log);

		skr_log("Unable to compile shader:\n");
		skr_log(log);
		free(log);
	}
	if (needs_free)
		free(final_data);

	return result;
}

/////////////////////////////////////////// 

void skr_shader_stage_destroy(skr_shader_stage_t *shader) {
	//glDeleteShader(shader->shader);
	*shader = {};
}

///////////////////////////////////////////
// skr_shader_t                          //
///////////////////////////////////////////

skr_shader_t skr_shader_create_manual(skr_shader_meta_t *meta, skr_shader_stage_t v_shader, skr_shader_stage_t p_shader) {
	skr_shader_t result = {};
	result.meta   = meta;
	result.vertex = v_shader.shader;
	result.pixel  = p_shader.shader;
	skr_shader_meta_reference(result.meta);

	result.program = glCreateProgram();
	if (result.vertex) glAttachShader(result.program, result.vertex);
	if (result.pixel)  glAttachShader(result.program, result.pixel);
	glLinkProgram (result.program);

	// check for errors?
	int32_t err, length;
	glGetProgramiv(result.program, GL_LINK_STATUS, &err);
	if (err == 0) {
		char *log;

		glGetProgramiv(result.program, GL_INFO_LOG_LENGTH, &length);
		log = (char*)malloc(length);
		glGetProgramInfoLog(result.program, length, &err, log);

		skr_log("Unable to compile shader program:");
		skr_log(log);
		free(log);
	}

	return result;
}

///////////////////////////////////////////

void skr_shader_destroy(skr_shader_t *shader) {
	glDeleteProgram(shader->program);
	glDeleteShader (shader->vertex);
	glDeleteShader (shader->pixel);
	*shader = {};
}

///////////////////////////////////////////
// skr_pipeline                          //
///////////////////////////////////////////

skr_pipeline_t skr_pipeline_create(skr_shader_t *shader) {
	skr_pipeline_t result = {};
	result.transparency = skr_transparency_none;
	result.cull         = skr_cull_none;
	result.wireframe    = false;
	result.shader       = *shader;

	return result;
}

/////////////////////////////////////////// 

void skr_pipeline_set(const skr_pipeline_t *pipeline) {
	glUseProgram(pipeline->shader.program);
	
	switch (pipeline->transparency) {
	case skr_transparency_blend: {
		glEnable(GL_BLEND);
	}break;
	case skr_transparency_clip:
	case skr_transparency_none: {
		glDisable(GL_BLEND);
	}break;
	}

	switch (pipeline->cull) {
	case skr_cull_back: {
		glEnable  (GL_CULL_FACE);
		glCullFace(GL_BACK);
	} break;
	case skr_cull_front: {
		glEnable  (GL_CULL_FACE);
		glCullFace(GL_FRONT);
	} break;
	case skr_cull_none: {
		glDisable(GL_CULL_FACE);
	} break;
	}
	
#ifndef __ANDROID__
	if (pipeline->wireframe) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	} else {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
#endif
}

/////////////////////////////////////////// 

void skr_pipeline_set_transparency(skr_pipeline_t *pipeline, skr_transparency_ transparency) {
	pipeline->transparency = transparency;
}

/////////////////////////////////////////// 
void skr_pipeline_set_cull(skr_pipeline_t *pipeline, skr_cull_ cull) {
	pipeline->cull = cull;
}
/////////////////////////////////////////// 
void skr_pipeline_set_wireframe(skr_pipeline_t *pipeline, bool wireframe) {
	pipeline->wireframe = wireframe;
}
/////////////////////////////////////////// 

skr_transparency_ skr_pipeline_get_transparency(const skr_pipeline_t *pipeline) {
	return pipeline->transparency;
}

/////////////////////////////////////////// 

skr_cull_ skr_pipeline_get_cull(const skr_pipeline_t *pipeline) {
	return pipeline->cull;
}

/////////////////////////////////////////// 

bool skr_pipeline_get_wireframe(const skr_pipeline_t *pipeline) {
	return pipeline->wireframe;
}

///////////////////////////////////////////

void skr_pipeline_destroy(skr_pipeline_t *pipeline) {
	*pipeline = {};
}

///////////////////////////////////////////

skr_swapchain_t skr_swapchain_create(skr_tex_fmt_ format, skr_tex_fmt_ depth_format, int32_t width, int32_t height) {
	skr_swapchain_t result = {};
	return result;
}

/////////////////////////////////////////// 

void skr_swapchain_resize(skr_swapchain_t *swapchain, int32_t width, int32_t height) {
	gl_width  = width;
	gl_height = height;
}

/////////////////////////////////////////// 

void skr_swapchain_present(skr_swapchain_t *swapchain) {
#ifdef _WIN32
	SwapBuffers(gl_hdc);
#elif __ANDROID__
	eglSwapBuffers(egl_display, egl_surface);
#endif
}

/////////////////////////////////////////// 

skr_tex_t *skr_swapchain_get_next(skr_swapchain_t *swapchain) {
	return nullptr;
}

/////////////////////////////////////////// 

void skr_swapchain_destroy(skr_swapchain_t *swapchain) {
}

/////////////////////////////////////////// 

skr_tex_t skr_tex_from_native(void *native_tex, skr_tex_type_ type, skr_tex_fmt_ format, int32_t width, int32_t height, int32_t array_count) {
	skr_tex_t result = {};
	result.type    = type;
	result.use     = skr_use_static;
	result.mips    = skr_mip_none;
	result.texture = (uint32_t)(uint64_t)native_tex;
	result.format  = format;
	result.width       = width;
	result.height      = height;
	result.array_count = array_count;

	if (type == skr_tex_type_rendertarget) {
		glGenFramebuffers(1, &result.framebuffer);

		glBindFramebuffer     (GL_FRAMEBUFFER, result.framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, result.texture, 0); 
		glBindFramebuffer     (GL_FRAMEBUFFER, gl_current_framebuffer);
	}
	
	return result;
}

/////////////////////////////////////////// 

skr_tex_t skr_tex_create(skr_tex_type_ type, skr_use_ use, skr_tex_fmt_ format, skr_mip_ mip_maps) {
	skr_tex_t result = {};
	result.type   = type;
	result.use    = use;
	result.format = format;
	result.mips   = mip_maps;

	glGenTextures(1, &result.texture);
	skr_tex_settings(&result, type == skr_tex_type_cubemap ? skr_tex_address_clamp : skr_tex_address_repeat, skr_tex_sample_linear, 1);

	if (type == skr_tex_type_rendertarget) {
		glGenFramebuffers(1, &result.framebuffer);
	}
	
	return result;
}

/////////////////////////////////////////// 

bool skr_tex_is_valid(const skr_tex_t *tex) {
	return tex->texture != 0;
}

/////////////////////////////////////////// 

void skr_tex_set_depth(skr_tex_t *tex, skr_tex_t *depth) {
	bool stencil = depth->format == skr_tex_fmt_depthstencil;
	if (tex->type == skr_tex_type_rendertarget) {
		glBindFramebuffer     (GL_FRAMEBUFFER, tex->framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth->texture, 0); 
		glBindFramebuffer     (GL_FRAMEBUFFER, gl_current_framebuffer);
	} else {
		skr_log("Can't bind a depth texture to a non-rendertarget");
	}
}

/////////////////////////////////////////// 

void skr_tex_settings(skr_tex_t *tex, skr_tex_address_ address, skr_tex_sample_ sample, int32_t anisotropy) {
	uint32_t target = tex->type == skr_tex_type_cubemap 
		? GL_TEXTURE_CUBE_MAP 
		: GL_TEXTURE_2D;
	glBindTexture(target, tex->texture);

	uint32_t mode;
	switch (address) {
	case skr_tex_address_clamp:  mode = GL_CLAMP_TO_EDGE;   break;
	case skr_tex_address_repeat: mode = GL_REPEAT;          break;
	case skr_tex_address_mirror: mode = GL_MIRRORED_REPEAT; break;
	default: mode = GL_REPEAT;
	}

	uint32_t filter, min_filter;
	switch (sample) {
	case skr_tex_sample_linear:     filter = GL_LINEAR;  min_filter = tex->mips == skr_mip_generate ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR; break; // Technically trilinear
	case skr_tex_sample_point:      filter = GL_NEAREST; min_filter = GL_NEAREST;              break;
	case skr_tex_sample_anisotropic:filter = GL_LINEAR;  min_filter = tex->mips == skr_mip_generate ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR; break;
	default: filter = GL_LINEAR;
	}

	glTexParameteri(target, GL_TEXTURE_WRAP_S,     mode  );	
	glTexParameteri(target, GL_TEXTURE_WRAP_T,     mode  );
	if (tex->type == skr_tex_type_cubemap) {
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, mode);
	}
	glTexParameteri(target, GL_TEXTURE_MIN_FILTER, min_filter);
	glTexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
#ifdef _WIN32
	glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY, sample == skr_tex_sample_anisotropic ? anisotropy : 1.0f);
#endif
}

/////////////////////////////////////////// 

void skr_tex_set_data(skr_tex_t *tex, void **data_frames, int32_t data_frame_count, int32_t width, int32_t height) {
	uint32_t target = tex->type == skr_tex_type_cubemap 
		? GL_TEXTURE_CUBE_MAP 
		: GL_TEXTURE_2D;
	tex->width       = width;
	tex->height      = height;
	tex->array_count = data_frame_count;
	glBindTexture(target, tex->texture);

	uint32_t format = (uint32_t)skr_tex_fmt_to_native(tex->format);
	uint32_t type   = skr_tex_fmt_to_gl_type         (tex->format);
	uint32_t layout = skr_tex_fmt_to_gl_layout       (tex->format);
	if (tex->type == skr_tex_type_cubemap) {
		if (data_frame_count != 6) {
			skr_log("sk_gpu: cubemaps need 6 data frames");
			return;
		}
		for (size_t f = 0; f < 6; f++)
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+f , 0, format, width, height, 0, layout, type, data_frames[f]);
	} else {
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, layout, type, data_frames == nullptr ? nullptr : data_frames[0]);
	}
	if (tex->mips == skr_mip_generate)
		glGenerateMipmap(target);

	if (tex->type == skr_tex_type_rendertarget) {
		glBindFramebuffer     (GL_FRAMEBUFFER, tex->framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex->texture, 0); 
		glBindFramebuffer     (GL_FRAMEBUFFER, gl_current_framebuffer);
	}
}

/////////////////////////////////////////// 

void skr_tex_set_active(const skr_tex_t *texture, skr_shader_bind_t bind) {
	uint32_t target = texture == nullptr || texture->type != skr_tex_type_cubemap 
		? GL_TEXTURE_2D
		: GL_TEXTURE_CUBE_MAP;

	// Added this in to fix textures initially? Removed it after I switched to
	// explicit binding locations in GLSL. This may need further attention? I
	// have no idea what's happening here!
	//if (texture)
	//	glUniform1i(bind.slot, bind.slot);

	glActiveTexture(GL_TEXTURE0 + bind.slot);
	glBindTexture  (target, texture == nullptr ? 0 : texture->texture);
}

/////////////////////////////////////////// 

void skr_tex_destroy(skr_tex_t *tex) {
	glDeleteTextures    (1, &tex->texture);
	glDeleteFramebuffers(1, &tex->framebuffer);  
	*tex = {};
}

/////////////////////////////////////////// 

uint32_t skr_buffer_type_to_gl(skr_buffer_type_ type) {
	switch (type) {
	case skr_buffer_type_vertex:   return GL_ARRAY_BUFFER;
	case skr_buffer_type_index:    return GL_ELEMENT_ARRAY_BUFFER;
	case skr_buffer_type_constant: return GL_UNIFORM_BUFFER;
	default: return 0;
	}
}

/////////////////////////////////////////// 

int64_t skr_tex_fmt_to_native(skr_tex_fmt_ format) {
	switch (format) {
	case skr_tex_fmt_rgba32:        return GL_SRGB8_ALPHA8;
	case skr_tex_fmt_rgba32_linear: return GL_RGBA8;
	case skr_tex_fmt_rgba64:        return GL_RGBA16UI;
	case skr_tex_fmt_rgba128:       return GL_RGBA32F;
	case skr_tex_fmt_depth16:       return GL_DEPTH_COMPONENT16;
	case skr_tex_fmt_depth32:       return GL_DEPTH_COMPONENT32F;
	case skr_tex_fmt_depthstencil:  return GL_DEPTH24_STENCIL8;
	case skr_tex_fmt_r8:            return GL_R8;
	case skr_tex_fmt_r16:           return GL_R16UI;
	case skr_tex_fmt_r32:           return GL_R32F;
	default: return 0;
	}
}

/////////////////////////////////////////// 

skr_tex_fmt_ skr_tex_fmt_from_native(int64_t format) {
	switch (format) {
	case GL_SRGB8_ALPHA8:       return skr_tex_fmt_rgba32;
	case GL_RGBA8:              return skr_tex_fmt_rgba32_linear;
	case GL_RGBA16UI:           return skr_tex_fmt_rgba64;
	case GL_RGBA32F:            return skr_tex_fmt_rgba128;
	case GL_DEPTH_COMPONENT16:  return skr_tex_fmt_depth16;
	case GL_DEPTH_COMPONENT32F: return skr_tex_fmt_depth32;
	case GL_DEPTH24_STENCIL8:   return skr_tex_fmt_depthstencil;
	case GL_R8:                 return skr_tex_fmt_r8;
	case GL_R16UI:              return skr_tex_fmt_r16;
	case GL_R32F:               return skr_tex_fmt_r32;
	default: return skr_tex_fmt_none;
	}
}

/////////////////////////////////////////// 

uint32_t skr_tex_fmt_to_gl_layout(skr_tex_fmt_ format) {
	switch (format) {
	case skr_tex_fmt_rgba32:
	case skr_tex_fmt_rgba32_linear:
	case skr_tex_fmt_rgba64:
	case skr_tex_fmt_rgba128:       return GL_RGBA;
	case skr_tex_fmt_depth16:
	case skr_tex_fmt_depth32:       return GL_DEPTH_COMPONENT;
	case skr_tex_fmt_depthstencil:  return GL_DEPTH_STENCIL;
	case skr_tex_fmt_r8:
	case skr_tex_fmt_r16:
	case skr_tex_fmt_r32:           return GL_RED;
	default: return 0;
	}
}

/////////////////////////////////////////// 

uint32_t skr_tex_fmt_to_gl_type(skr_tex_fmt_ format) {
	switch (format) {
	case skr_tex_fmt_rgba32:        return GL_UNSIGNED_BYTE;
	case skr_tex_fmt_rgba32_linear: return GL_UNSIGNED_BYTE;
	case skr_tex_fmt_rgba64:        return GL_UNSIGNED_SHORT;
	case skr_tex_fmt_rgba128:       return GL_FLOAT;
	case skr_tex_fmt_depth16:       return GL_UNSIGNED_SHORT;
	case skr_tex_fmt_depth32:       return GL_FLOAT;
	case skr_tex_fmt_depthstencil:  return GL_DEPTH24_STENCIL8;
	case skr_tex_fmt_r8:            return GL_UNSIGNED_BYTE;
	case skr_tex_fmt_r16:           return GL_UNSIGNED_SHORT;
	case skr_tex_fmt_r32:           return GL_FLOAT;
	default: return 0;
	}
}

#endif
