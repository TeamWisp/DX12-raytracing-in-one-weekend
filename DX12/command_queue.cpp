#include <command_queue.h>
#include <helpers.h>

#include <cassert>

CommandQueue::CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> arg_device, D3D12_COMMAND_LIST_TYPE arg_type)
	: fence_value_(0)
	, command_list_type_(arg_type)
	, d3d12_device_(arg_device)
{
	D3D12_COMMAND_QUEUE_DESC desc = { };
	desc.Type = arg_type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowIfFailed(d3d12_device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12_command_queue_)));
	ThrowIfFailed(d3d12_device_->CreateFence(fence_value_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12_fence_)));

	fence_event_ = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fence_event_ && "Failed to create fence event handle.");
}

CommandQueue::~CommandQueue()
{ }

Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandQueue::CreateCommandAllocator()
{
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator;
	ThrowIfFailed(d3d12_device_->CreateCommandAllocator(command_list_type_, IID_PPV_ARGS(&command_allocator)));

	return command_allocator;
}

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandQueue::CreateCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> arg_allocator)
{
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> command_list;
	ThrowIfFailed(d3d12_device_->CreateCommandList(0, command_list_type_, arg_allocator.Get(), nullptr, IID_PPV_ARGS(&command_list)));

	return command_list;
}

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandQueue::GetCommandList()
{
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> command_list;

	if (!command_allocator_queue_.empty() && IsFenceComplete(command_allocator_queue_.front().fence_value))
	{
		command_allocator = command_allocator_queue_.front().command_allocator;
		command_allocator_queue_.pop();

		ThrowIfFailed(command_allocator->Reset());
	}
	else
	{
		command_allocator = CreateCommandAllocator();
	}

	if (!command_list_queue_.empty())
	{
		command_list = command_list_queue_.front();
		command_list_queue_.pop();

		ThrowIfFailed(command_list->Reset(command_allocator.Get(), nullptr));
	}
	else
	{
		command_list = CreateCommandList(command_allocator);
	}

	// Associate the command allocator with the command list so that it can be
	// retrieved when the command list is executed.
	ThrowIfFailed(command_list->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), command_allocator.Get()));

	return command_list;
}

uint64_t CommandQueue::ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> arg_command_list)
{
	arg_command_list->Close();

	ID3D12CommandAllocator* command_allocator;
	UINT data_size = sizeof(command_allocator);
	ThrowIfFailed(arg_command_list->GetPrivateData(__uuidof(ID3D12CommandAllocator), &data_size, &command_allocator));

	ID3D12CommandList* const command_lists[] = {
		arg_command_list.Get()
	};

	d3d12_command_queue_->ExecuteCommandLists(1, command_lists);
	uint64_t fence_value = Signal();

	command_allocator_queue_.emplace(CommandAllocatorEntry{fence_value, command_allocator});
	command_list_queue_.push(arg_command_list);

	// The ownership of the command allocator has been transferred to the ComPtr
	// in the command allocator queue. It is safe to release the reference 
	// in this temporary COM pointer here.
	command_allocator->Release();

	return fence_value;
}

uint64_t CommandQueue::Signal()
{
	uint64_t fence_value = ++fence_value_;
	d3d12_command_queue_->Signal(d3d12_fence_.Get(), fence_value);
	return fence_value;
}

bool CommandQueue::IsFenceComplete(uint64_t arg_fence_value)
{
	return d3d12_fence_->GetCompletedValue() >= arg_fence_value;
}

void CommandQueue::WaitForFenceValue(uint64_t arg_fence_value)
{
	if (!IsFenceComplete(arg_fence_value))
	{
		d3d12_fence_->SetEventOnCompletion(arg_fence_value, fence_event_);
		::WaitForSingleObject(fence_event_, DWORD_MAX);
	}
}

void CommandQueue::Flush()
{
	WaitForFenceValue(Signal());
}

Microsoft::WRL::ComPtr<ID3D12CommandQueue> CommandQueue::GetD3D12CommandQueue() const
{
	return d3d12_command_queue_;
}
