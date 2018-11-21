#pragma once

#include <d3d12.h>  // For ID3D12CommandQueue, ID3D12Device2, and ID3D12Fence
#include <wrl.h>    // For Microsoft::WRL::ComPtr

#include <cstdint>  // For uint64_t
#include <queue>    // For std::queue

class CommandQueue
{
public:
	CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> arg_device, D3D12_COMMAND_LIST_TYPE arg_type);
	virtual ~CommandQueue();

	// Get an available command list from the command queue.
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetCommandList();

	// Execute a command list.
	// Returns the fence value to wait for for this command list.
	uint64_t ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> arg_command_list);

	uint64_t Signal();
	bool IsFenceComplete(uint64_t arg_fence_value);
	void WaitForFenceValue(uint64_t arg_fence_value);
	void Flush();

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const;

protected:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> arg_allocator);

private:
	// Keep track of command allocators that are "in-flight"
	struct CommandAllocatorEntry
	{
		uint64_t fence_value;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator;
	};

	using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
	using CommandListQueue = std::queue< Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> >;

	D3D12_COMMAND_LIST_TYPE                     command_list_type_;
	Microsoft::WRL::ComPtr<ID3D12Device2>       d3d12_device_;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>  d3d12_command_queue_;
	Microsoft::WRL::ComPtr<ID3D12Fence>         d3d12_fence_;
	HANDLE                                      fence_event_;
	uint64_t                                    fence_value_;

	CommandAllocatorQueue                       command_allocator_queue_;
	CommandListQueue                            command_list_queue_;
};