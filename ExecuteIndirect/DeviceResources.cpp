#include "pch.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"

using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;

// Constructor for DeviceResources.
DX::DeviceResources::DeviceResources(HWND handle, int w, int h) :
	m_currentFrame(0),
	m_screenViewport(),
	m_rtvDescriptorSize(0),
	m_fenceEvent(0),
	m_deviceRemoved(false),
	mClientHeight(h),
	mClientWidth(w),
	mhMainWnd(handle)
{
	ZeroMemory(m_fenceValues, sizeof(m_fenceValues));
	CreateDeviceIndependentResources();
	CreateDeviceResources();
	CreateWindowSizeDependentResources();
}

/// <summary>
/// Configures resources that don't depend on the Direct3D device.
/// </summary>
void DX::DeviceResources::CreateDeviceIndependentResources()
{
}

/// <summary>
/// Configures the Direct3D device, and stores handles to it and the device context.
/// </summary>
void DX::DeviceResources::CreateDeviceResources()
{
#if defined(_DEBUG)
	// If the project is in a debug build, enable debugging via SDK Layers.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
		}
	}
#endif

	//Create the DXGI Factory Interface
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory)));

	ComPtr<IDXGIAdapter1> adapter;
	//Find DirectX12 compatible adapter
	GetHardwareAdapter(m_dxgiFactory.Get(), &adapter);

	// Create the Direct3D 12 API device object
	HRESULT hr = D3D12CreateDevice(
		adapter.Get(),					// The hardware adapter.
		D3D_FEATURE_LEVEL_11_0,			// Minimum feature level this app can support.
		IID_PPV_ARGS(&m_d3dDevice)		
		);

	if (FAILED(hr))
	{
		// If the initialization fails, fall back to the WARP device (http://go.microsoft.com/fwlink/?LinkId=286690)

		ComPtr<IDXGIAdapter> warpAdapter;
		m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)); 

		ThrowIfFailed(
			D3D12CreateDevice(
				warpAdapter.Get(),
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(&m_d3dDevice)
				)
			);
	}

	// Describe the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	// Create the command queue
	ThrowIfFailed(m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
	// Save the command queue timestamp frequency
	ThrowIfFailed(m_commandQueue->GetTimestampFrequency(&m_commandQueueTimestampFrequency));
	m_commandQueue->SetName(L"Command Queue");

	// Describe the compute command queue
	D3D12_COMMAND_QUEUE_DESC computeQueueDesc = {};
	computeQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	computeQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	// Create the compute command queue
	ThrowIfFailed(m_d3dDevice->CreateCommandQueue(&computeQueueDesc, IID_PPV_ARGS(&m_computeCommandQueue)));
	// Save the compute command queue timestamp frequency
	ThrowIfFailed(m_computeCommandQueue->GetTimestampFrequency(&m_computeCommandQueueTimestampFrequency));
	m_computeCommandQueue->SetName(L"Compute Command Queue");

	//Create command allocators for the command queues
	for (UINT n = 0; n < c_frameCount; n++)
	{
		ThrowIfFailed(m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
		ThrowIfFailed(m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_computeCommandAllocators[n])));
	}

	// Create synchronization objects.
	ThrowIfFailed(m_d3dDevice->CreateFence(m_fenceValues[m_currentFrame], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	ThrowIfFailed(m_d3dDevice->CreateFence(m_fenceValues[m_currentFrame], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_computeFence)));
	m_fenceValues[m_currentFrame]++;

	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_fenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

	//Create timestamp descriptor heap
	{
		//Describe the heap
		D3D12_QUERY_HEAP_DESC timestampHeapDesc = {};
		timestampHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		timestampHeapDesc.Count = 4;
		//Create the timestamp buffer
		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),		//in readback heap, so we can map pointer to the GPU resource
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(4 * sizeof(UINT64)),		//two start and two end times
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_timestampResultBuffer)));
		//Create the query heap
		ThrowIfFailed(m_d3dDevice->CreateQueryHeap(&timestampHeapDesc, IID_PPV_ARGS(&m_timestampQueryHeap)));
	}

}

/// <summary>
/// These resources need to be recreated every time the window size is changed.
/// </summary>
void DX::DeviceResources::CreateWindowSizeDependentResources()
{
	// Wait until all previous GPU work is complete.
	WaitForGpu();

	// Clear the previous window size specific content.
	for (UINT n = 0; n < c_frameCount; n++)
	{
		m_renderTargets[n] = nullptr;
	}
	m_rtvHeap = nullptr;

	if (m_swapChain != nullptr)
	{
		// If the swap chain already exists, resize it.
		HRESULT hr = m_swapChain->ResizeBuffers(
			c_frameCount,
			lround(mClientWidth),
			lround(mClientHeight),
			DXGI_FORMAT_B8G8R8A8_UNORM,
			0
			);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			// If the device was removed for any reason, a new device and swap chain will need to be created.
			m_deviceRemoved = true;

			// Do not continue execution of this method. DeviceResources will be destroyed and re-created.
			return;
		}
		else
		{
			ThrowIfFailed(hr);
		}
	}
	else
	{
		//Describe the swap chaib
		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		swapChainDesc.BufferDesc.Width = mClientWidth;					//width of back buffer
		swapChainDesc.BufferDesc.Height = mClientHeight;				//height of back buffer
		swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;	//format of pixel from the back buffer
		swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = c_frameCount;						//number of buffers to swap
		swapChainDesc.OutputWindow = mhMainWnd;							//the application's window
		swapChainDesc.Windowed = true;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		//Create the swap chain
		ComPtr<IDXGISwapChain> swapChain;
		ThrowIfFailed(
			m_dxgiFactory->CreateSwapChain(
				m_commandQueue.Get(),								
				&swapChainDesc,
				&swapChain
				)
			);

		ThrowIfFailed(swapChain.As(&m_swapChain));
		m_currentFrame = m_swapChain->GetCurrentBackBufferIndex();
	}

	// Create render target views of the swap chain back buffer.
	{
		//Describe the RTV heap
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = c_frameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		//Create the RTV heap
		ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
		m_rtvHeap->SetName(L"Render Target View Descriptor Heap");

		m_currentFrame = 0;
		//Descriptor to itereate in the heap
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
		m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		for (UINT n = 0; n < c_frameCount; n++)
		{
			//Create render target view to the swap chain's buffers
			ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_d3dDevice->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvDescriptor);
			//Offset the descriptor for the next RTV
			rtvDescriptor.Offset(m_rtvDescriptorSize);

			WCHAR name[25];
			swprintf_s(name, L"Render Target %d", n);
			m_renderTargets[n]->SetName(name);
		}
	}

	// Create a depth stencil view.
	{
		//Describe the depth stencil heap
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		//Create the depth stencil heap
		ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

		//Describe the depth buffer - same size as the back buffers
		D3D12_HEAP_PROPERTIES depthHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_DESC depthResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_D32_FLOAT,
			static_cast<UINT>(mClientWidth),
			static_cast<UINT>(mClientHeight),
			1,
			0,
			1,
			0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		//Describe a clear value - for depth it is the maximum value for depth - 1.0f
		D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
		depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
		depthOptimizedClearValue.DepthStencil.Stencil = 0;

		//Create the depth stencil texture 
		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
			&depthHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&depthResourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&m_depthStencil)
			));
		//Describe the depth stencil view
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		//Create the view in the heap
		m_d3dDevice->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	}


	// All pending GPU work was already finished. Update the tracked fence values
	// to the last value signaled.
	for (UINT n = 0; n < c_frameCount; n++)
	{
		m_fenceValues[n] = m_fenceValues[m_currentFrame];
	}

	// Set the 3D rendering viewport to target the entire window.
	m_screenViewport = { 0.0f, 0.0f, (FLOAT)mClientWidth, (FLOAT)mClientHeight, 0.0f, 1.0f };
}

/// <summary>
/// Updates the size of the render target.
/// </summary>
/// <param name="width">The new width.</param>
/// <param name="height">The new height.</param>
void DX::DeviceResources::UpdateRenderTargetSize(int width, int height) {
	mClientWidth = width;
	mClientHeight = height;
	CreateWindowSizeDependentResources();
}

/// <summary>
/// This method is called in the event handler for the DisplayContentsInvalidated event.
/// </summary>
void DX::DeviceResources::ValidateDevice()
{
	// The D3D Device is no longer valid if the default adapter changed since the device
	// was created or if the device has been removed.

	// First, get the LUID (locally unique identifier) for the adapter from when the device was created.
	LUID previousAdapterLuid = m_d3dDevice->GetAdapterLuid();

	// Next, get the information for the current default adapter.

	ComPtr<IDXGIFactory2> currentFactory;
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&currentFactory)));

	ComPtr<IDXGIAdapter1> currentDefaultAdapter;
	ThrowIfFailed(currentFactory->EnumAdapters1(0, &currentDefaultAdapter));

	DXGI_ADAPTER_DESC currentDesc;
	ThrowIfFailed(currentDefaultAdapter->GetDesc(&currentDesc));

	// If the adapter LUIDs don't match, or if the device reports that it has been removed,
	// a new D3D device must be created.

	if (previousAdapterLuid.LowPart != currentDesc.AdapterLuid.LowPart ||
		previousAdapterLuid.HighPart != currentDesc.AdapterLuid.HighPart ||
		FAILED(m_d3dDevice->GetDeviceRemovedReason()))
	{
		m_deviceRemoved = true;
	}
}

/// <summary>
/// Wait for pending GPU work to complete.
/// </summary>
void DX::DeviceResources::WaitForGpu()
{
	// Schedule a Signal command in the queue.
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_currentFrame]));

	// Wait until the fence has been crossed.
	ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_currentFrame], m_fenceEvent));
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	// Increment the fence value for the current frame.
	m_fenceValues[m_currentFrame]++;
}

/// <summary>
/// Present the current back buffer.
/// </summary>
void DX::DeviceResources::Present()
{

	// The first argument instructs DXGI to block until VSync, putting the application
	// to sleep until the next VSync. This ensures we don't waste any cycles rendering
	// frames that will never be displayed to the screen.
	HRESULT hr = m_swapChain->Present(0, 0);

	// If the device was removed either by a disconnection or a driver upgrade, we 
	// must recreate all device resources.
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		HRESULT why = m_d3dDevice->GetDeviceRemovedReason();
		m_deviceRemoved = true;
	}
	else
	{
		ThrowIfFailed(hr);

		MoveToNextFrame();
	}
}

/// <summary>
/// Reads the timestamps.
/// </summary>
/// <param name="isComputeQueue">Used to choose the timestamp frequency</param>
void DX::DeviceResources::ReadTimestamps(bool isComputeQueue) {
	D3D12_RANGE readRange = {};
	const D3D12_RANGE emptyRange = {};

	readRange.Begin = 0;
	readRange.End = 4 * sizeof(UINT64);

	void* pData = nullptr;
	//Map GPU resource to pointer - this can be done because the GPU resource is in the readback heap
	ThrowIfFailed(m_timestampResultBuffer->Map(0, &readRange, &pData));

	const UINT64* pTimestamps = reinterpret_cast<UINT64*>(static_cast<UINT8*>(pData) + readRange.Begin);
	UINT64 timeStampDelta;
	//Choose which data to read
	if(isComputeQueue)
		timeStampDelta = pTimestamps[3] - pTimestamps[2];
	else
		timeStampDelta = pTimestamps[1] - pTimestamps[0];

	// Unmap with an empty range (written range).
	m_timestampResultBuffer->Unmap(0, &emptyRange);
	// Calculate the time in miliseconds
	const UINT64 gpuTimeUS = (timeStampDelta * 1000000) / (isComputeQueue ? m_computeCommandQueueTimestampFrequency : m_commandQueueTimestampFrequency);
	OutputDebugStringW(std::to_wstring(gpuTimeUS).c_str());
	OutputDebugStringW(L"\n");
}

/// <summary>
/// Prepare to render the next frame.
/// </summary>
void DX::DeviceResources::MoveToNextFrame()
{
	// Schedule a Signal command in the queue.
	const UINT64 currentFenceValue = m_fenceValues[m_currentFrame];
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

	// Advance the frame index.
	//m_currentFrame = (m_currentFrame + 1) % c_frameCount;
	m_currentFrame = m_swapChain->GetCurrentBackBufferIndex();

	// Check to see if the next frame is ready to start.
	if (m_fence->GetCompletedValue() < m_fenceValues[m_currentFrame])
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_currentFrame], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame.
	m_fenceValues[m_currentFrame] = currentFenceValue + 1;
	//OutputDebugStringW(L"Command list execute time: ");
	//ReadTimestamps(false);
}

/// <summary>
/// This method acquires the first available hardware adapter that supports Direct3D 12.
/// If no such adapter can be found, *ppAdapter will be set to nullptr.
/// </summary>
/// <param name="pFactory">The DXGI factory interface.</param>
/// <param name="ppAdapter">The DXGI adapter interface.</param>
void DX::DeviceResources::GetHardwareAdapter(IDXGIFactory4* pFactory, IDXGIAdapter1** ppAdapter)
{
	ComPtr<IDXGIAdapter1> adapter;
	*ppAdapter = nullptr;

	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Don't select the Basic Render Driver adapter.
			continue;
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the
		// actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

	*ppAdapter = adapter.Detach();
}
