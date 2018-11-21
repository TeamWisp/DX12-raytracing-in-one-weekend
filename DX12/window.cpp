#include <window.h>
#include <application.h>
#include <game.h>
#include <command_queue.h>
#include <helpers.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#if defined(CreateWindow)
#undef CreateWindow
#endif

#include <wrl.h>
using namespace Microsoft::WRL;

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include <d3dx12.h>

// STL
#include <algorithm>
#include <cassert>
#include <chrono>
#include <map>
#include <memory>


Window::Window(HWND arg_h_wnd, const std::wstring& arg_window_name, int arg_client_width, int arg_client_height, bool arg_v_sync, Application &arg_app)
	: h_wnd_(arg_h_wnd)
	, window_name_(arg_window_name)
	, client_width_(arg_client_width)
	, client_height_(arg_client_height)
	, v_sync_(arg_v_sync)
	, fullscreen_(false)
	, frame_counter_(0)
	, app_(&arg_app)
{
	

	tearing_supported_ = app_->IsTearingSupported();

	dxgi_swap_chain_ = CreateSwapChain();
	d3d12_rtv_descriptor_heap_ = app_->CreateDescriptorHeap(buffer_count_, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	rtv_descriptor_size_ = app_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	UpdateRenderTargetViews();
}

Window::~Window()
{
	// Window should be destroyed with Application::DestroyWindow before
	// the window goes out of scope.
	assert(!h_wnd_ && "Use Application::DestroyWindow before destruction.");
}

HWND Window::GetWindowHandle() const
{
	return h_wnd_;
}

const std::wstring& Window::GetWindowName() const
{
	return window_name_;
}

void Window::Show()
{
	::ShowWindow(h_wnd_, SW_SHOW);
}

/**
* Hide the window.
*/
void Window::Hide()
{
	::ShowWindow(h_wnd_, SW_HIDE);
}

void Window::Destroy()
{
	if (auto game = game_.lock())
	{
		// Notify the registered game that the window is being destroyed.
		game->OnWindowDestroy();
	}
	if (h_wnd_)
	{
		DestroyWindow(h_wnd_);
		h_wnd_ = nullptr;
	}
}

int Window::GetClientWidth() const
{
	return client_width_;
}

int Window::GetClientHeight() const
{
	return client_height_;
}

bool Window::IsVSync() const
{
	return v_sync_;
}

void Window::SetVSync(bool arg_v_sync)
{
	v_sync_ = arg_v_sync;
}

void Window::ToggleVSync()
{
	SetVSync(!v_sync_);
}

bool Window::IsFullScreen() const
{
	return fullscreen_;
}

// Set the fullscreen state of the window.
void Window::SetFullscreen(bool arg_fullscreen)
{
	if (fullscreen_ != arg_fullscreen)
	{
		fullscreen_ = arg_fullscreen;

		if (fullscreen_) // Switching to fullscreen.
		{
			// Store the current window dimensions so they can be restored 
			// when switching out of fullscreen state.
			::GetWindowRect(h_wnd_, &window_rect_);

			// Set the window style to a borderless window so the client area fills
			// the entire screen.
			UINT window_style = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

			::SetWindowLongW(h_wnd_, GWL_STYLE, window_style);

			// Query the name of the nearest display device for the window.
			// This is required to set the fullscreen dimensions of the window
			// when using a multi-monitor setup.
			HMONITOR hMonitor = ::MonitorFromWindow(h_wnd_, MONITOR_DEFAULTTONEAREST);
			MONITORINFOEX monitor_info = { };
			monitor_info.cbSize = sizeof(MONITORINFOEX);
			::GetMonitorInfo(hMonitor, &monitor_info);

			::SetWindowPos(h_wnd_, HWND_TOPMOST,
						   monitor_info.rcMonitor.left,
						   monitor_info.rcMonitor.top,
						   monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
						   monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
						   SWP_FRAMECHANGED | SWP_NOACTIVATE);

			::ShowWindow(h_wnd_, SW_MAXIMIZE);
		}
		else
		{
			// Restore all the window decorators.
			::SetWindowLong(h_wnd_, GWL_STYLE, WS_OVERLAPPEDWINDOW);

			::SetWindowPos(h_wnd_, HWND_NOTOPMOST,
						   window_rect_.left,
						   window_rect_.top,
						   window_rect_.right - window_rect_.left,
						   window_rect_.bottom - window_rect_.top,
						   SWP_FRAMECHANGED | SWP_NOACTIVATE);

			::ShowWindow(h_wnd_, SW_NORMAL);
		}
	}
}

void Window::ToggleFullscreen()
{
	SetFullscreen(!fullscreen_);
}


void Window::RegisterCallbacks(std::shared_ptr<Game> arg_game)
{
	game_ = arg_game;

	return;
}

void Window::OnUpdate(UpdateEventArgs&)
{
	update_clock_.Tick();

	if (auto game = game_.lock())
	{
		frame_counter_++;

		UpdateEventArgs update_event_args(update_clock_.GetDeltaSeconds(), update_clock_.GetTotalSeconds());
		game->OnUpdate(update_event_args);
	}
}

void Window::OnRender(RenderEventArgs&)
{
	render_clock_.Tick();

	if (auto game = game_.lock())
	{
		RenderEventArgs render_event_args(render_clock_.GetDeltaSeconds(), render_clock_.GetTotalSeconds());
		game->OnRender(render_event_args);
	}
}

void Window::OnKeyPressed(KeyEventArgs& arg_e)
{
	if (auto game = game_.lock())
	{
		game->OnKeyPressed(arg_e);
	}
}

void Window::OnKeyReleased(KeyEventArgs& arg_e)
{
	if (auto game = game_.lock())
	{
		game->OnKeyReleased(arg_e);
	}
}

// The mouse was moved
void Window::OnMouseMoved(MouseMotionEventArgs& arg_e)
{
	if (auto game = game_.lock())
	{
		game->OnMouseMoved(arg_e);
	}
}

// A button on the mouse was pressed
void Window::OnMouseButtonPressed(MouseButtonEventArgs& arg_e)
{
	if (auto game = game_.lock())
	{
		game->OnMouseButtonPressed(arg_e);
	}
}

// A button on the mouse was released
void Window::OnMouseButtonReleased(MouseButtonEventArgs& arg_e)
{
	if (auto game = game_.lock())
	{
		game->OnMouseButtonReleased(arg_e);
	}
}

// The mouse wheel was moved.
void Window::OnMouseWheel(MouseWheelEventArgs& arg_e)
{
	if (auto game = game_.lock())
	{
		game->OnMouseWheel(arg_e);
	}
}

void Window::OnResize(ResizeEventArgs& arg_e)
{
	// Update the client size.
	if (client_width_ != arg_e.Width || client_height_ != arg_e.Height)
	{
		client_width_ = std::max(1, arg_e.Width);
		client_height_ = std::max(1, arg_e.Height);

		app_->Flush();

		for (int i = 0; i < buffer_count_; ++i)
		{
			d3d12_back_buffers_[i].Reset();
		}

		DXGI_SWAP_CHAIN_DESC swap_chain_desc = { };
		ThrowIfFailed(dxgi_swap_chain_->GetDesc(&swap_chain_desc));
		ThrowIfFailed(dxgi_swap_chain_->ResizeBuffers(buffer_count_, client_width_,
					  client_height_, swap_chain_desc.BufferDesc.Format, swap_chain_desc.Flags));

		current_back_buffer_index_ = dxgi_swap_chain_->GetCurrentBackBufferIndex();

		UpdateRenderTargetViews();
	}

	if (auto game = game_.lock())
	{
		game->OnResize(arg_e);
	}
}

Microsoft::WRL::ComPtr<IDXGISwapChain4> Window::CreateSwapChain()
{
	ComPtr<IDXGISwapChain4> dxgi_swap_chain_4;
	ComPtr<IDXGIFactory4> dxgi_factory_4;
	UINT create_factory_flags = 0;
#if defined(_DEBUG)
	create_factory_flags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(create_factory_flags, IID_PPV_ARGS(&dxgi_factory_4)));

	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = { };
	swap_chain_desc.Width = client_width_;
	swap_chain_desc.Height = client_height_;
	swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.Stereo = FALSE;
	swap_chain_desc.SampleDesc = {1, 0};
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.BufferCount = buffer_count_;
	swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	// It is recommended to always allow tearing if tearing support is available.
	swap_chain_desc.Flags = tearing_supported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	ID3D12CommandQueue* command_queue = app_->GetCommandQueue()->GetD3D12CommandQueue().Get();

	ComPtr<IDXGISwapChain1> swap_chain_1;
	ThrowIfFailed(dxgi_factory_4->CreateSwapChainForHwnd(
		command_queue,
		h_wnd_,
		&swap_chain_desc,
		nullptr,
		nullptr,
		&swap_chain_1));

	// Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
	// will be handled manually.
	ThrowIfFailed(dxgi_factory_4->MakeWindowAssociation(h_wnd_, DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swap_chain_1.As(&dxgi_swap_chain_4));

	current_back_buffer_index_ = dxgi_swap_chain_4->GetCurrentBackBufferIndex();

	return dxgi_swap_chain_4;
}

// Update the render target views for the swapchain back buffers.
void Window::UpdateRenderTargetViews()
{
	auto device = app_->GetDevice();

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(d3d12_rtv_descriptor_heap_->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < buffer_count_; ++i)
	{
		ComPtr<ID3D12Resource> back_buffer;
		ThrowIfFailed(dxgi_swap_chain_->GetBuffer(i, IID_PPV_ARGS(&back_buffer)));

		device->CreateRenderTargetView(back_buffer.Get(), nullptr, rtv_handle);

		d3d12_back_buffers_[i] = back_buffer;

		rtv_handle.Offset(rtv_descriptor_size_);
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE Window::GetCurrentRenderTargetView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(d3d12_rtv_descriptor_heap_->GetCPUDescriptorHandleForHeapStart(),
										 current_back_buffer_index_, rtv_descriptor_size_);
}

Microsoft::WRL::ComPtr<ID3D12Resource> Window::GetCurrentBackBuffer() const
{
	return d3d12_back_buffers_[current_back_buffer_index_];
}

UINT Window::GetCurrentBackBufferIndex() const
{
	return current_back_buffer_index_;
}

UINT Window::Present()
{
	UINT syncInterval = v_sync_ ? 1 : 0;
	UINT presentFlags = tearing_supported_ && !v_sync_ ? DXGI_PRESENT_ALLOW_TEARING : 0;
	ThrowIfFailed(dxgi_swap_chain_->Present(syncInterval, presentFlags));
	current_back_buffer_index_ = dxgi_swap_chain_->GetCurrentBackBufferIndex();

	return current_back_buffer_index_;
}
