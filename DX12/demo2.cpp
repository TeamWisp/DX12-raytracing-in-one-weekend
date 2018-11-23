#include <demo2.h>
#include <application.h>
#include <command_queue.h>
#include <helpers.h>
#include <window.h>

#include <wrl.h>
using namespace Microsoft::WRL;

#include <d3dx12.h>
#include <d3dcompiler.h>

#include <algorithm> // For std::min and std::max.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace DirectX;

// Clamp a value between a min and max range.
template<typename T>
constexpr const T& clamp(const T& val, const T& min, const T& max)
{
	return val < min ? min : val > max ? max : val;
}

// Vertex data for a colored cube.
struct Vertex
{
	XMFLOAT3 Position;
	XMFLOAT2 Color;
};

static Vertex g_vertices[4] = {
	Vertex{
		XMFLOAT3(-1.0f, +1.0f, +0.0f),
		XMFLOAT2(+0.0f, +1.0f)
	},
	Vertex{
		XMFLOAT3(-1.0f, -1.0f, +0.0f),
		XMFLOAT2(+0.0f, +0.0f)
	},
	Vertex{
		XMFLOAT3(+1.0f, -1.0f, +0.0f),
		XMFLOAT2(+1.0f, +0.0f)
	},
	Vertex{
		XMFLOAT3(+1.0f, +1.0f, +0.0f),
		XMFLOAT2(+1.0f, +1.0f)
	}
};

static WORD g_indicies[6] =
{
	2, 1, 0, 3, 2, 0
};

Demo2::Demo2(Application &arg_app, const std::wstring& arg_name, int arg_width, int arg_height, bool arg_v_sync)
	: parent(arg_app, arg_name, arg_width, arg_height, arg_v_sync)
	, scissor_rect_(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	, viewport_(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(arg_width), static_cast<float>(arg_height)))
	, fov_(45.0)
	, content_loaded_(false)
{ }

void Demo2::TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> arg_command_list, Microsoft::WRL::ComPtr<ID3D12Resource> arg_resource, D3D12_RESOURCE_STATES arg_before_state, D3D12_RESOURCE_STATES arg_after_state)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		arg_resource.Get(),
		arg_before_state, arg_after_state);

	arg_command_list->ResourceBarrier(1, &barrier);

}

void Demo2::ClearRTV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> arg_command_list, D3D12_CPU_DESCRIPTOR_HANDLE arg_rtv, FLOAT * arg_clear_color)
{
	arg_command_list->ClearRenderTargetView(arg_rtv, arg_clear_color, 0, nullptr);
}

void Demo2::ClearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> arg_command_list, D3D12_CPU_DESCRIPTOR_HANDLE arg_dsv, FLOAT arg_depth)
{
	arg_command_list->ClearDepthStencilView(arg_dsv, D3D12_CLEAR_FLAG_DEPTH, arg_depth, 0, 0, nullptr);
}

void Demo2::UpdateBufferResource(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> arg_command_list,
	ID3D12Resource** arg_destination_resource, ID3D12Resource** arg_intermediate_resource,
	size_t arg_num_elements, size_t arg_element_size,
	const void* arg_buffer_data,
	D3D12_RESOURCE_FLAGS arg_flags)
{
	auto device = app_->GetDevice();

	size_t buffer_size = arg_num_elements * arg_element_size;

	// Create a committed resource for the GPU resource in a default heap.
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffer_size, arg_flags),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(arg_destination_resource)));

	// Create an committed resource for the upload.
	if (arg_buffer_data)
	{
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(buffer_size),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(arg_intermediate_resource)));

		D3D12_SUBRESOURCE_DATA subresource_data = { };
		subresource_data.pData = arg_buffer_data;
		subresource_data.RowPitch = buffer_size;
		subresource_data.SlicePitch = subresource_data.RowPitch;

		UpdateSubresources(arg_command_list.Get(),
						   *arg_destination_resource, *arg_intermediate_resource,
						   0, 0, 1, &subresource_data);
	}
}

void Demo2::ResizeDepthBuffer(int arg_width, int arg_height)
{
	if (content_loaded_)
	{
		// Flush any GPU commands that might be referencing the depth buffer.
		app_->Flush();

		arg_width = std::max(1, arg_width);
		arg_height = std::max(1, arg_height);

		auto device = app_->GetDevice();

		// Resize screen dependent resources.
		// Create a depth buffer.
		D3D12_CLEAR_VALUE optimized_clear_value = { };
		optimized_clear_value.Format = DXGI_FORMAT_D32_FLOAT;
		optimized_clear_value.DepthStencil = {1.0f, 0};

		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, arg_width, arg_height,
			1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&optimized_clear_value,
			IID_PPV_ARGS(&depth_buffer_)
		));

		// Update the depth-stencil view.
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = { };
		dsv.Format = DXGI_FORMAT_D32_FLOAT;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		dsv.Flags = D3D12_DSV_FLAG_NONE;

		device->CreateDepthStencilView(depth_buffer_.Get(), &dsv,
									   dsv_heap_->GetCPUDescriptorHandleForHeapStart());
	}
}

bool Demo2::LoadContent()
{
	auto device = app_->GetDevice();
	auto command_queue = app_->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto command_list = command_queue->GetCommandList();

	XMFLOAT2 dims = XMFLOAT2(GetClientWidth() * 0.5f, GetClientHeight() * 0.5f);
	for (size_t i = 0; i < _countof(g_vertices); ++i)
	{
		g_vertices[i].Position.x *= dims.x;
		g_vertices[i].Position.y *= dims.y;
	}

	// Upload vertex pos buffer data.
	ComPtr<ID3D12Resource> intermediate_vertex_buffer;
	UpdateBufferResource(command_list.Get(),
						 &vertex_buffer_, &intermediate_vertex_buffer,
						 _countof(g_vertices), sizeof(Vertex), g_vertices);

	// Create the vertex pos buffer view.
	vertex_buffer_view_.BufferLocation = vertex_buffer_->GetGPUVirtualAddress();
	vertex_buffer_view_.SizeInBytes = sizeof(g_vertices);
	vertex_buffer_view_.StrideInBytes = sizeof(Vertex);

	// Upload index buffer data.
	ComPtr<ID3D12Resource> intermediate_index_buffer;
	UpdateBufferResource(command_list.Get(),
						 &index_buffer_, &intermediate_index_buffer,
						 _countof(g_indicies), sizeof(WORD), g_indicies);

	// Create the index buffer view.
	index_buffer_view_.BufferLocation = index_buffer_->GetGPUVirtualAddress();
	index_buffer_view_.SizeInBytes = sizeof(g_indicies);
	index_buffer_view_.Format = DXGI_FORMAT_R16_UINT;

	// Create the descriptor heap for the depth-stencil view.
	D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = { };
	dsv_heap_desc.NumDescriptors = 1;
	dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&dsv_heap_)));

	// Load the vertex shader.
	ComPtr<ID3DBlob> vertex_shader_blob;
	ThrowIfFailed(D3DReadFileToBlob(L"VertexShader.cso", &vertex_shader_blob));

	// Load the pixel shader.
	ComPtr<ID3DBlob> pixel_shader_blob;
	ThrowIfFailed(D3DReadFileToBlob(L"PixelShader.cso", &pixel_shader_blob));

	D3D12_INPUT_ELEMENT_DESC input_layout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXTURECOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	// Create a root signature.
	D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = { };
	feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
	{
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	// Allow input layout and deny unnecessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS root_signature_flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	// ========= TEXTURE STUFF =========

	// create a descriptor range (descriptor table) and fill it out
	// this is a range of descriptors inside a descriptor heap
	CD3DX12_DESCRIPTOR_RANGE1  descriptor_table_ranges[1]; // only one range right now
	descriptor_table_ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	// ========= ENDS TEXTURE STUFF =========

	// A single 32-bit constant root parameter that is used by the vertex shader.
	CD3DX12_ROOT_PARAMETER1 root_parameters[2];
	root_parameters[0].InitAsConstants(sizeof(XMMATRIX) / 4, 0);
	root_parameters[1].InitAsDescriptorTable(_countof(descriptor_table_ranges), descriptor_table_ranges);

	// ========= TEXTURE STUFF =========

	// create a static sampler
	D3D12_STATIC_SAMPLER_DESC sampler_desc = {};
	sampler_desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sampler_desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler_desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler_desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler_desc.MipLODBias = 0;
	sampler_desc.MaxAnisotropy = 0;
	sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler_desc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler_desc.MinLOD = 0.0f;
	sampler_desc.MaxLOD = D3D12_FLOAT32_MAX;
	sampler_desc.ShaderRegister = 0;
	sampler_desc.RegisterSpace = 0;
	sampler_desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// ========= ENDS TEXTURE STUFF =========

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_description;
	root_signature_description.Init_1_1(_countof(root_parameters), root_parameters, 1,
										// ============ TEXTURE STUFF ===========
										&sampler_desc,
										// ========= ENDS TEXTURE STUFF =========
										root_signature_flags);

	// Serialize the root signature.
	ComPtr<ID3DBlob> root_signature_blob;
	ComPtr<ID3DBlob> error_blob;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&root_signature_description,
				  feature_data.HighestVersion, &root_signature_blob, &error_blob));

	// Create the root signature.
	ThrowIfFailed(device->CreateRootSignature(0, root_signature_blob->GetBufferPointer(),
				  root_signature_blob->GetBufferSize(), IID_PPV_ARGS(&root_signature_)));

	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE root_signature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT input_layout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitive_topology_type;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER rasterizer;
		CD3DX12_PIPELINE_STATE_STREAM_VS vs;
		CD3DX12_PIPELINE_STATE_STREAM_PS ps;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT dsv_format;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtv_formats;
		CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC blend_desc;
	} pipeline_state_stream;

	D3D12_RT_FORMAT_ARRAY rtv_formats = { };
	rtv_formats.NumRenderTargets = 1;
	rtv_formats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	pipeline_state_stream.root_signature = root_signature_.Get();
	pipeline_state_stream.input_layout = {input_layout, _countof(input_layout)};
	pipeline_state_stream.primitive_topology_type = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipeline_state_stream.rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	pipeline_state_stream.vs = CD3DX12_SHADER_BYTECODE(vertex_shader_blob.Get());
	pipeline_state_stream.ps = CD3DX12_SHADER_BYTECODE(pixel_shader_blob.Get());
	pipeline_state_stream.dsv_format = DXGI_FORMAT_D32_FLOAT;
	pipeline_state_stream.rtv_formats = rtv_formats;
	pipeline_state_stream.blend_desc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

	D3D12_PIPELINE_STATE_STREAM_DESC pipeline_state_stream_desc = {
		sizeof(PipelineStateStream), &pipeline_state_stream
	};
	HRESULT hr = device->CreatePipelineState(&pipeline_state_stream_desc, IID_PPV_ARGS(&pipeline_state_));
	ThrowIfFailed(hr);



	// ============ TEXTURE STUFF ===========

	D3D12_RESOURCE_DESC texture_desc;
	UINT64 texture_width = 0, texture_height = 0;
	DXGI_FORMAT texture_format;
	int pixel_size = 0; // in bytes
	unsigned char * texture = LoadTexture("texture.png", texture_desc, texture_width, texture_height, texture_format, pixel_size);

	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&texture_desc, // the description of our texture
		D3D12_RESOURCE_STATE_COPY_DEST, // We will copy the texture from the upload heap to here, so we start it out in a copy dest state
		nullptr, // used for render targets and depth/stencil buffers
		IID_PPV_ARGS(&texture_buffer_));

	int texture_heap_size = ((((texture_width * pixel_size) + 255) & ~255) * (texture_height - 1)) + (texture_width * pixel_size);

	UINT64 texture_upload_buffer_size;
	// this function gets the size an upload buffer needs to be to upload a texture to the gpu.
	// each row must be 256 byte aligned except for the last row, which can just be the size in bytes of the row
	// eg. textureUploadBufferSize = ((((width * numBytesPerPixel) + 255) & ~255) * (height - 1)) + (width * numBytesPerPixel);
	//textureUploadBufferSize = (((imageBytesPerRow + 255) & ~255) * (textureDesc.Height - 1)) + imageBytesPerRow;
	device->GetCopyableFootprints(&texture_desc, 0, 1, 0, nullptr, nullptr, nullptr, &texture_upload_buffer_size);

	// now we create an upload heap to upload our texture to the GPU
	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(texture_upload_buffer_size), // resource description for a buffer (storing the image data in this heap just to copy to the default heap)
		D3D12_RESOURCE_STATE_GENERIC_READ, // We will copy the contents from this heap to the default heap above
		nullptr,
		IID_PPV_ARGS(&texture_buffer_upload_heap_));

	// store vertex buffer in upload heap
	D3D12_SUBRESOURCE_DATA texture_data = {};
	texture_data.pData = &texture[0]; // pointer to our image data
	texture_data.RowPitch = texture_width * pixel_size; // size of all our triangle vertex data
	texture_data.SlicePitch = texture_data.RowPitch * texture_height; // also the size of our triangle vertex data

	// Now we copy the upload buffer contents to the default heap
	ID3D12GraphicsCommandList* cmdlst = command_list.Get();
	UpdateSubresources(cmdlst, texture_buffer_.Get(), texture_buffer_upload_heap_.Get(), 0, 0, 1, &texture_data);

	// transition the texture default heap to a pixel shader resource (we will be sampling from this heap in the pixel shader to get the color of pixels)
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture_buffer_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

	D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
	heap_desc.NumDescriptors = 1;
	heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&main_descriptor_heap_));

	// now we create a shader resource view (descriptor that points to the texture and describes it)
	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv_desc.Format = texture_desc.Format;
	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = 1;
	device->CreateShaderResourceView(texture_buffer_.Get(), &srv_desc, main_descriptor_heap_->GetCPUDescriptorHandleForHeapStart());

	// ========= ENDS TEXTURE STUFF =========
	



	auto fence_value = command_queue->ExecuteCommandList(command_list);
	command_queue->WaitForFenceValue(fence_value);

	stbi_image_free(texture);
	content_loaded_ = true;


	// Resize/Create the depth buffer.
	ResizeDepthBuffer(GetClientWidth(), GetClientHeight());

	return true;
}

unsigned char * Demo2::LoadTexture(const char * path, D3D12_RESOURCE_DESC & resource_desc, UINT64 &texture_width, UINT64 &texture_height, DXGI_FORMAT &dxgi_format, int & pixel_size)
{

	int width, height, channels;
	stbi_set_flip_vertically_on_load(true);
	unsigned char *image = stbi_load(path,
									 &width,
									 &height,
									 &channels,
									 STBI_rgb_alpha);

	if (image == nullptr)
	{
		ThrowIfFailed(-1);
	}

	texture_width = width;
	texture_height = height;

	dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM;
	pixel_size = 4; // in bytes

	resource_desc = {};
	resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resource_desc.Alignment = 0; // may be 0, 4KB, 64KB, or 4MB. 0 will let runtime decide between 64KB and 4MB (4MB for multi-sampled textures)
	resource_desc.Width = width; // width of the texture
	resource_desc.Height = height; // height of the texture
	resource_desc.DepthOrArraySize = 1; // if 3d image, depth of 3d image. Otherwise an array of 1D or 2D textures (we only have one image, so we set 1)
	resource_desc.MipLevels = 1; // Number of mipmaps. We are not generating mipmaps for this texture, so we have only one level
	resource_desc.Format = dxgi_format; // This is the dxgi format of the image (format of the pixels)
	resource_desc.SampleDesc.Count = 1; // This is the number of samples per pixel, we just want 1 sample
	resource_desc.SampleDesc.Quality = 0; // The quality level of the samples. Higher is better quality, but worse performance
	resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // The arrangement of the pixels. Setting to unknown lets the driver choose the most efficient one
	resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE; // no flags

	return image;
}

void Demo2::LoadTextures()
{


	

}

void Demo2::OnResize(ResizeEventArgs& arg_e)
{
	if (arg_e.Width != GetClientWidth() || arg_e.Height != GetClientHeight())
	{
		parent::OnResize(arg_e);

		viewport_ = CD3DX12_VIEWPORT(0.0f, 0.0f,
									 static_cast<float>(arg_e.Width), static_cast<float>(arg_e.Height));

		ResizeDepthBuffer(arg_e.Width, arg_e.Height);
	}
}

void Demo2::OnUpdate(UpdateEventArgs& arg_e)
{
	static uint64_t frame_count = 0;
	static double total_time = 0.0;

	parent::OnUpdate(arg_e);

	total_time += arg_e.ElapsedTime;
	frame_count++;

	if (total_time > 1.0)
	{
		double fps = frame_count / total_time;

		char buffer[512];
		sprintf_s(buffer, "FPS: %f\n", fps);
		OutputDebugStringA(buffer);

		frame_count = 0;
		total_time = 0.0;
	}

	// Update the model matrix.
	float angle = 0.f;
	const XMVECTOR rotation_axis = XMVectorSet(1, 0, 0, 0);
	model_matrix_ = XMMatrixRotationAxis(rotation_axis, XMConvertToRadians(angle));

	// Update the view matrix.
	const XMVECTOR eye_position = XMVectorSet(0, 0, -10, 1);
	const XMVECTOR focus_point = XMVectorSet(0, 0, 0, 1);
	const XMVECTOR up_direction = XMVectorSet(0, 1, 0, 0);
	view_matrix_ = XMMatrixLookAtLH(eye_position, focus_point, up_direction);

	// Update the projection matrix.
	float aspect_ratio = GetClientWidth() / static_cast<float>(GetClientHeight());
	projection_matrix_ = XMMatrixOrthographicLH(GetClientWidth(), GetClientHeight(), 0.1f, 100.0f);
}

void Demo2::OnRender(RenderEventArgs& arg_e)
{
	parent::OnRender(arg_e);

	auto command_queue = app_->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto command_list = command_queue->GetCommandList();

	UINT current_back_buffer_index = window_->GetCurrentBackBufferIndex();
	auto back_buffer = window_->GetCurrentBackBuffer();
	auto rtv = window_->GetCurrentRenderTargetView();
	auto dsv = dsv_heap_->GetCPUDescriptorHandleForHeapStart();

	// Clear the render targets.
	{
		TransitionResource(command_list, back_buffer,
						   D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		FLOAT clear_color[] = {0.4f, 0.6f, 0.9f, 1.0f};

		ClearRTV(command_list, rtv, clear_color);
		ClearDepth(command_list, dsv);
	}

	command_list->SetPipelineState(pipeline_state_.Get());
	command_list->SetGraphicsRootSignature(root_signature_.Get());

	// set the descriptor heap
	ID3D12DescriptorHeap* descriptorHeaps[] = {main_descriptor_heap_.Get()};
	command_list->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// set the descriptor table to the descriptor heap (parameter 1, as constant buffer root descriptor is parameter index 0)
	command_list->SetGraphicsRootDescriptorTable(1, main_descriptor_heap_->GetGPUDescriptorHandleForHeapStart());

	D3D12_VERTEX_BUFFER_VIEW buffer_views[1] = {
		vertex_buffer_view_
	};

	command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command_list->IASetVertexBuffers(0, 1, buffer_views);
	command_list->IASetIndexBuffer(&index_buffer_view_);

	command_list->RSSetViewports(1, &viewport_);
	command_list->RSSetScissorRects(1, &scissor_rect_);

	command_list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

	// Update the MVP matrix
	XMMATRIX mvp_matrix = XMMatrixMultiply(model_matrix_, view_matrix_);
	mvp_matrix = XMMatrixMultiply(mvp_matrix, projection_matrix_);
	command_list->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / 4, &mvp_matrix, 0);

	command_list->DrawIndexedInstanced(_countof(g_indicies), 1, 0, 0, 0);

	// Present
	{
		TransitionResource(command_list, back_buffer,
						   D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		fence_values_[current_back_buffer_index] = command_queue->ExecuteCommandList(command_list);

		current_back_buffer_index = window_->Present();

		command_queue->WaitForFenceValue(fence_values_[current_back_buffer_index]);
	}
}



void Demo2::OnKeyPressed(KeyEventArgs& arg_e)
{
	parent::OnKeyPressed(arg_e);

	switch (arg_e.Key)
	{
		case KeyCode::Escape:
			app_->Quit(0);
			break;
		case KeyCode::Enter:
			if (arg_e.Alt)
			{
		case KeyCode::F11:
			window_->ToggleFullscreen();
			break;
			}
		case KeyCode::V:
			window_->ToggleVSync();
			break;
	}
}

void Demo2::OnMouseWheel(MouseWheelEventArgs& arg_e)
{
	fov_ -= arg_e.WheelDelta;
	fov_ = clamp(fov_, 12.0f, 90.0f);

	char buffer[256];
	sprintf_s(buffer, "FoV: %f\n", fov_);
	OutputDebugStringA(buffer);
}

void Demo2::UnloadContent()
{
	content_loaded_ = false;
}
