#pragma once
#include <events.h>
#include <high_resolution_clock.h>

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_5.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <memory>

class Game;
class Application;

class Window
{
public:
	// Number of swapchain back buffers.
	static const UINT buffer_count_ = 3;
	
	Window(HWND arg_h_wnd, const std::wstring& arg_window_name, int arg_client_width, int arg_client_height, bool arg_v_sync, Application &arg_app);
	virtual ~Window();

	/**
	* Get a handle to this window's instance.
	* @returns The handle to the window instance or nullptr if this is not a valid window.
	*/
	HWND GetWindowHandle() const;

	/**
	* Destroy this window.
	*/
	void Destroy();

	const std::wstring& GetWindowName() const;

	int GetClientWidth() const;
	int GetClientHeight() const;

	/**
	* Should this window be rendered with vertical refresh synchronization.
	*/
	bool IsVSync() const;
	void SetVSync(bool arg_v_sync);
	void ToggleVSync();

	/**
	* Is this a windowed window or full-screen?
	*/
	bool IsFullScreen() const;

	// Set the fullscreen state of the window.
	void SetFullscreen(bool arg_fullscreen);
	void ToggleFullscreen();

	/**
	* Show this window.
	*/
	void Show();

	/**
	* Hide the window.
	*/
	void Hide();

	/**
	* Return the current back buffer index.
	*/
	UINT GetCurrentBackBufferIndex() const;

	/**
	* Present the swapchain's back buffer to the screen.
	* Returns the current back buffer index after the present.
	*/
	UINT Present();

	/**
	* Get the render target view for the current back buffer.
	*/
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetView() const;

	/**
	* Get the back buffer resource for the current back buffer.
	*/
	Microsoft::WRL::ComPtr<ID3D12Resource> GetCurrentBackBuffer() const;


protected:
	// The Window procedure needs to call protected methods of this class.
	friend LRESULT CALLBACK WndProc(HWND arg_hwnd, UINT arg_message, WPARAM arg_wParam, LPARAM arg_lParam);

	// Only the application can create a window.
	friend class Application;
	// The DirectXTemplate class needs to register itself with a window.
	friend class Game;

	Window() = delete;

	// Register a Game with this window. This allows
	// the window to callback functions in the Game class.
	void RegisterCallbacks(std::shared_ptr<Game> arg_game);

	// Update and Draw can only be called by the application.
	virtual void OnUpdate(UpdateEventArgs& arg_e);
	virtual void OnRender(RenderEventArgs& arg_e);

	// A keyboard key was pressed
	virtual void OnKeyPressed(KeyEventArgs& arg_e);
	// A keyboard key was released
	virtual void OnKeyReleased(KeyEventArgs& arg_e);

	// The mouse was moved
	virtual void OnMouseMoved(MouseMotionEventArgs& arg_e);
	// A button on the mouse was pressed
	virtual void OnMouseButtonPressed(MouseButtonEventArgs& arg_e);
	// A button on the mouse was released
	virtual void OnMouseButtonReleased(MouseButtonEventArgs& arg_e);
	// The mouse wheel was moved.
	virtual void OnMouseWheel(MouseWheelEventArgs& arg_e);

	// The window was resized.
	virtual void OnResize(ResizeEventArgs& arg_e);

	// Create the swapchian.
	Microsoft::WRL::ComPtr<IDXGISwapChain4> CreateSwapChain();

	// Update the render target views for the swapchain back buffers.
	void UpdateRenderTargetViews();

private:
	// Windows should not be copied.
	Window(const Window& copy) = delete;
	Window& operator=(const Window& other) = delete;

	HWND h_wnd_;

	std::wstring window_name_;

	int client_width_;
	int client_height_;
	bool v_sync_;
	bool fullscreen_;

	HighResolutionClock update_clock_;
	HighResolutionClock render_clock_;
	uint64_t frame_counter_;

	std::weak_ptr<Game> game_;

	Microsoft::WRL::ComPtr<IDXGISwapChain4> dxgi_swap_chain_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> d3d12_rtv_descriptor_heap_;
	Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_back_buffers_[buffer_count_];

	UINT rtv_descriptor_size_;
	UINT current_back_buffer_index_;

	RECT window_rect_;
	bool tearing_supported_;

	Application *app_;

};