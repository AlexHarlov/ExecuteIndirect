#pragma once 
#include <d3d12.h>
#include "DeviceResources.h"
#include "ShaderStructures.h"

using namespace Microsoft::WRL;
namespace ExecuteIndirect
{
	class FrameResources;
	class RenderItem;

	class HiZBuffer
	{
	public:
		HiZBuffer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void InitMIPMap();
		void InitDepth();
		void RenderOccluders(FrameResources* currFrameResource, UINT occluderCount);
		void RenderMIPMap();
		UINT GetHIZBufferHeight() { return m_height; }
		UINT GetHIZBufferWidth() { return m_width; }

		CD3DX12_STATIC_SAMPLER_DESC* GetSamplerDesc() { return &m_HizSamplerDesc; }
		D3D12_SHADER_RESOURCE_VIEW_DESC* GetMIPMapChainDesc() { return &m_MIPMapChainDesc; }
		ComPtr<ID3D12Resource>&	 GetHiZBuffer() { return m_pHizBuffer; }
		ComPtr<ID3D12Resource>&	 GetOccluderCommandBuffer() { return OccluderCommandBuffer; }
		ComPtr<ID3D12Resource>&	 GetOccluderCommandBufferUploader() { return OccluderCommandBufferUploader; }


	private:
		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		ComPtr<ID3D12CommandAllocator>	    m_HiZDepthCommandAllocator;
		ComPtr<ID3D12CommandAllocator>	    m_HiZMIPMapCommandAllocator;
		ComPtr<ID3D12GraphicsCommandList>	m_HiZDepthCommandList;
		ComPtr<ID3D12GraphicsCommandList>	m_HiZMIPMapCommandList;

		ComPtr<ID3D12PipelineState>			m_HiZDepthPSO;				//used for rendering potential occluders 
		ComPtr<ID3D12PipelineState>			m_HiZMIPMapPSO;				//used for creating the mipmap
		ComPtr<ID3D12Resource>				m_pHizBuffer;
		ComPtr<ID3D12Resource>				m_pTempBuffer;
		ComPtr<ID3D12Resource>				OccluderCommandBuffer;
		ComPtr<ID3D12Resource>				OccluderCommandBufferUploader;
		
		ComPtr<ID3D12Resource>				m_pHizDepthBuffer;
		ComPtr<ID3D12Resource>				m_pMIPMapVertexBuffer;
		ComPtr<ID3D12Resource>				m_PotentialOccludersIndexBuffer;
		ComPtr<ID3D12Resource>				m_MIPMapVertexBufferUpload;
		ComPtr<ID3D12RootSignature>			m_HiZMIPMapRootSignature;
		ComPtr<ID3D12RootSignature>			m_HiZDepthRootSignature;
		ComPtr<ID3D12CommandSignature>		m_HiZDepthCommandSignature;
		ComPtr<ID3D12DescriptorHeap>		m_HiZDepthDSVHeap;
		ComPtr<ID3D12DescriptorHeap>		m_MIPMapRTVHeap;
		ComPtr<ID3D12DescriptorHeap>		m_MIPMapSRVHeap;
		ComPtr<ID3D12Fence>					m_fence;


		D3D12_SHADER_RESOURCE_VIEW_DESC     m_MIPMapChainDesc;
		CD3DX12_STATIC_SAMPLER_DESC			m_HizSamplerDesc;
		D3D12_VERTEX_BUFFER_VIEW			m_MIPMapVertexBufferView;
		D3D12_INDEX_BUFFER_VIEW			    m_PotentialOccludersIndexBufferView;
		std::vector<byte>					m_HiZMIPMapVS;
		std::vector<byte>					m_HiZMIPMapPS;
		std::vector<byte>					m_HiZDepthVS;
		std::vector<byte>					m_HiZDepthPS;

		UINT m_width;
		UINT m_height;
		UINT m_mipLevels;
		UINT m_MIPMapSRVDescriptorSize;
		UINT m_MIPMapRTVDescriptorSize;
		UINT m_fenceValue;
		HANDLE m_fenceEvent;
		void CreateResources();
	};
}