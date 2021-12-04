#include "sk_gpu_dev.h"
#ifdef SKG_DIRECT3D11
///////////////////////////////////////////
// Direct3D11 Implementation             //
///////////////////////////////////////////

#pragma comment(lib,"D3D11.lib")
#pragma comment(lib,"Dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#include <d3d11.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <math.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

///////////////////////////////////////////

ID3D11Device            *d3d_device      = nullptr;
ID3D11DeviceContext     *d3d_context     = nullptr;
ID3D11InfoQueue         *d3d_info        = nullptr;
ID3D11RasterizerState   *d3d_rasterstate = nullptr;
ID3D11DepthStencilState *d3d_depthstate  = nullptr;
skg_tex_t               *d3d_active_rendertarget = nullptr;

///////////////////////////////////////////

bool skg_tex_make_view(skg_tex_t *tex, uint32_t mip_count, uint32_t array_start, bool use_in_shader);

template <typename T>
void skg_downsample_1(T *data, int32_t width, int32_t height, T **out_data, int32_t *out_width, int32_t *out_height);
template <typename T>
void skg_downsample_4(T *data, int32_t width, int32_t height, T **out_data, int32_t *out_width, int32_t *out_height);

///////////////////////////////////////////

int32_t skg_init(const char *app_name, void *adapter_id) {
	UINT creation_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
	creation_flags |= D3D11_CREATE_DEVICE_DEBUG;
	skg_log(skg_log_info, "Requesting debug Direct3D context.");
#endif

	// Find the right adapter to use:
	IDXGIAdapter1 *final_adapter = nullptr;
	IDXGIAdapter1 *curr_adapter  = nullptr;
	IDXGIFactory1 *dxgi_factory  = nullptr;
	int            curr          = 0;
	SIZE_T         video_mem     = 0;
	CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)(&dxgi_factory));
	while (dxgi_factory->EnumAdapters1(curr++, &curr_adapter) == S_OK) {
		DXGI_ADAPTER_DESC1 adapterDesc;
		curr_adapter->GetDesc1(&adapterDesc);

		// By default, we pick the adapter that has the most available memory
		if (adapterDesc.DedicatedVideoMemory > video_mem) {
			video_mem = adapterDesc.DedicatedVideoMemory;
			if (final_adapter != nullptr) final_adapter->Release();
			final_adapter = curr_adapter;
			final_adapter->AddRef();
		}

		// If the user asks for a specific device though, return it right away!
		if (adapter_id != nullptr && memcmp(&adapterDesc.AdapterLuid, adapter_id, sizeof(LUID)) == 0) {
			if (final_adapter != nullptr) final_adapter->Release();
			final_adapter = curr_adapter;
			final_adapter->AddRef();
			break;
		}
		curr_adapter->Release();
	}
	dxgi_factory->Release();

	// Create the interface to the graphics card
	D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
	HRESULT           hr               = D3D11CreateDevice(final_adapter, final_adapter == nullptr ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_UNKNOWN, 0, creation_flags, feature_levels, _countof(feature_levels), D3D11_SDK_VERSION, &d3d_device, nullptr, &d3d_context);
	if (FAILED(hr)) {

		// Message that we failed to initialize with the selected adapter.
		char d3d_info_txt[128];
		if (final_adapter != nullptr) {
			DXGI_ADAPTER_DESC1 final_adapter_info;
			final_adapter->GetDesc1(&final_adapter_info);
			snprintf(d3d_info_txt, sizeof(d3d_info_txt), "Failed starting Direct3D 11 adapter '%ls': 0x%08X", &final_adapter_info.Description, hr);
			final_adapter->Release();
		} else {
			snprintf(d3d_info_txt, sizeof(d3d_info_txt), "Failed starting Direct3D 11 adapter 'Default adapter': 0x%08X", hr);
		}
		skg_log(skg_log_critical, d3d_info_txt);

		// Get a human readable description of that error message.
		char *error_text = NULL;
		FormatMessageA(
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			hr,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(char*)&error_text, 0,
			NULL);
		skg_log(skg_log_critical, error_text);
		LocalFree(error_text);

		return 0;
	}

	// Notify what device and API we're using
	if (final_adapter != nullptr) {
		DXGI_ADAPTER_DESC1 final_adapter_info;
		final_adapter->GetDesc1(&final_adapter_info);
		char d3d_info_txt[128];
		snprintf(d3d_info_txt, sizeof(d3d_info_txt), "Using Direct3D 11: %ls", &final_adapter_info.Description);
		skg_log(skg_log_info, d3d_info_txt);
		final_adapter->Release();
	} else {
		skg_log(skg_log_info, "Using Direct3D 11: default device");
	}

	// Hook into debug information
	ID3D11Debug *d3d_debug = nullptr;
	if (SUCCEEDED(d3d_device->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3d_debug))) {
		d3d_info = nullptr;
		if (SUCCEEDED(d3d_debug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&d3d_info))) {
			D3D11_MESSAGE_ID hide[] = {
				D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
				(D3D11_MESSAGE_ID)351,
			};

			D3D11_INFO_QUEUE_FILTER filter = {};
			filter.DenyList.NumIDs = _countof(hide);
			filter.DenyList.pIDList = hide;
			d3d_info->ClearStorageFilter();
			d3d_info->AddStorageFilterEntries(&filter);
		}
		d3d_debug->Release();
	}

	D3D11_RASTERIZER_DESC desc_rasterizer = {};
	desc_rasterizer.FillMode = D3D11_FILL_SOLID;
	desc_rasterizer.CullMode = D3D11_CULL_BACK;
	desc_rasterizer.FrontCounterClockwise = true;
	desc_rasterizer.DepthClipEnable       = true;
	desc_rasterizer.MultisampleEnable     = true;
	d3d_device->CreateRasterizerState(&desc_rasterizer, &d3d_rasterstate);
	
	D3D11_DEPTH_STENCIL_DESC desc_depthstate = {};
	desc_depthstate.DepthEnable    = true;
	desc_depthstate.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	desc_depthstate.DepthFunc      = D3D11_COMPARISON_LESS;
	desc_depthstate.StencilEnable    = false;
	desc_depthstate.StencilReadMask  = 0xFF;
	desc_depthstate.StencilWriteMask = 0xFF;
	desc_depthstate.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
	desc_depthstate.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	desc_depthstate.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_KEEP;
	desc_depthstate.FrontFace.StencilFunc        = D3D11_COMPARISON_ALWAYS;
	desc_depthstate.BackFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
	desc_depthstate.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	desc_depthstate.BackFace.StencilPassOp      = D3D11_STENCIL_OP_KEEP;
	desc_depthstate.BackFace.StencilFunc        = D3D11_COMPARISON_ALWAYS;
	d3d_device->CreateDepthStencilState(&desc_depthstate, &d3d_depthstate);

	// This sets the default rasterize, depth_stencil, topology mode, etc.
	skg_draw_begin();

	return 1;
}

///////////////////////////////////////////

void skg_shutdown() {
	if (d3d_rasterstate) { d3d_rasterstate->Release(); d3d_rasterstate = nullptr; }
	if (d3d_depthstate ) { d3d_depthstate ->Release(); d3d_depthstate  = nullptr; }
	if (d3d_info       ) { d3d_info       ->Release(); d3d_info        = nullptr; }
	if (d3d_context    ) { d3d_context    ->Release(); d3d_context     = nullptr; }
	if (d3d_device     ) { d3d_device     ->Release(); d3d_device      = nullptr; }
}

///////////////////////////////////////////

void skg_draw_begin() {
	d3d_context->RSSetState            (d3d_rasterstate);
	d3d_context->OMSetDepthStencilState(d3d_depthstate, 1);
	d3d_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

///////////////////////////////////////////

skg_platform_data_t skg_get_platform_data() {
	skg_platform_data_t result = {};
	result._d3d11_device = d3d_device;

	return result;
}

///////////////////////////////////////////

bool skg_capability(skg_cap_ capability) {
	switch (capability) {
	case skg_cap_tex_layer_select: {
		D3D11_FEATURE_DATA_D3D11_OPTIONS3 options;
		d3d_device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS3, &options, sizeof(options));
		return options.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer;
	} break;
	case skg_cap_wireframe: return true;
	default: return false;
	}
}

///////////////////////////////////////////

void skg_tex_target_bind(skg_tex_t *render_target) {
	d3d_active_rendertarget = render_target;

	if (render_target == nullptr) {
		d3d_context->OMSetRenderTargets(0, nullptr, nullptr);
		return;
	}
	if (render_target->type != skg_tex_type_rendertarget)
		return;

	D3D11_VIEWPORT viewport = CD3D11_VIEWPORT(0.f, 0.f, (float)render_target->width, (float)render_target->height);
	d3d_context->RSSetViewports(1, &viewport);
	d3d_context->OMSetRenderTargets(1, &render_target->_target_view, render_target->_depth_view);
}

///////////////////////////////////////////

void skg_target_clear(bool depth, const float *clear_color_4) {
	if (!d3d_active_rendertarget) return;
	if (clear_color_4)
		d3d_context->ClearRenderTargetView(d3d_active_rendertarget->_target_view, clear_color_4);
	if (depth && d3d_active_rendertarget->_depth_view) {
		UINT clear_flags = D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL;
		d3d_context->ClearDepthStencilView(d3d_active_rendertarget->_depth_view, clear_flags, 1.0f, 0);
	}
}

///////////////////////////////////////////

skg_tex_t *skg_tex_target_get() {
	return d3d_active_rendertarget;
}

///////////////////////////////////////////

void skg_draw(int32_t index_start, int32_t index_base, int32_t index_count, int32_t instance_count) {
	d3d_context->DrawIndexedInstanced(index_count, instance_count, index_start, index_base, 0);
}

///////////////////////////////////////////

void skg_compute(uint32_t thread_count_x, uint32_t thread_count_y, uint32_t thread_count_z) {
	d3d_context->Dispatch(thread_count_x, thread_count_y, thread_count_z);
}

///////////////////////////////////////////

void skg_viewport(const int32_t *xywh) {
	D3D11_VIEWPORT viewport = CD3D11_VIEWPORT((float)xywh[0], (float)xywh[1], (float)xywh[2], (float)xywh[3]);
	d3d_context->RSSetViewports(1, &viewport);
}

///////////////////////////////////////////

void skg_viewport_get(int32_t *out_xywh) {
	uint32_t       count = 1;
	D3D11_VIEWPORT viewport;
	d3d_context->RSGetViewports(&count, &viewport);
	out_xywh[0] = (int32_t)viewport.TopLeftX;
	out_xywh[1] = (int32_t)viewport.TopLeftY;
	out_xywh[2] = (int32_t)viewport.Width;
	out_xywh[3] = (int32_t)viewport.Height;
}

///////////////////////////////////////////

void skg_scissor(const int32_t *xywh) {
	D3D11_RECT rect = {xywh[0], xywh[1], xywh[0]+xywh[2], xywh[1]+xywh[3]};
	d3d_context->RSSetScissorRects(1, &rect);
}

///////////////////////////////////////////

skg_buffer_t skg_buffer_create(const void *data, uint32_t size_count, uint32_t size_stride, skg_buffer_type_ type, skg_use_ use) {
	skg_buffer_t result = {};
	result.use    = use;
	result.type   = type;
	result.stride = size_stride;

	D3D11_SUBRESOURCE_DATA buffer_data = { data };
	D3D11_BUFFER_DESC      buffer_desc = {};
	buffer_desc.ByteWidth           = size_count * size_stride;
	buffer_desc.StructureByteStride = size_stride;
	buffer_desc.Usage               = D3D11_USAGE_DEFAULT;

	if (use & skg_use_dynamic) {
		buffer_desc.Usage          = D3D11_USAGE_DYNAMIC;
		buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}

	if (use & skg_use_compute_write || use & skg_use_compute_read) {
		buffer_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE; 
		buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	}

	switch (type) {
	case skg_buffer_type_vertex:   buffer_desc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;   break;
	case skg_buffer_type_index:    buffer_desc.BindFlags |= D3D11_BIND_INDEX_BUFFER;    break;
	case skg_buffer_type_constant: buffer_desc.BindFlags |= D3D11_BIND_CONSTANT_BUFFER; break;
	case skg_buffer_type_compute:  break;
	}
	if (FAILED(d3d_device->CreateBuffer(&buffer_desc, data == nullptr ? nullptr : &buffer_data, &result._buffer))) {
		skg_log(skg_log_critical, "CreateBuffer failed!");
		return {};
	}

	if (use & skg_use_compute_write) {
		D3D11_UNORDERED_ACCESS_VIEW_DESC view = {};
		view.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		view.Format        = DXGI_FORMAT_UNKNOWN;
		view.Buffer.FirstElement = 0;
		view.Buffer.NumElements  = size_count; 

		if(FAILED(d3d_device->CreateUnorderedAccessView( result._buffer, &view, &result._unordered ))) {
			skg_log(skg_log_critical, "CreateUnorderedAccessView failed!");
			skg_buffer_destroy(&result);
			return {};
		}
	} 
	if (use & skg_use_compute_read) {
		D3D11_SHADER_RESOURCE_VIEW_DESC view = {};
		view.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
		view.Format        = DXGI_FORMAT_UNKNOWN;
		view.BufferEx.FirstElement = 0;
		view.BufferEx.NumElements  = size_count;

		if (FAILED(d3d_device->CreateShaderResourceView(result._buffer, &view, &result._resource))) {
			skg_log(skg_log_critical, "CreateShaderResourceView failed!");
			skg_buffer_destroy(&result);
			return {};
		}
	} 
	return result;
}

///////////////////////////////////////////

bool skg_buffer_is_valid(const skg_buffer_t *buffer) {
	return buffer->_buffer != nullptr;
}

///////////////////////////////////////////

void skg_buffer_set_contents(skg_buffer_t *buffer, const void *data, uint32_t size_bytes) {
	if (buffer->use != skg_use_dynamic) {
		skg_log(skg_log_warning, "Attempting to dynamically set contents of a static buffer!");
		return;
	}

	D3D11_MAPPED_SUBRESOURCE resource;
	if (SUCCEEDED(d3d_context->Map(buffer->_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource))) {
		memcpy(resource.pData, data, size_bytes);
		d3d_context->Unmap(buffer->_buffer, 0);
	} else {
		skg_log(skg_log_critical, "Failed to set contents of buffer, may not be using a writeable buffer type?");
	}
}

///////////////////////////////////////////

void skg_buffer_get_contents(const skg_buffer_t *buffer, void *ref_buffer, uint32_t buffer_size) {
	ID3D11Buffer* cpu_buff = nullptr;

	D3D11_BUFFER_DESC desc = {};
	buffer->_buffer->GetDesc( &desc );
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.Usage          = D3D11_USAGE_STAGING;
	desc.BindFlags      = 0;
	desc.MiscFlags      = 0;
	if (FAILED(d3d_device->CreateBuffer(&desc, nullptr, &cpu_buff)) ) {
		skg_log(skg_log_critical, "Couldn't create a tep buffer for copy!");
		return;
	}
	d3d_context->CopyResource( cpu_buff, buffer->_buffer );

	D3D11_MAPPED_SUBRESOURCE resource;
	if (SUCCEEDED(d3d_context->Map(cpu_buff, 0, D3D11_MAP_READ, 0, &resource))) {
		memcpy(ref_buffer, resource.pData, buffer_size);
		d3d_context->Unmap(cpu_buff, 0);
	} else {
		memset(ref_buffer, 0, buffer_size);
		skg_log(skg_log_critical, "Failed to get contents of buffer!");
	}
	cpu_buff->Release();
}

///////////////////////////////////////////

void skg_buffer_clear(skg_bind_t bind) {
	if (bind.register_type == skg_register_readwrite) {
		ID3D11UnorderedAccessView *null_uav = nullptr;
		d3d_context->CSSetUnorderedAccessViews(bind.slot, 1, &null_uav, nullptr);
	}
}

///////////////////////////////////////////
void skg_buffer_bind(const skg_buffer_t *buffer, skg_bind_t bind, uint32_t offset) {
	switch (bind.register_type) {
	case skg_register_index:  d3d_context->IASetIndexBuffer(buffer->_buffer, DXGI_FORMAT_R32_UINT, offset); break;
	case skg_register_vertex: d3d_context->IASetVertexBuffers(bind.slot, 1, &buffer->_buffer, &buffer->stride, &offset); break;
	case skg_register_constant: {
#if !defined(NDEBUG)
		if (buffer->type != skg_buffer_type_constant) skg_log(skg_log_critical, "Attempting to bind the wrong buffer type to a constant register! Use skg_buffer_type_constant");
#endif
		if (bind.stage_bits & skg_stage_vertex ) d3d_context->VSSetConstantBuffers(bind.slot, 1, &buffer->_buffer);
		if (bind.stage_bits & skg_stage_pixel  ) d3d_context->PSSetConstantBuffers(bind.slot, 1, &buffer->_buffer);
		if (bind.stage_bits & skg_stage_compute) d3d_context->CSSetConstantBuffers(bind.slot, 1, &buffer->_buffer);
	} break;
	case skg_register_resource: {
#if !defined(NDEBUG)
		if (buffer->type != skg_buffer_type_compute) skg_log(skg_log_critical, "Attempting to bind the wrong buffer type to a resource register! Use skg_buffer_type_compute");
#endif
		if (bind.stage_bits & skg_stage_vertex ) d3d_context->VSSetShaderResources(bind.slot, 1, &buffer->_resource);
		if (bind.stage_bits & skg_stage_pixel  ) d3d_context->PSSetShaderResources(bind.slot, 1, &buffer->_resource);
		if (bind.stage_bits & skg_stage_compute) d3d_context->CSSetShaderResources(bind.slot, 1, &buffer->_resource);
	} break;
	case skg_register_readwrite: {
#if !defined(NDEBUG)
		if (buffer->type != skg_buffer_type_compute) skg_log(skg_log_critical, "Attempting to bind the wrong buffer type to a UAV register! Use skg_buffer_type_compute");
#endif
		if (bind.stage_bits & skg_stage_compute) d3d_context->CSSetUnorderedAccessViews(bind.slot, 1, &buffer->_unordered, nullptr);
	} break;
	}
}

///////////////////////////////////////////

void skg_buffer_destroy(skg_buffer_t *buffer) {
	if (buffer->_buffer) buffer->_buffer->Release();
	*buffer = {};
}

///////////////////////////////////////////

skg_mesh_t skg_mesh_create(const skg_buffer_t *vert_buffer, const skg_buffer_t *ind_buffer) {
	skg_mesh_t result = {};
	result._ind_buffer  = ind_buffer  ? ind_buffer ->_buffer : nullptr;
	result._vert_buffer = vert_buffer ? vert_buffer->_buffer : nullptr;
	if (result._ind_buffer ) result._ind_buffer ->AddRef();
	if (result._vert_buffer) result._vert_buffer->AddRef();

	return result;
}

///////////////////////////////////////////

void skg_mesh_set_verts(skg_mesh_t *mesh, const skg_buffer_t *vert_buffer) {
	if (mesh->_vert_buffer) mesh->_vert_buffer->Release();
	mesh->_vert_buffer = vert_buffer->_buffer;
	if (mesh->_vert_buffer) mesh->_vert_buffer->AddRef();
}

///////////////////////////////////////////

void skg_mesh_set_inds(skg_mesh_t *mesh, const skg_buffer_t *ind_buffer) {
	if (mesh->_ind_buffer) mesh->_ind_buffer->Release();
	mesh->_ind_buffer = ind_buffer->_buffer;
	if (mesh->_ind_buffer) mesh->_ind_buffer->AddRef();
}

///////////////////////////////////////////

void skg_mesh_bind(const skg_mesh_t *mesh) {
	UINT strides[] = { sizeof(skg_vert_t) };
	UINT offsets[] = { 0 };
	d3d_context->IASetVertexBuffers(0, 1, &mesh->_vert_buffer, strides, offsets);
	d3d_context->IASetIndexBuffer  (mesh->_ind_buffer, DXGI_FORMAT_R32_UINT, 0);
}

///////////////////////////////////////////

void skg_mesh_destroy(skg_mesh_t *mesh) {
	if (mesh->_ind_buffer ) mesh->_ind_buffer ->Release();
	if (mesh->_vert_buffer) mesh->_vert_buffer->Release();
	*mesh = {};
}

///////////////////////////////////////////

#include <stdio.h>
skg_shader_stage_t skg_shader_stage_create(const void *file_data, size_t shader_size, skg_stage_ type) {
	skg_shader_stage_t result = {};
	result.type = type;

	DWORD flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#if !defined(NDEBUG)
	flags |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#else
	flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	ID3DBlob   *compiled = nullptr;
	const void *buffer;
	size_t      buffer_size;
	if (shader_size >= 4 && memcmp(file_data, "DXBC", 4) == 0) {
		buffer      = file_data;
		buffer_size = shader_size;
	} else {
		ID3DBlob *errors;
		const char *entrypoint = "", *target = "";
		switch (type) {
			case skg_stage_vertex:  entrypoint = "vs"; target = "vs_5_0"; break;
			case skg_stage_pixel:   entrypoint = "ps"; target = "ps_5_0"; break;
			case skg_stage_compute: entrypoint = "cs"; target = "cs_5_0"; break; }
		if (FAILED(D3DCompile(file_data, shader_size, nullptr, nullptr, nullptr, entrypoint, target, flags, 0, &compiled, &errors))) {
			skg_log(skg_log_warning, "D3DCompile failed:");
			skg_log(skg_log_warning, (char *)errors->GetBufferPointer());
		}
		if (errors) errors->Release();

		buffer      = compiled->GetBufferPointer();
		buffer_size = compiled->GetBufferSize();
	}

	switch(type) {
	case skg_stage_vertex  : d3d_device->CreateVertexShader (buffer, buffer_size, nullptr, (ID3D11VertexShader **)&result._shader); break;
	case skg_stage_pixel   : d3d_device->CreatePixelShader  (buffer, buffer_size, nullptr, (ID3D11PixelShader  **)&result._shader); break;
	case skg_stage_compute : d3d_device->CreateComputeShader(buffer, buffer_size, nullptr, (ID3D11ComputeShader**)&result._shader); break;
	}

	if (type == skg_stage_vertex) {
		// Describe how our mesh is laid out in memory
		D3D11_INPUT_ELEMENT_DESC vert_desc[] = {
			{"SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"NORMAL",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR" ,      0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0} };
		d3d_device->CreateInputLayout(vert_desc, (UINT)_countof(vert_desc), buffer, buffer_size, &result._layout);
	}
	if (compiled) compiled->Release();

	return result;
}

///////////////////////////////////////////

void skg_shader_stage_destroy(skg_shader_stage_t *shader) {
	switch(shader->type) {
	case skg_stage_vertex  : ((ID3D11VertexShader *)shader->_shader)->Release(); shader->_layout->Release(); break;
	case skg_stage_pixel   : ((ID3D11PixelShader  *)shader->_shader)->Release(); break;
	case skg_stage_compute : ((ID3D11ComputeShader*)shader->_shader)->Release(); break;
	}
}

///////////////////////////////////////////
// skg_shader_t                          //
///////////////////////////////////////////

skg_shader_t skg_shader_create_manual(skg_shader_meta_t *meta, skg_shader_stage_t v_shader, skg_shader_stage_t p_shader, skg_shader_stage_t c_shader) {
	if (v_shader._shader == nullptr && p_shader._shader == nullptr && c_shader._shader == nullptr) {
		char text[290];
		snprintf(text, sizeof(text), "Shader '%s' has no valid stages!", meta->name);
		skg_log(skg_log_warning, text);
		return {};
	}

	skg_shader_t result = {};
	result.meta    = meta;
	if (v_shader._shader) result._vertex  = (ID3D11VertexShader *)v_shader._shader;
	if (v_shader._layout) result._layout  = v_shader._layout;
	if (p_shader._shader) result._pixel   = (ID3D11PixelShader  *)p_shader._shader;
	if (c_shader._shader) result._compute = (ID3D11ComputeShader*)c_shader._shader;
	skg_shader_meta_reference(result.meta);
	if (result._vertex ) result._vertex ->AddRef();
	if (result._layout ) result._layout ->AddRef();
	if (result._pixel  ) result._pixel  ->AddRef();
	if (result._compute) result._compute->AddRef();

	return result;
}

///////////////////////////////////////////

bool skg_shader_is_valid(const skg_shader_t *shader) {
	return shader->meta
		&& (shader->_vertex && shader->_pixel) || shader->_compute;
}

///////////////////////////////////////////

void skg_shader_compute_bind(const skg_shader_t *shader) {
	if (shader) d3d_context->CSSetShader(shader->_compute, nullptr, 0);
	else        d3d_context->CSSetShader(nullptr, nullptr, 0);
}

///////////////////////////////////////////

void skg_shader_destroy(skg_shader_t *shader) {
	skg_shader_meta_release(shader->meta);
	if (shader->_vertex ) shader->_vertex ->Release();
	if (shader->_layout ) shader->_layout ->Release();
	if (shader->_pixel  ) shader->_pixel  ->Release();
	if (shader->_compute) shader->_compute->Release();
	*shader = {};
}

///////////////////////////////////////////
// skg_pipeline                          //
///////////////////////////////////////////

void skg_pipeline_update_blend(skg_pipeline_t *pipeline) {
	if (pipeline->_blend) pipeline->_blend->Release();

	D3D11_BLEND_DESC desc_blend = {};
	desc_blend.AlphaToCoverageEnable  = false;
	desc_blend.IndependentBlendEnable = false;
	desc_blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	switch (pipeline->transparency) {
	case skg_transparency_blend:
		desc_blend.RenderTarget[0].BlendEnable           = true;
		desc_blend.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
		desc_blend.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
		desc_blend.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
		desc_blend.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
		desc_blend.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
		desc_blend.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
		break;
	case skg_transparency_add:
		desc_blend.RenderTarget[0].BlendEnable           = true;
		desc_blend.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
		desc_blend.RenderTarget[0].DestBlend             = D3D11_BLEND_ONE;
		desc_blend.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
		desc_blend.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
		desc_blend.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ONE;
		desc_blend.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
		break;
	}

	d3d_device->CreateBlendState(&desc_blend, &pipeline->_blend);
}

///////////////////////////////////////////

void skg_pipeline_update_rasterizer(skg_pipeline_t *pipeline) {
	if (pipeline->_rasterize) pipeline->_rasterize->Release();

	D3D11_RASTERIZER_DESC desc_rasterizer = {};
	desc_rasterizer.FillMode              = pipeline->wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
	desc_rasterizer.FrontCounterClockwise = true;
	desc_rasterizer.ScissorEnable         = pipeline->scissor;
	desc_rasterizer.DepthClipEnable       = true;
	switch (pipeline->cull) {
	case skg_cull_none:  desc_rasterizer.CullMode = D3D11_CULL_NONE;  break;
	case skg_cull_front: desc_rasterizer.CullMode = D3D11_CULL_FRONT; break;
	case skg_cull_back:  desc_rasterizer.CullMode = D3D11_CULL_BACK;  break;
	}
	d3d_device->CreateRasterizerState(&desc_rasterizer, &pipeline->_rasterize);
}

///////////////////////////////////////////

void skg_pipeline_update_depth(skg_pipeline_t *pipeline) {
	if (pipeline->_depth) pipeline->_depth->Release();

	D3D11_COMPARISON_FUNC comparison = D3D11_COMPARISON_LESS;
	switch (pipeline->depth_test) {
	case skg_depth_test_always:        comparison = D3D11_COMPARISON_ALWAYS;        break;
	case skg_depth_test_equal:         comparison = D3D11_COMPARISON_EQUAL;         break;
	case skg_depth_test_greater:       comparison = D3D11_COMPARISON_GREATER;       break;
	case skg_depth_test_greater_or_eq: comparison = D3D11_COMPARISON_GREATER_EQUAL; break;
	case skg_depth_test_less:          comparison = D3D11_COMPARISON_LESS;          break;
	case skg_depth_test_less_or_eq:    comparison = D3D11_COMPARISON_LESS_EQUAL;    break;
	case skg_depth_test_never:         comparison = D3D11_COMPARISON_NEVER;         break;
	case skg_depth_test_not_equal:     comparison = D3D11_COMPARISON_NOT_EQUAL;     break;
	}

	D3D11_DEPTH_STENCIL_DESC desc_depthstate = {};
	desc_depthstate.DepthEnable    = pipeline->depth_write != false || pipeline->depth_test != skg_depth_test_always;
	desc_depthstate.DepthWriteMask = pipeline->depth_write ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	desc_depthstate.DepthFunc      = comparison;
	desc_depthstate.StencilEnable    = false;
	desc_depthstate.StencilReadMask  = 0xFF;
	desc_depthstate.StencilWriteMask = 0xFF;
	desc_depthstate.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
	desc_depthstate.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	desc_depthstate.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_KEEP;
	desc_depthstate.FrontFace.StencilFunc        = D3D11_COMPARISON_ALWAYS;
	desc_depthstate.BackFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
	desc_depthstate.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	desc_depthstate.BackFace.StencilPassOp      = D3D11_STENCIL_OP_KEEP;
	desc_depthstate.BackFace.StencilFunc        = D3D11_COMPARISON_ALWAYS;
	d3d_device->CreateDepthStencilState(&desc_depthstate, &pipeline->_depth);
}

///////////////////////////////////////////

skg_pipeline_t skg_pipeline_create(skg_shader_t *shader) {
	skg_pipeline_t result = {};
	result.transparency = skg_transparency_none;
	result.cull         = skg_cull_back;
	result.wireframe    = false;
	result.depth_write  = true;
	result.depth_test   = skg_depth_test_less;
	result.meta         = shader->meta;
	result._vertex      = shader->_vertex;
	result._pixel       = shader->_pixel;
	result._layout      = shader->_layout;
	if (result._vertex) result._vertex->AddRef();
	if (result._layout) result._layout->AddRef();
	if (result._pixel ) result._pixel ->AddRef();
	skg_shader_meta_reference(shader->meta);

	skg_pipeline_update_blend     (&result);
	skg_pipeline_update_rasterizer(&result);
	skg_pipeline_update_depth     (&result);
	return result;
}

///////////////////////////////////////////

void skg_pipeline_bind(const skg_pipeline_t *pipeline) {
	d3d_context->OMSetBlendState       (pipeline->_blend,  nullptr, 0xFFFFFFFF);
	d3d_context->OMSetDepthStencilState(pipeline->_depth,  0);
	d3d_context->RSSetState            (pipeline->_rasterize);
	d3d_context->VSSetShader           (pipeline->_vertex, nullptr, 0);
	d3d_context->PSSetShader           (pipeline->_pixel,  nullptr, 0);
	d3d_context->IASetInputLayout      (pipeline->_layout);
}

///////////////////////////////////////////

void skg_pipeline_set_transparency(skg_pipeline_t *pipeline, skg_transparency_ transparency) {
	if (pipeline->transparency != transparency) {
		pipeline->transparency  = transparency;
		skg_pipeline_update_blend(pipeline);
	}
}

///////////////////////////////////////////

void skg_pipeline_set_cull(skg_pipeline_t *pipeline, skg_cull_ cull) {
	if (pipeline->cull != cull) {
		pipeline->cull  = cull;
		skg_pipeline_update_rasterizer(pipeline);
	}
}

///////////////////////////////////////////

void skg_pipeline_set_depth_write(skg_pipeline_t *pipeline, bool write) {
	if (pipeline->depth_write != write) {
		pipeline->depth_write = write;
		skg_pipeline_update_depth(pipeline);
	}
}

///////////////////////////////////////////

void skg_pipeline_set_depth_test (skg_pipeline_t *pipeline, skg_depth_test_ test) {
	if (pipeline->depth_test != test) {
		pipeline->depth_test = test;
		skg_pipeline_update_depth(pipeline);
	}
}

///////////////////////////////////////////

void skg_pipeline_set_wireframe(skg_pipeline_t *pipeline, bool wireframe) {
	if (pipeline->wireframe != wireframe) {
		pipeline->wireframe  = wireframe;
		skg_pipeline_update_rasterizer(pipeline);
	}
}

///////////////////////////////////////////

void skg_pipeline_set_scissor(skg_pipeline_t *pipeline, bool enable) {
	if (pipeline->scissor != enable) {
		pipeline->scissor  = enable;
		skg_pipeline_update_rasterizer(pipeline);
	}
}

///////////////////////////////////////////

skg_transparency_ skg_pipeline_get_transparency(const skg_pipeline_t *pipeline) {
	return pipeline->transparency;
}

///////////////////////////////////////////

skg_cull_ skg_pipeline_get_cull(const skg_pipeline_t *pipeline) {
	return pipeline->cull;
}

///////////////////////////////////////////

bool skg_pipeline_get_wireframe(const skg_pipeline_t *pipeline) {
	return pipeline->wireframe;
}

///////////////////////////////////////////

bool skg_pipeline_get_depth_write(const skg_pipeline_t *pipeline) {
	return pipeline->depth_write;
}

///////////////////////////////////////////

skg_depth_test_ skg_pipeline_get_depth_test(const skg_pipeline_t *pipeline) {
	return pipeline->depth_test;
}

///////////////////////////////////////////

bool skg_pipeline_get_scissor(const skg_pipeline_t *pipeline) {
	return pipeline->scissor;
}

///////////////////////////////////////////

void skg_pipeline_destroy(skg_pipeline_t *pipeline) {
	skg_shader_meta_release(pipeline->meta);
	if (pipeline->_blend    ) pipeline->_blend    ->Release();
	if (pipeline->_rasterize) pipeline->_rasterize->Release();
	if (pipeline->_depth    ) pipeline->_depth    ->Release();
	if (pipeline->_vertex   ) pipeline->_vertex   ->Release();
	if (pipeline->_layout   ) pipeline->_layout   ->Release();
	if (pipeline->_pixel    ) pipeline->_pixel    ->Release();
	*pipeline = {};
}

///////////////////////////////////////////
// skg_swapchain                         //
///////////////////////////////////////////

skg_swapchain_t skg_swapchain_create(void *hwnd, skg_tex_fmt_ format, skg_tex_fmt_ depth_format, int32_t requested_width, int32_t requested_height) {
	skg_swapchain_t result = {};
	result.width  = requested_width;
	result.height = requested_height;

	DXGI_SWAP_CHAIN_DESC1 swapchain_desc = { };
	swapchain_desc.BufferCount = 2;
	swapchain_desc.Width       = result.width;
	swapchain_desc.Height      = result.height;
	swapchain_desc.Format      = (DXGI_FORMAT)skg_tex_fmt_to_native(format);
	swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchain_desc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchain_desc.AlphaMode   = DXGI_ALPHA_MODE_IGNORE;
	swapchain_desc.SampleDesc.Count = 1;

	IDXGIDevice2  *dxgi_device;  d3d_device  ->QueryInterface(__uuidof(IDXGIDevice2),  (void **)&dxgi_device);
	IDXGIAdapter  *dxgi_adapter; dxgi_device ->GetParent     (__uuidof(IDXGIAdapter),  (void **)&dxgi_adapter);
	IDXGIFactory2 *dxgi_factory; dxgi_adapter->GetParent     (__uuidof(IDXGIFactory2), (void **)&dxgi_factory);

	if (FAILED(dxgi_factory->CreateSwapChainForHwnd(d3d_device, (HWND)hwnd, &swapchain_desc, nullptr, nullptr, &result._swapchain))) {
		skg_log(skg_log_critical, "sk_gpu couldn't create swapchain!");
		result = {};
		return result;
	}

	// Set the target view to an sRGB format for proper presentation of 
	// linear color data.
	skg_tex_fmt_ target_fmt = format;
	switch (format) {
	case skg_tex_fmt_bgra32_linear: target_fmt = skg_tex_fmt_bgra32; break;
	case skg_tex_fmt_rgba32_linear: target_fmt = skg_tex_fmt_rgba32; break;
	}

	ID3D11Texture2D *back_buffer;
	result._swapchain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
	result._target = skg_tex_create_from_existing(back_buffer, skg_tex_type_rendertarget, target_fmt, result.width, result.height, 1);
	if (depth_format != skg_tex_fmt_none) {
		result._depth = skg_tex_create(skg_tex_type_depth, skg_use_static, depth_format, skg_mip_none);
		skg_tex_set_contents(&result._depth, nullptr, result.width, result.height);
		skg_tex_attach_depth(&result._target, &result._depth);
	}
	back_buffer->Release();

	dxgi_factory->Release();
	dxgi_adapter->Release();
	dxgi_device ->Release();

	return result;
}

///////////////////////////////////////////

void skg_swapchain_resize(skg_swapchain_t *swapchain, int32_t width, int32_t height) {
	if (swapchain->_swapchain == nullptr || (width == swapchain->width && height == swapchain->height))
		return;

	skg_tex_fmt_ target_fmt = swapchain->_target.format;
	skg_tex_fmt_ depth_fmt  = swapchain->_depth .format;
	skg_tex_destroy(&swapchain->_target);
	skg_tex_destroy(&swapchain->_depth);

	swapchain->width  = width;
	swapchain->height = height;
	swapchain->_swapchain->ResizeBuffers(0, (UINT)width, (UINT)height, DXGI_FORMAT_UNKNOWN, 0);

	ID3D11Texture2D *back_buffer;
	swapchain->_swapchain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
	swapchain->_target = skg_tex_create_from_existing(back_buffer, skg_tex_type_rendertarget, target_fmt, width, height, 1);
	if (depth_fmt != skg_tex_fmt_none) {
		swapchain->_depth = skg_tex_create(skg_tex_type_depth, skg_use_static, depth_fmt, skg_mip_none);
		skg_tex_set_contents(&swapchain->_depth, nullptr, width, height);
		skg_tex_attach_depth(&swapchain->_target, &swapchain->_depth);
	}
	back_buffer->Release();
}

///////////////////////////////////////////

void skg_swapchain_present(skg_swapchain_t *swapchain) {
	swapchain->_swapchain->Present(1, 0);
}

///////////////////////////////////////////

void skg_swapchain_bind(skg_swapchain_t *swapchain) {
	skg_tex_target_bind(swapchain->_target.format != 0 ? &swapchain->_target : nullptr);
}

///////////////////////////////////////////

void skg_swapchain_destroy(skg_swapchain_t *swapchain) {
	skg_tex_destroy(&swapchain->_target);
	skg_tex_destroy(&swapchain->_depth);
	swapchain->_swapchain->Release();
	*swapchain = {};
}

///////////////////////////////////////////

skg_tex_t skg_tex_create_from_existing(void *native_tex, skg_tex_type_ type, skg_tex_fmt_ override_format, int32_t width, int32_t height, int32_t array_count) {
	skg_tex_t result = {};
	result.type     = type;
	result.use      = skg_use_static;
	result._texture = (ID3D11Texture2D *)native_tex;
	result._texture->AddRef();

	// Get information about the image!
	D3D11_TEXTURE2D_DESC color_desc;
	result._texture->GetDesc(&color_desc);
	result.width       = color_desc.Width;
	result.height      = color_desc.Height;
	result.array_count = color_desc.ArraySize;
	result.multisample = color_desc.SampleDesc.Count;
	result.format      = override_format != 0 ? override_format : skg_tex_fmt_from_native(color_desc.Format);
	skg_tex_make_view(&result, color_desc.MipLevels, 0, color_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE);

	return result;
}

///////////////////////////////////////////

skg_tex_t skg_tex_create_from_layer(void *native_tex, skg_tex_type_ type, skg_tex_fmt_ override_format, int32_t width, int32_t height, int32_t array_layer) {
	skg_tex_t result = {};
	result.type     = type;
	result.use      = skg_use_static;
	result._texture = (ID3D11Texture2D *)native_tex;
	result._texture->AddRef();

	// Get information about the image!
	D3D11_TEXTURE2D_DESC color_desc;
	result._texture->GetDesc(&color_desc);
	result.width       = color_desc.Width;
	result.height      = color_desc.Height;
	result.array_count = 1;
	result.multisample = color_desc.SampleDesc.Count;
	result.format      = override_format != 0 ? override_format : skg_tex_fmt_from_native(color_desc.Format);
	skg_tex_make_view(&result, color_desc.MipLevels, array_layer, color_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE);

	return result;
}

///////////////////////////////////////////

skg_tex_t skg_tex_create(skg_tex_type_ type, skg_use_ use, skg_tex_fmt_ format, skg_mip_ mip_maps) {
	skg_tex_t result = {};
	result.type   = type;
	result.use    = use;
	result.format = format;
	result.mips   = mip_maps;

	if (use == skg_use_dynamic && mip_maps == skg_mip_generate)
		skg_log(skg_log_warning, "Dynamic textures don't support mip-maps!");

	return result;
}

///////////////////////////////////////////

void skg_tex_copy_to(const skg_tex_t *tex, skg_tex_t *destination) {
	if (destination->width != tex->width || destination->height != tex->height) {
		skg_tex_set_contents_arr(destination, nullptr, tex->array_count, tex->width, tex->height, tex->multisample);
	}

	if (tex->multisample > 1) {
		d3d_context->ResolveSubresource(destination->_texture, 0, tex->_texture, 0, (DXGI_FORMAT)skg_tex_fmt_to_native(tex->format));
	} else {
		d3d_context->CopyResource(destination->_texture, tex->_texture);
	}
}

///////////////////////////////////////////

void skg_tex_copy_to_swapchain(const skg_tex_t *tex, skg_swapchain_t *destination) {
	skg_tex_copy_to(tex, &destination->_target);
}

///////////////////////////////////////////

bool skg_tex_is_valid(const skg_tex_t *tex) {
	return tex->_texture != nullptr;
}

///////////////////////////////////////////

void skg_tex_attach_depth(skg_tex_t *tex, skg_tex_t *depth) {
	if (depth->type == skg_tex_type_depth) {
		if (tex->_depth_view) tex->_depth_view->Release();
		tex->_depth_view = depth->_depth_view;
		tex->_depth_view->AddRef();
	} else {
		skg_log(skg_log_warning, "Can't bind a depth texture to a non-rendertarget");
	}
}

///////////////////////////////////////////

void skg_tex_settings(skg_tex_t *tex, skg_tex_address_ address, skg_tex_sample_ sample, int32_t anisotropy) {
	if (tex->_sampler)
		tex->_sampler->Release();

	D3D11_TEXTURE_ADDRESS_MODE mode;
	switch (address) {
	case skg_tex_address_clamp:  mode = D3D11_TEXTURE_ADDRESS_CLAMP;  break;
	case skg_tex_address_repeat: mode = D3D11_TEXTURE_ADDRESS_WRAP;   break;
	case skg_tex_address_mirror: mode = D3D11_TEXTURE_ADDRESS_MIRROR; break;
	default: mode = D3D11_TEXTURE_ADDRESS_WRAP;
	}

	D3D11_FILTER filter;
	switch (sample) {
	case skg_tex_sample_linear:     filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; break; // Technically trilinear
	case skg_tex_sample_point:      filter = D3D11_FILTER_MIN_MAG_MIP_POINT;  break;
	case skg_tex_sample_anisotropic:filter = D3D11_FILTER_ANISOTROPIC;        break;
	default: filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	}

	D3D11_SAMPLER_DESC desc_sampler = {};
	desc_sampler.AddressU = mode;
	desc_sampler.AddressV = mode;
	desc_sampler.AddressW = mode;
	desc_sampler.Filter   = filter;
	desc_sampler.MaxAnisotropy  = anisotropy;
	desc_sampler.MaxLOD         = D3D11_FLOAT32_MAX;
	desc_sampler.ComparisonFunc = D3D11_COMPARISON_ALWAYS;

	// D3D will already return the same sampler when provided the same 
	// settings, so we can just lean on that to prevent sampler duplicates :)
	if (FAILED(d3d_device->CreateSamplerState(&desc_sampler, &tex->_sampler)))
		skg_log(skg_log_critical, "Failed to create sampler state!");
}

///////////////////////////////////////////

bool skg_can_make_mips(skg_tex_fmt_ format) {
	switch (format) {
	case skg_tex_fmt_bgra32:
	case skg_tex_fmt_bgra32_linear:
	case skg_tex_fmt_rgba32:
	case skg_tex_fmt_rgba32_linear: 
	case skg_tex_fmt_rgba64u:
	case skg_tex_fmt_rgba64s:
	case skg_tex_fmt_rgba128:
	case skg_tex_fmt_depth32:
	case skg_tex_fmt_r32:
	case skg_tex_fmt_depth16:
	case skg_tex_fmt_r16:
	case skg_tex_fmt_r8: return true;
	default: return false;
	}
}

///////////////////////////////////////////

void skg_make_mips(D3D11_SUBRESOURCE_DATA *tex_mem, const void *curr_data, skg_tex_fmt_ format, int32_t width, int32_t height, uint32_t mip_levels) {
	const void *mip_data = curr_data;
	int32_t     mip_w    = width;
	int32_t     mip_h    = height;
	for (uint32_t m = 1; m < mip_levels; m++) {
		tex_mem[m] = {};
		switch (format) { // When adding a new format here, also add it to skg_can_make_mips
		case skg_tex_fmt_bgra32:
		case skg_tex_fmt_bgra32_linear:
		case skg_tex_fmt_rgba32:
		case skg_tex_fmt_rgba32_linear: 
			skg_downsample_4((uint8_t  *)mip_data, mip_w, mip_h, (uint8_t  **)&tex_mem[m].pSysMem, &mip_w, &mip_h); 
			break;
		case skg_tex_fmt_rgba64u:
			skg_downsample_4((uint16_t *)mip_data, mip_w, mip_h, (uint16_t **)&tex_mem[m].pSysMem, &mip_w, &mip_h);
			break;
		case skg_tex_fmt_rgba64s:
			skg_downsample_4((int16_t  *)mip_data, mip_w, mip_h, (int16_t  **)&tex_mem[m].pSysMem, &mip_w, &mip_h);
			break;
		case skg_tex_fmt_rgba128:
			skg_downsample_4((float    *)mip_data, mip_w, mip_h, (float    **)&tex_mem[m].pSysMem, &mip_w, &mip_h);
			break;
		case skg_tex_fmt_depth32:
		case skg_tex_fmt_r32:
			skg_downsample_1((float    *)mip_data, mip_w, mip_h, (float    **)&tex_mem[m].pSysMem, &mip_w, &mip_h); 
			break;
		case skg_tex_fmt_depth16:
		case skg_tex_fmt_r16:
			skg_downsample_1((uint16_t *)mip_data, mip_w, mip_h, (uint16_t **)&tex_mem[m].pSysMem, &mip_w, &mip_h); 
			break;
		case skg_tex_fmt_r8:
			skg_downsample_1((uint8_t  *)mip_data, mip_w, mip_h, (uint8_t  **)&tex_mem[m].pSysMem, &mip_w, &mip_h); 
			break;
		default: skg_log(skg_log_warning, "Unsupported texture format for mip maps!"); break;
		}
		mip_data = (void*)tex_mem[m].pSysMem;
		tex_mem[m].SysMemPitch = (UINT)(skg_tex_fmt_size(format) * mip_w);
	}
}

///////////////////////////////////////////

bool skg_tex_make_view(skg_tex_t *tex, uint32_t mip_count, uint32_t array_start, bool use_in_shader) {
	DXGI_FORMAT format = (DXGI_FORMAT)skg_tex_fmt_to_native(tex->format);

	if (tex->type != skg_tex_type_depth) {
		D3D11_SHADER_RESOURCE_VIEW_DESC res_desc = {};
		res_desc.Format = format;
		// This struct is a union, but all elements follow the same order in
		// the struct. Texture2DArray is representative of the union with the
		// most data in it, so if we fill it properly, all others should also
		// be filled correctly. *Fingers crossed it stays that way*
		res_desc.Texture2DArray.FirstArraySlice = array_start;
		res_desc.Texture2DArray.ArraySize       = tex->array_count;
		res_desc.Texture2DArray.MipLevels       = mip_count;

		if (tex->type == skg_tex_type_cubemap) {
			res_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		} else if (tex->array_count > 1) {
			res_desc.ViewDimension = tex->multisample > 1
				? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY
				: D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		} else {
			res_desc.ViewDimension = tex->multisample > 1 
				? D3D11_SRV_DIMENSION_TEXTURE2DMS
				: D3D11_SRV_DIMENSION_TEXTURE2D;
		}

		if (use_in_shader && FAILED(d3d_device->CreateShaderResourceView(tex->_texture, &res_desc, &tex->_resource))) {
			skg_log(skg_log_critical, "Create Shader Resource View error!");
			return false;
		}
	} else {
		D3D11_DEPTH_STENCIL_VIEW_DESC stencil_desc = {};
		stencil_desc.Format = format;
		stencil_desc.Texture2DArray.FirstArraySlice = array_start;
		stencil_desc.Texture2DArray.ArraySize       = tex->array_count;
		if (tex->type == skg_tex_type_cubemap || tex->array_count > 1) {
			stencil_desc.ViewDimension = tex->multisample > 1
				? D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY
				: D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		} else {
			stencil_desc.ViewDimension = tex->multisample > 1 
				? D3D11_DSV_DIMENSION_TEXTURE2DMS
				: D3D11_DSV_DIMENSION_TEXTURE2D;
		}
		if (FAILED(d3d_device->CreateDepthStencilView(tex->_texture, &stencil_desc, &tex->_depth_view))) {
			skg_log(skg_log_critical, "Create Depth Stencil View error!");
			return false;
		}
	}

	if (tex->type == skg_tex_type_rendertarget) {
		D3D11_RENDER_TARGET_VIEW_DESC target_desc = {};
		target_desc.Format = format;
		target_desc.Texture2DArray.FirstArraySlice = array_start;
		target_desc.Texture2DArray.ArraySize       = tex->array_count;
		if (tex->type == skg_tex_type_cubemap || tex->array_count > 1) {
			target_desc.ViewDimension = tex->multisample > 1
				? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY
				: D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		} else {
			target_desc.ViewDimension = tex->multisample > 1
				? D3D11_RTV_DIMENSION_TEXTURE2DMS
				: D3D11_RTV_DIMENSION_TEXTURE2D;
		}

		if (FAILED(d3d_device->CreateRenderTargetView(tex->_texture, &target_desc, &tex->_target_view))) {
			skg_log(skg_log_critical, "Create Render Target View error!");
			return false;
		}
	}

	if (tex->use & skg_use_compute_write) {
		D3D11_UNORDERED_ACCESS_VIEW_DESC view = {};
		view.Format = DXGI_FORMAT_UNKNOWN;
		if (tex->type == skg_tex_type_cubemap || tex->array_count > 1) {
			view.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
			view.Texture2DArray.FirstArraySlice = array_start;
			view.Texture2DArray.ArraySize       = tex->array_count;
		} else {
			view.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		}

		if(FAILED(d3d_device->CreateUnorderedAccessView( tex->_texture, &view, &tex->_unordered ))) {
			skg_log(skg_log_critical, "CreateUnorderedAccessView failed!");
			return {};
		}
	} 
	return true;
}

///////////////////////////////////////////

void skg_tex_set_contents(skg_tex_t *tex, const void *data, int32_t width, int32_t height) {
	const void *data_arr[1] = { data };
	return skg_tex_set_contents_arr(tex, data_arr, 1, width, height, 1);
}

///////////////////////////////////////////

void skg_tex_set_contents_arr(skg_tex_t *tex, const void **data_frames, int32_t data_frame_count, int32_t width, int32_t height, int32_t multisample) {
	// Some warning messages
	if (tex->use != skg_use_dynamic && tex->_texture) {
		skg_log(skg_log_warning, "Only dynamic textures can be updated!");
		return;
	}
	if (tex->use == skg_use_dynamic && (tex->mips == skg_mip_generate || data_frame_count > 1)) {
		skg_log(skg_log_warning, "Dynamic textures don't support mip-maps or texture arrays!");
		return;
	}

	tex->width       = width;
	tex->height      = height;
	tex->array_count = data_frame_count;
	tex->multisample = multisample;
	bool mips = 
		   tex->mips == skg_mip_generate
		&& skg_can_make_mips(tex->format)
		&& (width  & (width  - 1)) == 0
		&& (height & (height - 1)) == 0;

	uint32_t mip_levels = (mips ? skg_mip_count(width, height) : 1);
	uint32_t px_size    = skg_tex_fmt_size(tex->format);

	if (tex->_texture == nullptr) {
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width            = width;
		desc.Height           = height;
		desc.MipLevels        = mip_levels;
		desc.ArraySize        = data_frame_count;
		desc.SampleDesc.Count = multisample;
		desc.Format           = (DXGI_FORMAT)skg_tex_fmt_to_native(tex->format);
		desc.BindFlags        = tex->type == skg_tex_type_depth ? D3D11_BIND_DEPTH_STENCIL : D3D11_BIND_SHADER_RESOURCE;
		desc.Usage            = tex->use  == skg_use_dynamic    ? D3D11_USAGE_DYNAMIC      : D3D11_USAGE_DEFAULT;
		desc.CPUAccessFlags   = tex->use  == skg_use_dynamic    ? D3D11_CPU_ACCESS_WRITE   : 0;
		if (tex->type == skg_tex_type_rendertarget) desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
		if (tex->type == skg_tex_type_cubemap     ) desc.MiscFlags  = D3D11_RESOURCE_MISC_TEXTURECUBE;
		if (tex->use  &  skg_use_compute_write    ) desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

		D3D11_SUBRESOURCE_DATA *tex_mem = nullptr;
		if (data_frames != nullptr && data_frames[0] != nullptr) {
			tex_mem = (D3D11_SUBRESOURCE_DATA *)malloc((int64_t)data_frame_count * mip_levels * sizeof(D3D11_SUBRESOURCE_DATA));
			if (!tex_mem) { skg_log(skg_log_critical, "Out of memory"); return;  }

			for (int32_t i = 0; i < data_frame_count; i++) {
				tex_mem[i*mip_levels] = {};
				tex_mem[i*mip_levels].pSysMem     = data_frames[i];
				tex_mem[i*mip_levels].SysMemPitch = (UINT)(px_size * width);

				if (mips) {
					skg_make_mips(&tex_mem[i*mip_levels], data_frames[i], tex->format, width, height, mip_levels);
				}
			}
		}

		if (FAILED(d3d_device->CreateTexture2D(&desc, tex_mem, &tex->_texture))) {
			skg_log(skg_log_critical, "Create texture error!");
		}

		if (tex_mem != nullptr) {
			for (int32_t i = 0; i < data_frame_count; i++) {
				for (uint32_t m = 1; m < mip_levels; m++) {
					free((void*)tex_mem[i*mip_levels + m].pSysMem);
				} 
			}
			free(tex_mem);
		}

		skg_tex_make_view(tex, mip_levels, 0, true);
	} else {
		// For dynamic textures, just upload the new value into the texture!
		D3D11_MAPPED_SUBRESOURCE tex_mem = {};
		if (FAILED(d3d_context->Map(tex->_texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &tex_mem))) {
			skg_log(skg_log_critical, "Failed mapping a texture");
			return;
		}

		uint8_t *dest_line  = (uint8_t *)tex_mem.pData;
		uint8_t *src_line   = (uint8_t *)data_frames[0];
		for (int i = 0; i < height; i++) {
			memcpy(dest_line, src_line, (size_t)width * px_size);
			dest_line += tex_mem.RowPitch;
			src_line  += px_size * (uint64_t)width;
		}
		d3d_context->Unmap(tex->_texture, 0);
	}

	// If the sampler has not been set up yet, we'll make a default one real quick.
	if (tex->_sampler == nullptr) {
		skg_tex_settings(tex, skg_tex_address_repeat, skg_tex_sample_linear, 0);
	}
}

///////////////////////////////////////////

bool skg_tex_get_contents(skg_tex_t *tex, void *ref_data, size_t data_size) {
	// Make sure we've been provided enough memory to hold this texture
	size_t format_size = skg_tex_fmt_size(tex->format);
	if (data_size != (size_t)tex->width * (size_t)tex->height * format_size) {
		skg_log(skg_log_critical, "Insufficient buffer size for skg_tex_get_contents");
		return false;
	}

	D3D11_TEXTURE2D_DESC desc             = {};
	ID3D11Texture2D     *copy_tex         = nullptr;
	bool                 copy_tex_release = true;
	tex->_texture->GetDesc(&desc);

	// Make sure copy_tex is a texture that we can read from!
	if (desc.SampleDesc.Count > 1) {
		// Not gonna bother with MSAA stuff
		skg_log(skg_log_warning, "skg_tex_get_contents MSAA surfaces not implemented");
		return false;
	} else if ((desc.Usage == D3D11_USAGE_STAGING) && (desc.CPUAccessFlags & D3D11_CPU_ACCESS_READ)) {
		// Handle case where the source is already a staging texture we can use directly
		copy_tex         = tex->_texture;
		copy_tex_release = false;
	} else {
		// Otherwise, create a staging texture from the non-MSAA source
		desc.BindFlags      = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.Usage          = D3D11_USAGE_STAGING;

		if (FAILED(d3d_device->CreateTexture2D(&desc, nullptr, &copy_tex))) {
			skg_log(skg_log_critical, "CreateTexture2D failed!");
			return false;
		}
		d3d_context->CopyResource(copy_tex, tex->_texture);
	}

	// Load the data into CPU RAM
	D3D11_MAPPED_SUBRESOURCE data;
	if (FAILED(d3d_context->Map(copy_tex, 0, D3D11_MAP_READ, 0, &data))) {
		skg_log(skg_log_critical, "Texture Map failed!");
		return false;
	}

	// Copy it into our waiting buffer
	uint8_t *srcPtr  = (uint8_t*)data.pData;
	uint8_t *destPtr = (uint8_t*)ref_data;
	size_t   msize   = tex->width*format_size;
	for (size_t h = 0; h < desc.Height; ++h) {
		memcpy(destPtr, srcPtr, msize);
		srcPtr  += data.RowPitch;
		destPtr += msize;
	}

	// And cleanup
	d3d_context->Unmap(copy_tex, 0);
	if (copy_tex_release)
		copy_tex->Release();

	return true;
}

///////////////////////////////////////////

void skg_tex_clear(skg_bind_t bind) {
	switch (bind.register_type) {
	case skg_register_resource: {
		ID3D11SamplerState       *null_state = nullptr;
		ID3D11ShaderResourceView *null_view  = nullptr;
		if (bind.stage_bits & skg_stage_pixel ){
			d3d_context->PSSetSamplers       (bind.slot, 1, &null_state);
			d3d_context->PSSetShaderResources(bind.slot, 1, &null_view);
		}
		if (bind.stage_bits & skg_stage_vertex) {
			d3d_context->VSSetSamplers       (bind.slot, 1, &null_state);
			d3d_context->VSSetShaderResources(bind.slot, 1, &null_view);
		}
		if (bind.stage_bits & skg_stage_compute) {
			d3d_context->CSSetSamplers       (bind.slot, 1, &null_state);
			d3d_context->CSSetShaderResources(bind.slot, 1, &null_view);
		}
	} break;
	case skg_register_readwrite: {
		if (bind.stage_bits & skg_stage_compute) {
			ID3D11UnorderedAccessView *null_view = nullptr;
			d3d_context->CSSetUnorderedAccessViews(bind.slot, 1, &null_view, nullptr);
		}
	} break;
	default: skg_log(skg_log_critical, "You can only bind/clear a texture to skg_register_resource, or skg_register_readwrite!"); break;
	}
}

///////////////////////////////////////////

void skg_tex_bind(const skg_tex_t *texture, skg_bind_t bind) {
	switch (bind.register_type) {
	case skg_register_resource: {
		if (bind.stage_bits & skg_stage_pixel ){
			d3d_context->PSSetSamplers       (bind.slot, 1, &texture->_sampler);
			d3d_context->PSSetShaderResources(bind.slot, 1, &texture->_resource);
		}
		if (bind.stage_bits & skg_stage_vertex) {
			d3d_context->VSSetSamplers       (bind.slot, 1, &texture->_sampler);
			d3d_context->VSSetShaderResources(bind.slot, 1, &texture->_resource);
		}
		if (bind.stage_bits & skg_stage_compute) {
			d3d_context->CSSetSamplers       (bind.slot, 1, &texture->_sampler);
			d3d_context->CSSetShaderResources(bind.slot, 1, &texture->_resource);
		}
	} break;
	case skg_register_readwrite: {
		if (bind.stage_bits & skg_stage_compute) d3d_context->CSSetUnorderedAccessViews(bind.slot, 1, &texture->_unordered, nullptr);
	} break;
	default: skg_log(skg_log_critical, "You can only bind/clear a texture to skg_register_resource, or skg_register_readwrite!"); break;
	}
}

///////////////////////////////////////////

void skg_tex_destroy(skg_tex_t *tex) {
	if (tex->_target_view) tex->_target_view->Release();
	if (tex->_depth_view ) tex->_depth_view ->Release();
	if (tex->_resource   ) tex->_resource   ->Release();
	if (tex->_sampler    ) tex->_sampler    ->Release();
	if (tex->_texture    ) tex->_texture    ->Release();
	*tex = {};
}

///////////////////////////////////////////

template <typename T>
void skg_downsample_4(T *data, int32_t width, int32_t height, T **out_data, int32_t *out_width, int32_t *out_height) {
	int w = (int32_t)log2f((float)width);
	int h = (int32_t)log2f((float)height);
	*out_width  = w = max(1, (1 << w) >> 1);
	*out_height = h = max(1, (1 << h) >> 1);

	*out_data = (T*)malloc((int64_t)w * h * sizeof(T) * 4);
	if (*out_data == nullptr) { skg_log(skg_log_critical, "Out of memory"); return; }
	memset(*out_data, 0, (int64_t)w * h * sizeof(T) * 4);
	T *result = *out_data;

	for (int32_t y = 0; y < (*out_height); y++) {
		int32_t src_row_start  = y * 2 * width;
		int32_t dest_row_start = y * (*out_width);
		for (int32_t x = 0; x < (*out_width);  x++) {
			int src   = (x*2 + src_row_start )*4;
			int dest  = (x   + dest_row_start)*4;
			int src_n = src + width*4;
			T *cD = &result[dest];

			cD[0] = (data[src] + data[src+4] + data[src_n] + data[src_n+4])/4; src++; src_n++;
			cD[1] = (data[src] + data[src+4] + data[src_n] + data[src_n+4])/4; src++; src_n++;
			cD[2] = (data[src] + data[src+4] + data[src_n] + data[src_n+4])/4; src++; src_n++;
			cD[3] = (data[src] + data[src+4] + data[src_n] + data[src_n+4])/4; src++; src_n++;
		}
	}
}

///////////////////////////////////////////

template <typename T>
void skg_downsample_1(T *data, int32_t width, int32_t height, T **out_data, int32_t *out_width, int32_t *out_height) {
	int w = (int32_t)log2f((float)width);
	int h = (int32_t)log2f((float)height);
	*out_width  = w = (1 << w) >> 1;
	*out_height = h = (1 << h) >> 1;

	*out_data = (T*)malloc((int64_t)w * h * sizeof(T));
	if (*out_data == nullptr) { skg_log(skg_log_critical, "Out of memory"); return; }
	memset(*out_data, 0, (int64_t)w * h * sizeof(T));
	T *result = *out_data;

	for (int32_t y = 0; y < (*out_height); y++) {
		int32_t src_row_start  = y * 2 * width;
		int32_t dest_row_start = y * (*out_width);
		for (int32_t x = 0; x < (*out_width);  x++) {
			int src   = x*2 + src_row_start;
			int dest  = x   + dest_row_start;
			int src_n = src + width;
			result[dest] = (data[src] + data[src+1] + data[src_n] + data[src_n+1]) / 4;
		}
	}
}

///////////////////////////////////////////

int64_t skg_tex_fmt_to_native(skg_tex_fmt_ format){
	switch (format) {
	case skg_tex_fmt_rgba32:        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case skg_tex_fmt_rgba32_linear: return DXGI_FORMAT_R8G8B8A8_UNORM;
	case skg_tex_fmt_bgra32:        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	case skg_tex_fmt_bgra32_linear: return DXGI_FORMAT_B8G8R8A8_UNORM;
	case skg_tex_fmt_rg11b10:       return DXGI_FORMAT_R11G11B10_FLOAT;
	case skg_tex_fmt_rgb10a2:       return DXGI_FORMAT_R10G10B10A2_UNORM;
	case skg_tex_fmt_rgba64u:       return DXGI_FORMAT_R16G16B16A16_UNORM;
	case skg_tex_fmt_rgba64s:       return DXGI_FORMAT_R16G16B16A16_SNORM;
	case skg_tex_fmt_rgba64f:       return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case skg_tex_fmt_rgba128:       return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case skg_tex_fmt_depth16:       return DXGI_FORMAT_D16_UNORM;
	case skg_tex_fmt_depth32:       return DXGI_FORMAT_D32_FLOAT;
	case skg_tex_fmt_depthstencil:  return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case skg_tex_fmt_r8:            return DXGI_FORMAT_R8_UNORM;
	case skg_tex_fmt_r16:           return DXGI_FORMAT_R16_UNORM;
	case skg_tex_fmt_r32:           return DXGI_FORMAT_R32_FLOAT;
	default: return DXGI_FORMAT_UNKNOWN;
	}
}

///////////////////////////////////////////

skg_tex_fmt_ skg_tex_fmt_from_native(int64_t format) {
	switch (format) {
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return skg_tex_fmt_rgba32;
	case DXGI_FORMAT_R8G8B8A8_UNORM:      return skg_tex_fmt_rgba32_linear;
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return skg_tex_fmt_bgra32;
	case DXGI_FORMAT_B8G8R8A8_UNORM:      return skg_tex_fmt_bgra32_linear;
	case DXGI_FORMAT_R11G11B10_FLOAT:     return skg_tex_fmt_rg11b10;
	case DXGI_FORMAT_R10G10B10A2_UNORM:   return skg_tex_fmt_rgb10a2;
	case DXGI_FORMAT_R16G16B16A16_UNORM:  return skg_tex_fmt_rgba64u;
	case DXGI_FORMAT_R16G16B16A16_SNORM:  return skg_tex_fmt_rgba64s;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:  return skg_tex_fmt_rgba64f;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:  return skg_tex_fmt_rgba128;
	case DXGI_FORMAT_D16_UNORM:           return skg_tex_fmt_depth16;
	case DXGI_FORMAT_D32_FLOAT:           return skg_tex_fmt_depth32;
	case DXGI_FORMAT_D24_UNORM_S8_UINT:   return skg_tex_fmt_depthstencil;
	case DXGI_FORMAT_R8_UNORM:            return skg_tex_fmt_r8;
	case DXGI_FORMAT_R16_UNORM:           return skg_tex_fmt_r16;
	case DXGI_FORMAT_R32_FLOAT:           return skg_tex_fmt_r32;
	default: return skg_tex_fmt_none;
	}
}

///////////////////////////////////////////

const char *skg_semantic_to_d3d(skg_el_semantic_ semantic) {
	switch (semantic) {
	case skg_el_semantic_none:         return "";
	case skg_el_semantic_position:     return "SV_POSITION";
	case skg_el_semantic_normal:       return "NORMAL";
	case skg_el_semantic_texcoord:     return "TEXCOORD";
	case skg_el_semantic_color:        return "COLOR";
	case skg_el_semantic_target_index: return "SV_RenderTargetArrayIndex";
	default: return "";
	}
}

#endif
