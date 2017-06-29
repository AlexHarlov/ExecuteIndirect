#pragma once
#include <dxgidebug.h>

namespace DX
{
	static const UINT c_frameCount = 3;		// Use triple buffering.

	// Controls all the DirectX device resources.
	class DeviceResources
	{
	public:
		DeviceResources(HWND handle, int w, int h);
		void ValidateDevice();
		void WaitForGpu();
		void Present();

		bool						IsDeviceRemoved() const				{ return m_deviceRemoved; }

		// D3D Accessors.
		ID3D12Device*				GetD3DDevice() const				{ return m_d3dDevice.Get(); }
		IDXGISwapChain1*			GetSwapChain() const				{ return m_swapChain.Get(); }
		ID3D12Resource*				GetRenderTarget() const				{ return m_renderTargets[m_currentFrame].Get(); }
		ID3D12Resource*				GetDepthStencil() const				{ return m_depthStencil.Get(); }
		ID3D12CommandQueue*			GetCommandQueue() const				{ return m_commandQueue.Get(); }
		ID3D12CommandQueue*			GetComputeCommandQueue() const		{ return m_computeCommandQueue.Get(); }
		ID3D12CommandAllocator*		GetCommandAllocator() const			{ return m_commandAllocators[m_currentFrame].Get(); }
		ID3D12CommandAllocator*		GetComputeCommandAllocator() const	{ return m_computeCommandAllocators[m_currentFrame].Get(); }
		ID3D12QueryHeap*			GetTimestampQueryHeap() const		{ return m_timestampQueryHeap.Get(); }
		ID3D12Resource*				GetTimestampResultBuffer() const	{ return m_timestampResultBuffer.Get(); }
		D3D12_VIEWPORT*				GetScreenViewport() 				{ return &m_screenViewport; }
		UINT						GetCurrentFrameIndex() const		{ return m_currentFrame; }
		ID3D12Fence*				GetFence() const					{ return m_fence.Get(); }
		ID3D12Fence*				GetComputeFence() const				{ return m_computeFence.Get(); }
		UINT64						GetFenceValue() const				{ return m_fenceValues[m_currentFrame]; }
		UINT64						GetCommandQueueTimestampFrequency() const { return m_commandQueueTimestampFrequency; }
		UINT64						GetComputeCommandQueueTimestampFrequency() const { return m_computeCommandQueueTimestampFrequency; }
		INT							GetRenderTargetWidth() const { return mClientWidth; }
		INT							GetRenderTargetHeight() const { return mClientHeight; }

		void ReadTimestamps(bool isComputeQueue);
		void UpdateRenderTargetSize(int width, int height);

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const
		{
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_currentFrame, m_rtvDescriptorSize);
		}
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const
		{
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
		}

	private:
		void CreateDeviceIndependentResources();
		void CreateDeviceResources();
		void CreateWindowSizeDependentResources();

		

		void MoveToNextFrame();
		void GetHardwareAdapter(IDXGIFactory4* pFactory, IDXGIAdapter1** ppAdapter);

		UINT											m_currentFrame;

		// Direct3D objects.
		Microsoft::WRL::ComPtr<ID3D12Device>			m_d3dDevice;
		Microsoft::WRL::ComPtr<IDXGIFactory4>			m_dxgiFactory;
		Microsoft::WRL::ComPtr<IDXGISwapChain3>			m_swapChain;
		Microsoft::WRL::ComPtr<ID3D12Resource>			m_renderTargets[c_frameCount];
		Microsoft::WRL::ComPtr<ID3D12Resource>			m_depthStencil;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	m_rtvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	m_dsvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	m_cbvSrvUavHeap;
		UINT											m_rtvDescriptorSize;
		UINT											m_cbvSrvUavDescriptorSize;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>		m_commandQueue;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>		m_computeCommandQueue;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>	m_commandAllocators[c_frameCount];
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>	m_computeCommandAllocators[c_frameCount];
		Microsoft::WRL::ComPtr<IDXGIDebug>				m_dxgiDebug;
		D3D12_VIEWPORT									m_screenViewport;
		bool											m_deviceRemoved;

		//GPU timestamps
		Microsoft::WRL::ComPtr<ID3D12QueryHeap>							m_timestampQueryHeap;
		Microsoft::WRL::ComPtr<ID3D12Resource>							m_timestampResultBuffer;

		// CPU/GPU Synchronization.
		Microsoft::WRL::ComPtr<ID3D12Fence>				m_fence;
		Microsoft::WRL::ComPtr<ID3D12Fence>				m_computeFence;
		UINT64											m_fenceValues[c_frameCount];
		HANDLE											m_fenceEvent;
		UINT64											m_commandQueueTimestampFrequency;
		UINT64											m_computeCommandQueueTimestampFrequency;

		// Cached device properties.
		HWND      mhMainWnd = nullptr;
		int mClientWidth;
		int mClientHeight;

		
		
	};
}
