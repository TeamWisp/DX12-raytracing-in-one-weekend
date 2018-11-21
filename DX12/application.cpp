#include <application.h>
#include <window.h>
#include <game.h>
#include <command_queue.h>
#include <helpers.h>
#include <events.h>
#include <defines.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h> // For CommandLineToArgvW

// The min/max macros conflict with like-named member functions.
// Only use std::min and std::max defined in <algorithm>.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

// In order to define a function called CreateWindow, the Windows macro needs to
// be undefined.
#if defined(CreateWindow)
#undef CreateWindow
#endif

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using namespace Microsoft::WRL;

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// D3D12 extension library.
#include <d3dx12.h>

// STL
#include <memory>
#include <map>

//#include "..\resource.h"

//#include <Game.h>
//#include <CommandQueue.h>
//#include <Window.h>

constexpr wchar_t WINDOW_CLASS_NAME[] = L"DX12RenderWindowClass";

using WindowPtr = std::shared_ptr<Window>;
using WindowMap = std::map<HWND, WindowPtr>;
using WindowNameMap = std::map<std::wstring, WindowPtr>;

static WindowMap gs_windows;
static WindowNameMap gs_window_by_name;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

Application::Application(HINSTANCE arg_h_inst)
	: h_instance_(arg_h_inst)
	, tearing_supported_(false)
{
	// Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
	// Using this awareness context allows the client area of the window 
	// to achieve 100% scaling while still allowing non-client window content to 
	// be rendered in a DPI sensitive fashion.
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

#if defined(_DEBUG)
	// Always enable the debug layer before doing anything DX12 related
	// so all possible errors generated while creating DX12 objects
	// are caught by the debug layer.
	ComPtr<ID3D12Debug> debug_interface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface)));
	debug_interface->EnableDebugLayer();
#endif

	// Register window
	WNDCLASSEXW wndClass = {0};

	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = &WndProc;
	wndClass.hInstance = h_instance_;
	wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wndClass.hIcon = LoadIcon(h_instance_, MAKEINTRESOURCE(APP_ICON));
	wndClass.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
	wndClass.lpszMenuName = nullptr;
	wndClass.lpszClassName = WINDOW_CLASS_NAME;
	wndClass.hIconSm = LoadIcon(h_instance_, MAKEINTRESOURCE(APP_ICON));

	if (!RegisterClassExW(&wndClass))
	{
		MessageBoxA(NULL, "Unable to register the window class.", "Error", MB_OK | MB_ICONERROR);
	}

	dxgi_adapter_ = GetAdapter(false);
	if (dxgi_adapter_)
	{
		d3d12_device_ = CreateDevice(dxgi_adapter_);
	}
	if (d3d12_device_)
	{
		direct_command_queue_ = std::make_shared<CommandQueue>(d3d12_device_, D3D12_COMMAND_LIST_TYPE_DIRECT);
		compute_command_queue_ = std::make_shared<CommandQueue>(d3d12_device_, D3D12_COMMAND_LIST_TYPE_COMPUTE);
		copy_command_queue_ = std::make_shared<CommandQueue>(d3d12_device_, D3D12_COMMAND_LIST_TYPE_COPY);

		tearing_supported_ = CheckTearingSupport();
	}
}

Application::~Application()
{
	Flush();
}

Microsoft::WRL::ComPtr<IDXGIAdapter4> Application::GetAdapter(bool bUseWarp)
{
	ComPtr<IDXGIFactory4> dxgiFactory;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	ComPtr<IDXGIAdapter1> dxgiAdapter1;
	ComPtr<IDXGIAdapter4> dxgiAdapter4;

	if (bUseWarp)
	{
		ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
		ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
	}
	else
	{
		SIZE_T maxDedicatedVideoMemory = 0;
		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			// Check to see if the adapter can create a D3D12 device without actually 
			// creating it. The adapter with the largest dedicated video memory
			// is favored.
			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
				D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
				dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
			}
		}
	}

	return dxgiAdapter4;
}
Microsoft::WRL::ComPtr<ID3D12Device2> Application::CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter)
{
	ComPtr<ID3D12Device2> d3d12Device2;
	ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));
	//    NAME_D3D12_OBJECT(d3d12Device2);

	// Enable debug messages in debug mode.
#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> pInfoQueue;
	if (SUCCEEDED(d3d12Device2.As(&pInfoQueue)))
	{
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		// Suppress whole categories of messages
		//D3D12_MESSAGE_CATEGORY Categories[] = {};

		// Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY Severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID DenyIds[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
		};

		D3D12_INFO_QUEUE_FILTER NewFilter = { };
		//NewFilter.DenyList.NumCategories = _countof(Categories);
		//NewFilter.DenyList.pCategoryList = Categories;
		NewFilter.DenyList.NumSeverities = _countof(Severities);
		NewFilter.DenyList.pSeverityList = Severities;
		NewFilter.DenyList.NumIDs = _countof(DenyIds);
		NewFilter.DenyList.pIDList = DenyIds;

		ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
	}
#endif

	return d3d12Device2;
}

bool Application::CheckTearingSupport()
{
	BOOL allowTearing = FALSE;

	// Rather than create the DXGI 1.5 factory interface directly, we create the
	// DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
	// graphics debugging tools which will not support the 1.5 factory interface 
	// until a future update.
	ComPtr<IDXGIFactory4> factory4;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
	{
		ComPtr<IDXGIFactory5> factory5;
		if (SUCCEEDED(factory4.As(&factory5)))
		{
			factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
										  &allowTearing, sizeof(allowTearing));
		}
	}

	return allowTearing == TRUE;
}

bool Application::IsTearingSupported() const
{
	return tearing_supported_;
}

std::shared_ptr<Window> Application::CreateRenderWindow(const std::wstring& arg_window_name, int arg_client_width, int arg_client_height, bool arg_v_sync)
{
	// First check if a window with the given name already exists.
	WindowNameMap::iterator window_iter = gs_window_by_name.find(arg_window_name);
	if (window_iter != gs_window_by_name.end())
	{
		return window_iter->second;
	}

	RECT window_rect = {0, 0, arg_client_width, arg_client_height};
	AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

	HWND h_wnd = CreateWindowW(WINDOW_CLASS_NAME, arg_window_name.c_str(),
							  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
							   window_rect.right - window_rect.left,
							   window_rect.bottom - window_rect.top,
							  nullptr, nullptr, h_instance_, nullptr);

	if (!h_wnd)
	{
		MessageBoxA(NULL, "Could not create the render window.", "Error", MB_OK | MB_ICONERROR);
		return nullptr;
	}

	WindowPtr p_window = std::make_shared<Window>(h_wnd, arg_window_name, arg_client_width, arg_client_height, arg_v_sync, *this);

	gs_windows.insert(WindowMap::value_type(h_wnd, p_window));
	gs_window_by_name.insert(WindowNameMap::value_type(arg_window_name, p_window));

	return p_window;
}

void Application::DestroyWindow(std::shared_ptr<Window> arg_window)
{
	if (arg_window) arg_window->Destroy();
}

void Application::DestroyWindow(const std::wstring& arg_window_name)
{
	WindowPtr p_window = GetWindowByName(arg_window_name);
	if (p_window)
	{
		DestroyWindow(p_window);
	}
}

std::shared_ptr<Window> Application::GetWindowByName(const std::wstring& arg_window_name)
{
	std::shared_ptr<Window> window;
	WindowNameMap::iterator iter = gs_window_by_name.find(arg_window_name);
	if (iter != gs_window_by_name.end())
	{
		window = iter->second;
	}

	return window;
}


int Application::Run(std::shared_ptr<Game> arg_game)
{
	if (!arg_game->Initialize()) return 1;
	if (!arg_game->LoadContent()) return 2;

	MSG msg = {0};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	// Flush any commands in the commands queues before quiting.
	Flush();

	arg_game->UnloadContent();
	arg_game->Destroy();

	return static_cast<int>(msg.wParam);
}

void Application::Quit(int exitCode)
{
	PostQuitMessage(exitCode);
}

Microsoft::WRL::ComPtr<ID3D12Device2> Application::GetDevice() const
{
	return d3d12_device_;
}

std::shared_ptr<CommandQueue> Application::GetCommandQueue(D3D12_COMMAND_LIST_TYPE arg_type) const
{
	std::shared_ptr<CommandQueue> command_queue;
	switch (arg_type)
	{
		case D3D12_COMMAND_LIST_TYPE_DIRECT:
			command_queue = direct_command_queue_;
			break;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE:
			command_queue = compute_command_queue_;
			break;
		case D3D12_COMMAND_LIST_TYPE_COPY:
			command_queue = copy_command_queue_;
			break;
		default:
			assert(false && "Invalid command queue type.");
	}

	return command_queue;
}

void Application::Flush()
{
	direct_command_queue_->Flush();
	compute_command_queue_->Flush();
	copy_command_queue_->Flush();
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Application::CreateDescriptorHeap(UINT arg_num_descriptors, D3D12_DESCRIPTOR_HEAP_TYPE arg_type)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = { };
	desc.Type = arg_type;
	desc.NumDescriptors = arg_num_descriptors;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	ThrowIfFailed(d3d12_device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

	return descriptorHeap;
}

UINT Application::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE arg_type) const
{
	return d3d12_device_->GetDescriptorHandleIncrementSize(arg_type);
}


// Remove a window from our window lists.
static void RemoveWindow(HWND hWnd)
{
	WindowMap::iterator window_iter = gs_windows.find(hWnd);
	if (window_iter != gs_windows.end())
	{
		WindowPtr p_window = window_iter->second;
		gs_window_by_name.erase(p_window->GetWindowName());
		gs_windows.erase(window_iter);
	}
}

// Convert the message ID into a MouseButton ID
MouseButtonEventArgs::MouseButton DecodeMouseButton(UINT arg_message_id)
{
	MouseButtonEventArgs::MouseButton mouse_button = MouseButtonEventArgs::None;
	switch (arg_message_id)
	{
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_LBUTTONDBLCLK:
			{
				mouse_button = MouseButtonEventArgs::Left;
			}
			break;
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_RBUTTONDBLCLK:
			{
				mouse_button = MouseButtonEventArgs::Right;
			}
			break;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MBUTTONDBLCLK:
			{
				mouse_button = MouseButtonEventArgs::Middel;
			}
			break;
	}

	return mouse_button;
}

static LRESULT CALLBACK WndProc(HWND arg_hwnd, UINT arg_message, WPARAM arg_wParam, LPARAM arg_lParam)
{
	WindowPtr p_window;
	{
		WindowMap::iterator iter = gs_windows.find(arg_hwnd);
		if (iter != gs_windows.end())
		{
			p_window = iter->second;
		}
	}

	if (p_window)
	{
		switch (arg_message)
		{
			case WM_PAINT:
				{
					// Delta time will be filled in by the Window.
					UpdateEventArgs updateEventArgs(0.0f, 0.0f);
					p_window->OnUpdate(updateEventArgs);
					RenderEventArgs renderEventArgs(0.0f, 0.0f);
					// Delta time will be filled in by the Window.
					p_window->OnRender(renderEventArgs);
				}
				break;
			case WM_SYSKEYDOWN:
			case WM_KEYDOWN:
				{
					MSG char_msg;
					// Get the Unicode character (UTF-16)
					unsigned int c = 0;
					// For printable characters, the next message will be WM_CHAR.
					// This message contains the character code we need to send the KeyPressed event.
					// Inspired by the SDL 1.2 implementation.
					if (PeekMessage(&char_msg, arg_hwnd, 0, 0, PM_NOREMOVE) && char_msg.message == WM_CHAR)
					{
						GetMessage(&char_msg, arg_hwnd, 0, 0);
						c = static_cast<unsigned int>(char_msg.wParam);
					}
					bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
					bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
					bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
					KeyCode::Key key = (KeyCode::Key)arg_wParam;
					unsigned int scan_code = (arg_lParam & 0x00FF0000) >> 16;
					KeyEventArgs key_event_args(key, c, KeyEventArgs::Pressed, shift, control, alt);
					p_window->OnKeyPressed(key_event_args);
				}
				break;
			case WM_SYSKEYUP:
			case WM_KEYUP:
				{
					bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
					bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
					bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
					KeyCode::Key key = (KeyCode::Key)arg_wParam;
					unsigned int c = 0;
					unsigned int scan_code = (arg_lParam & 0x00FF0000) >> 16;

					// Determine which key was released by converting the key code and the scan code
					// to a printable character (if possible).
					// Inspired by the SDL 1.2 implementation.
					unsigned char keyboard_state[256];
					GetKeyboardState(keyboard_state);
					wchar_t translated_characters[4];
					if (int result = ToUnicodeEx(static_cast<UINT>(arg_wParam), scan_code, keyboard_state, translated_characters, 4, 0, NULL) > 0)
					{
						c = translated_characters[0];
					}

					KeyEventArgs key_event_args(key, c, KeyEventArgs::Released, shift, control, alt);
					p_window->OnKeyReleased(key_event_args);
				}
				break;
				// The default window procedure will play a system notification sound 
				// when pressing the Alt+Enter keyboard combination if this message is 
				// not handled.
			case WM_SYSCHAR:
				break;
			case WM_MOUSEMOVE:
				{
					bool l_button = (arg_wParam & MK_LBUTTON) != 0;
					bool r_button = (arg_wParam & MK_RBUTTON) != 0;
					bool m_button = (arg_wParam & MK_MBUTTON) != 0;
					bool shift = (arg_wParam & MK_SHIFT) != 0;
					bool control = (arg_wParam & MK_CONTROL) != 0;

					int x = ((int) (short) LOWORD(arg_lParam));
					int y = ((int) (short) HIWORD(arg_lParam));

					MouseMotionEventArgs mouse_motion_event_args(l_button, m_button, r_button, control, shift, x, y);
					p_window->OnMouseMoved(mouse_motion_event_args);
				}
				break;
			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
			case WM_MBUTTONDOWN:
				{
					bool l_button = (arg_wParam & MK_LBUTTON) != 0;
					bool r_button = (arg_wParam & MK_RBUTTON) != 0;
					bool m_button = (arg_wParam & MK_MBUTTON) != 0;
					bool shift = (arg_wParam & MK_SHIFT) != 0;
					bool control = (arg_wParam & MK_CONTROL) != 0;

					int x = ((int) (short) LOWORD(arg_lParam));
					int y = ((int) (short) HIWORD(arg_lParam));

					MouseButtonEventArgs mouse_button_event_args(DecodeMouseButton(arg_message), MouseButtonEventArgs::Pressed, l_button, m_button, r_button, control, shift, x, y);
					p_window->OnMouseButtonPressed(mouse_button_event_args);
				}
				break;
			case WM_LBUTTONUP:
			case WM_RBUTTONUP:
			case WM_MBUTTONUP:
				{
					bool lButton = (arg_wParam & MK_LBUTTON) != 0;
					bool rButton = (arg_wParam & MK_RBUTTON) != 0;
					bool mButton = (arg_wParam & MK_MBUTTON) != 0;
					bool shift = (arg_wParam & MK_SHIFT) != 0;
					bool control = (arg_wParam & MK_CONTROL) != 0;

					int x = ((int) (short) LOWORD(arg_lParam));
					int y = ((int) (short) HIWORD(arg_lParam));

					MouseButtonEventArgs mouse_button_event_args(DecodeMouseButton(arg_message), MouseButtonEventArgs::Released, lButton, mButton, rButton, control, shift, x, y);
					p_window->OnMouseButtonReleased(mouse_button_event_args);
				}
				break;
			case WM_MOUSEWHEEL:
				{
					// The distance the mouse wheel is rotated.
					// A positive value indicates the wheel was rotated to the right.
					// A negative value indicates the wheel was rotated to the left.
					float z_delta = ((int) (short) HIWORD(arg_wParam)) / (float) WHEEL_DELTA;
					short key_states = (short) LOWORD(arg_wParam);

					bool l_button = (key_states & MK_LBUTTON) != 0;
					bool r_button = (key_states & MK_RBUTTON) != 0;
					bool m_button = (key_states & MK_MBUTTON) != 0;
					bool shift = (key_states & MK_SHIFT) != 0;
					bool control = (key_states & MK_CONTROL) != 0;

					int x = ((int) (short) LOWORD(arg_lParam));
					int y = ((int) (short) HIWORD(arg_lParam));

					// Convert the screen coordinates to client coordinates.
					POINT client_to_screen_point;
					client_to_screen_point.x = x;
					client_to_screen_point.y = y;
					ScreenToClient(arg_hwnd, &client_to_screen_point);

					MouseWheelEventArgs mouse_wheel_event_args(z_delta, l_button, m_button, r_button, control, shift, (int) client_to_screen_point.x, (int) client_to_screen_point.y);
					p_window->OnMouseWheel(mouse_wheel_event_args);
				}
				break;
			case WM_SIZE:
				{
					int width = ((int) (short) LOWORD(arg_lParam));
					int height = ((int) (short) HIWORD(arg_lParam));

					ResizeEventArgs resize_event_args(width, height);
					p_window->OnResize(resize_event_args);
				}
				break;
			case WM_DESTROY:
				{
					// If a window is being destroyed, remove it from the 
					// window maps.
					RemoveWindow(arg_hwnd);

					if (gs_windows.empty())
					{
						// If there are no more windows, quit the application.
						PostQuitMessage(0);
					}
				}
				break;
			default:
				return DefWindowProcW(arg_hwnd, arg_message, arg_wParam, arg_lParam);
		}
	}
	else
	{
		return DefWindowProcW(arg_hwnd, arg_message, arg_wParam, arg_lParam);
	}

	return 0;
}