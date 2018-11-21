#pragma once

#include <Game.h>
#include <Window.h>

#include <DirectXMath.h>

class Demo2 : public Game
{
public:
	using parent = Game;

	Demo2(Application &arg_app, const std::wstring& arg_name, int arg_width, int arg_height, bool arg_v_sync = false);
	/**
	*  Load content required for the demo.
	*/
	virtual bool LoadContent() override;

	/**
	*  Unload demo specific content that was loaded in LoadContent.
	*/
	virtual void UnloadContent() override;

protected:
	/**
	*  Update the game logic.
	*/
	virtual void OnUpdate(UpdateEventArgs& arg_e) override;

	/**
	*  Render stuff.
	*/
	virtual void OnRender(RenderEventArgs& arg_e) override;

	/**
	* Invoked by the registered window when a key is pressed
	* while the window has focus.
	*/
	virtual void OnKeyPressed(KeyEventArgs& arg_e) override;

	/**
	* Invoked when the mouse wheel is scrolled while the registered window has focus.
	*/
	virtual void OnMouseWheel(MouseWheelEventArgs& arg_e) override;


	virtual void OnResize(ResizeEventArgs& arg_e) override;

private:
	// Helper functions
	// Transition a resource
	void TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> arg_command_list,
							Microsoft::WRL::ComPtr<ID3D12Resource> arg_resource,
							D3D12_RESOURCE_STATES arg_before_state, D3D12_RESOURCE_STATES arg_after_state);

	// Clear a render target view.
	void ClearRTV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> arg_command_list,
				  D3D12_CPU_DESCRIPTOR_HANDLE arg_rtv, FLOAT* arg_clear_color);

	// Clear the depth of a depth-stencil view.
	void ClearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> arg_command_list,
					D3D12_CPU_DESCRIPTOR_HANDLE arg_dsv, FLOAT arg_depth = 1.0f);

	// Create a GPU buffer.
	void UpdateBufferResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> arg_command_list,
							  ID3D12Resource** arg_destination_resource, ID3D12Resource** arg_intermediate_resource,
							  size_t arg_num_elements, size_t arg_element_size, const void* arg_buffer_data,
							  D3D12_RESOURCE_FLAGS arg_flags = D3D12_RESOURCE_FLAG_NONE);

	// Resize the depth buffer to match the size of the client area.
	void ResizeDepthBuffer(int arg_width, int arg_height);

	unsigned char * LoadTexture(const char * path, D3D12_RESOURCE_DESC &resource_desc, UINT64 &texture_width, UINT64 &texture_height, DXGI_FORMAT &dxgi_format, int & pixel_size);

	// Load texture(s)
	void LoadTextures();

	uint64_t fence_values_[Window::buffer_count_] = { };

	// Vertex buffer for the cube.
	Microsoft::WRL::ComPtr<ID3D12Resource> vertex_buffer_;
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view_;
	// Color buffer for the cube.
	Microsoft::WRL::ComPtr<ID3D12Resource> color_buffer_;
	D3D12_VERTEX_BUFFER_VIEW color_buffer_view_;
	// Index buffer for the cube.
	Microsoft::WRL::ComPtr<ID3D12Resource> index_buffer_;
	D3D12_INDEX_BUFFER_VIEW index_buffer_view_;

	// Depth buffer.
	Microsoft::WRL::ComPtr<ID3D12Resource> depth_buffer_;
	// Descriptor heap for depth buffer.
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsv_heap_;

	// Root signature
	Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;

	// Pipeline state object.
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state_;

	// Texture objects
	Microsoft::WRL::ComPtr<ID3D12Resource> texture_buffer_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> main_descriptor_heap_;
	Microsoft::WRL::ComPtr<ID3D12Resource> texture_buffer_upload_heap_;

	D3D12_VIEWPORT viewport_;
	D3D12_RECT scissor_rect_;

	float fov_;

	DirectX::XMMATRIX model_matrix_;
	DirectX::XMMATRIX view_matrix_;
	DirectX::XMMATRIX projection_matrix_;

	bool content_loaded_;
};