/**
* The application class is used to create windows for our application.
*/
#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include <memory>
#include <string>

class Window;
class Game;
class CommandQueue;

class Application
{
public:
	// Create an application instance.
	Application(HINSTANCE arg_h_inst);

	// Destroy the application instance and all windows associated with this application.
	virtual ~Application();

	/**
	* Check to see if VSync-off is supported.
	*/
	bool IsTearingSupported() const;

	/**
	* Create a new DirectX11 render window instance.
	* @param windowName The name of the window. This name will appear in the title bar of the window. This name should be unique.
	* @param clientWidth The width (in pixels) of the window's client area.
	* @param clientHeight The height (in pixels) of the window's client area.
	* @param vSync Should the rendering be synchronized with the vertical refresh rate of the screen.
	* @param windowed If true, the window will be created in windowed mode. If false, the window will be created full-screen.
	* @returns The created window instance. If an error occurred while creating the window an invalid
	* window instance is returned. If a window with the given name already exists, that window will be
	* returned.
	*/
	std::shared_ptr<Window> CreateRenderWindow(const std::wstring& arg_window_name, int arg_client_width, int arg_client_height, bool arg_v_sync = true);

	/**
	* Destroy a window given the window name.
	*/
	void DestroyWindow(const std::wstring& windowName);
	/**
	* Destroy a window given the window reference.
	*/
	void DestroyWindow(std::shared_ptr<Window> window);

	/**
	* Find a window by the window name.
	*/
	std::shared_ptr<Window> GetWindowByName(const std::wstring& arg_window_name);

	/**
	* Run the application loop and message pump.
	* @return The error code if an error occurred.
	*/
	int Run(std::shared_ptr<Game> arg_game);

	/**
	* Request to quit the application and close all windows.
	* @param exitCode The error code to return to the invoking process.
	*/
	void Quit(int arg_exit_code = 0);

	/**
	* Get the Direct3D 12 device
	*/
	Microsoft::WRL::ComPtr<ID3D12Device2> GetDevice() const;
	/**
	* Get a command queue. Valid types are:
	* - D3D12_COMMAND_LIST_TYPE_DIRECT : Can be used for draw, dispatch, or copy commands.
	* - D3D12_COMMAND_LIST_TYPE_COMPUTE: Can be used for dispatch or copy commands.
	* - D3D12_COMMAND_LIST_TYPE_COPY   : Can be used for copy commands.
	*/
	std::shared_ptr<CommandQueue> GetCommandQueue(D3D12_COMMAND_LIST_TYPE arg_type = D3D12_COMMAND_LIST_TYPE_DIRECT) const;

	// Flush all command queues.
	void Flush();

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(UINT arg_num_descriptors, D3D12_DESCRIPTOR_HEAP_TYPE arg_type);
	UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE arg_type) const;

protected:

	Microsoft::WRL::ComPtr<IDXGIAdapter4> GetAdapter(bool arg_use_warp);
	Microsoft::WRL::ComPtr<ID3D12Device2> CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> arg_adapter);
	bool CheckTearingSupport();

private:
	Application(const Application& arg_copy) = delete;
	Application& operator=(const Application& arg_other) = delete;

	// The application instance handle that this application was created with.
	HINSTANCE h_instance_;

	Microsoft::WRL::ComPtr<IDXGIAdapter4> dxgi_adapter_;
	Microsoft::WRL::ComPtr<ID3D12Device2> d3d12_device_;

	std::shared_ptr<CommandQueue> direct_command_queue_;
	std::shared_ptr<CommandQueue> compute_command_queue_;
	std::shared_ptr<CommandQueue> copy_command_queue_;

	bool tearing_supported_;

};