#pragma once

#include "DeviceResources.h"
#include "ShaderStructures.h"
#include "Camera.h"
#include "HiZBuffer.h"
#include "OBJLoader.h"
#include "FrameResources.h"
#include "Scene.h"

using namespace Microsoft::WRL;

namespace ExecuteIndirect
{
	// This sample renderer instantiates a basic rendering pipeline.
	class Renderer
	{
	public:
		Renderer(const std::shared_ptr<DX::DeviceResources>& deviceResources,
				 const std::shared_ptr<Camera>& camera);
		~Renderer();
		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources();
		void UpdateMaterialBuffer();
		void UpdateSceneCB();
		void Update();
		void ChangeCulling();
		bool Render();
		UINT GetTotalDrawnInstances();

	private:
		void PopulateCommandLists(); 
		void InitializeHiZBuffer();
		void BuildIndirectCommands();
		void UpdateIndirectCommands();
		void LoadTextures();
		void BuildDescriptorHeaps();
		void BuildRenderItems();
		void BuildPSOAndCommandLists();
		void CreateRootSignatures();
		void CreateCommandSignature();
		std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
		void BuildFrameResources();

	private:
		bool m_enableCulling;
		bool m_justChangedCulling;

		// Graphics root signature parameter offsets.
		enum GraphicsRootParameters
		{
			Graphics_SceneCBV,	
			Graphics_MaterialsSRV,
			Graphics_InstanceData,
			Graphics_TextureTable,
			Graphics_RootParametersCount
		};

		// Compute root signature parameter offsets.
		enum ComputeRootParameters
		{
			Compute_SceneCBV,
			Compute_HiZBufferSRV,
			Compute_InstanceDataTable,
			Compute_Constants,
			Compute_RootParametersCount
		};
		
		static const UINT ComputeThreadBlockSize = 64;		// Should match the value in compute.hlsl.

		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		//Cached pointer to the camera
		std::shared_ptr<Camera> m_Camera;

		ComPtr<ID3D12GraphicsCommandList>	m_commandList;
		ComPtr<ID3D12GraphicsCommandList>	m_computeCommandList;
		ComPtr<ID3D12CommandSignature>		m_commandSignature;
		ComPtr<ID3D12CommandSignature>		m_computeCommandSignature;
		ComPtr<ID3D12RootSignature>			m_rootSignature;
		ComPtr<ID3D12RootSignature>			m_computeRootSignature;

		ComPtr<ID3D12PipelineState>			m_pipelineState;
		ComPtr<ID3D12PipelineState>			m_computePipelineState;
		ComPtr<ID3D12DescriptorHeap>		m_InstanceDataHeap;
		ComPtr<ID3D12DescriptorHeap>		m_srvTextureHeap;
		ComPtr<ID3D12DescriptorHeap>		m_srvHiZMapHeap;
		ComPtr<ID3D12Resource>				CommandBuffer;
		ComPtr<ID3D12Resource>				CommandBufferUploader;
		ComPtr<ID3D12Resource>				DrawnInstancesReadbackBuffer;
		SceneConstantBuffer					m_SceneBufferData;
		OBJLoader							m_Loader;

		std::unordered_map<std::string, std::unique_ptr<RenderItem>>	m_renderItems;

		std::unordered_map<std::string, std::unique_ptr<Texture>>	m_DiffuseMaps;
		std::unordered_map<std::string, std::unique_ptr<Texture>>	m_NormalMaps;
		std::unordered_map<std::string, std::unique_ptr<Material>>	m_Materials;
		std::vector<std::unique_ptr<FrameResources>>				mFrameResources;
		FrameResources* mCurrFrameResource =						nullptr;
		UploadBuffer<UINT>* m_processedInstancesCounterReset =		nullptr;
		std::vector<RenderItem*> m_potentialOccluders;
		std::vector<IndirectCommand> m_indirectCommandData;
		std::vector<IndirectCommand> m_occluderIndirectCommandData;
		Scene								m_Scene;

		HiZBuffer							m_HizBuffer;
		int									mCurrFrameResourceIndex = 0;
		UINT								m_SrvCbvUavDescriptorSize;
		UINT								m_renderItemsSize = 0;
		UINT*								m_drawnInstancesCount;
		UINT								m_totalDrawnInstancesCount;

		D3D12_RECT							m_scissorRect;
		D3D12_RECT							m_cullingScissorRect;
		std::vector<byte>					m_vertexShader;
		std::vector<byte>					m_pixelShader;
		std::vector<byte>					m_computeShader;

		// Variables used with the rendering loop.
		bool	m_loadingComplete;
		
	};
}

