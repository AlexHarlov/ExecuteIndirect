#include "pch.h"
#include "Renderer.h"
#include "Camera.h"
#include "DirectXHelper.h"
#include "d3dx12.h"
#include "DDSTextureLoader.h"
#include <ppltasks.h>
#include <synchapi.h>
#include <array>
#include <sstream>

using namespace ExecuteIndirect; 
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;

static const char* fileNames[] = {  "models\\terrain.obj",
								    "models\\fir.obj",
									"models\\tree.obj",
									"models\\Bison.obj",
									"models\\Tiger.obj",
									"models\\Wolf.obj",
									"models\\Deer.obj",
									"models\\Stone.obj",
									"models\\Grass.obj",
									"models\\house.obj",
									"models\\farmhouse.obj"
								};

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
Renderer::Renderer(const std::shared_ptr<DX::DeviceResources>& deviceResources,
					const std::shared_ptr<Camera>& camera) :
	m_loadingComplete(false),
	m_deviceResources(deviceResources),
	m_Camera(camera),
	m_cullingScissorRect(),
	m_enableCulling(false),
	m_HizBuffer(deviceResources)
{
	//m_Loader.ReadOBJFiles(fileNames, _countof(fileNames), m_renderItems, m_DiffuseMaps, m_NormalMaps, m_Materials);
	m_Loader.ReadBinFiles(m_renderItems, m_DiffuseMaps, m_NormalMaps, m_Materials);
	m_cullingScissorRect.bottom = static_cast<LONG>(deviceResources->GetRenderTargetHeight());
	m_cullingScissorRect.right = static_cast<LONG>(deviceResources->GetRenderTargetWidth());
	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

Renderer::~Renderer()
{
	
	m_commandList->Close();
	m_deviceResources->WaitForGpu();
}

void Renderer::CreateDeviceDependentResources()
{
	CreateRootSignatures();
	BuildPSOAndCommandLists();
	LoadTextures();
	
	CreateCommandSignature();
	BuildFrameResources();
	
	m_Scene.BuildInstanceData(m_renderItems);
	m_Scene.SetOccluders(m_renderItems);
	BuildRenderItems();	
	BuildIndirectCommands();
	BuildDescriptorHeaps();
	InitializeHiZBuffer();
	// Close the command list and execute it to begin the vertex buffer copy into
	// the default heap.
	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	m_deviceResources->WaitForGpu();
	m_loadingComplete = true;
	
}

// Initializes view parameters when the window size changes.
void Renderer::CreateWindowSizeDependentResources()
{
	int width = m_deviceResources->GetRenderTargetWidth();
	int height = m_deviceResources->GetRenderTargetHeight();
	float aspectRatio = width /height;
	float fovAngleY = 70.0f * XM_PI / 180.0f;

	D3D12_VIEWPORT* viewport = m_deviceResources->GetScreenViewport();
	m_scissorRect = { 0, 0, static_cast<LONG>(viewport->Width), static_cast<LONG>(viewport->Height)};

	if (aspectRatio < 1.0f)
	{
		fovAngleY *= 2.0f;
	}

	m_Camera->SetFrustum(fovAngleY, aspectRatio, 1.0f, 2100.f);
	
	m_SceneBufferData.viewportSize.x = viewport->Width;
	m_SceneBufferData.viewportSize.y = viewport->Height;
}


void Renderer::UpdateMaterialBuffer()
{
	auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
	for (std::unordered_map<std::string, std::unique_ptr<Material>>::iterator iter = m_Materials.begin(); iter != m_Materials.end(); iter++)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = iter->second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->data.MatTransform);

			MaterialData matData;
			matData.DiffuseAlbedo = mat->data.DiffuseAlbedo;
			matData.FresnelR0 = mat->data.FresnelR0;
			matData.Roughness = mat->data.Roughness;
			matData.NormalMapIndex = mat->data.NormalMapIndex;
			matData.DiffuseMapIndex = mat->data.DiffuseMapIndex;
			XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
			currMaterialBuffer->CopyData(mat->MatCBIndex, matData);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

/// <summary>
/// Updates the scene constant buffer.
/// </summary>
void Renderer::UpdateSceneCB()
{
	//Load the view and projection matricies
	XMFLOAT4X4 view = m_Camera->GetViewMatrix();
	XMFLOAT4X4 proj = m_Camera->GetProjectionMatrix();
	//Store the matricies in upload buffers
	XMStoreFloat4x4(&m_SceneBufferData.viewMatrix, XMMatrixTranspose(XMLoadFloat4x4(&view)));
	XMStoreFloat4x4(&m_SceneBufferData.projectionMatrix, XMMatrixTranspose(XMLoadFloat4x4(&proj)));
	m_SceneBufferData.ambientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	//direction lights
	m_SceneBufferData.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	m_SceneBufferData.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
	m_SceneBufferData.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	m_SceneBufferData.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
	m_SceneBufferData.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	m_SceneBufferData.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

	
	auto currPassCB = mCurrFrameResource->SceneCB.get();
	currPassCB->CopyData(0, m_SceneBufferData);
}

/// <summary>
/// Updates the scene resources
/// </summary>
void Renderer::Update()
{
	if (m_loadingComplete) {
		// Cycle through the circular frame resource array.
		mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % DX::c_frameCount;
		mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

		UpdateSceneCB();
		UpdateMaterialBuffer();
	}
}

/// <summary>
/// Enables and disables the culling algorithms
/// </summary>
void Renderer::ChangeCulling()
{
	m_enableCulling = !m_enableCulling;
	m_justChangedCulling = true;
}


/// <summary>
/// Renders one frame from the scene.
/// </summary>
/// <returns></returns>
bool Renderer::Render()
{
	// Loading is asynchronous. Only draw geometry after it's loaded.
	if (!m_loadingComplete)
	{
		return false;
	}
		// Record all the commands we need to render the scene into the command lists.
		PopulateCommandLists();
		// Execute the compute work.
		if (m_enableCulling)
		{
 			PIXBeginEvent(m_deviceResources->GetCommandQueue(), 0, L"Cull invisible triangles");

			ID3D12CommandList* ppComputeCommandLists[] = { m_computeCommandList.Get() };
			m_deviceResources->GetComputeCommandQueue()->ExecuteCommandLists(_countof(ppComputeCommandLists), ppComputeCommandLists);

			PIXEndEvent(m_deviceResources->GetCommandQueue());

			m_deviceResources->GetComputeCommandQueue()->Signal(
				m_deviceResources->GetComputeFence(),
				m_deviceResources->GetFenceValue()
				);

			// Execute the rendering work only when the compute work is complete.
			m_deviceResources->GetCommandQueue()->Wait(
				m_deviceResources->GetComputeFence(),
				m_deviceResources->GetFenceValue()
				);
		}
		
		PIXBeginEvent(m_deviceResources->GetCommandQueue(), 0, L"Render");
		// Execute the rendering work.
		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		PIXEndEvent(m_deviceResources->GetCommandQueue());

		return true;
}

/// <summary>
/// Gets the total drawn instances count.
/// </summary>
/// <returns></returns>
UINT Renderer::GetTotalDrawnInstances()
{
	UINT* pMappedCounter = nullptr;
	CD3DX12_RANGE range(0, m_indirectCommandData.size() * sizeof(UINT));
	m_totalDrawnInstancesCount = 0;
	//Map the GPU resource, that contains the UAV counters 
	ThrowIfFailed(DrawnInstancesReadbackBuffer->Map(0, &range, reinterpret_cast<void**>(&pMappedCounter)));
	for (UINT i = 0; i < m_indirectCommandData.size(); i++) {
		m_totalDrawnInstancesCount += pMappedCounter[i];
	}
	//Unmap the resource
	DrawnInstancesReadbackBuffer->Unmap(0, nullptr);
	return m_totalDrawnInstancesCount;
}

/// <summary>
/// Builds the frame resources - for each frame create the upload buffers.
/// </summary>
void Renderer::BuildFrameResources()
{
	auto d3Device = m_deviceResources->GetD3DDevice();
	for (int i = 0; i < DX::c_frameCount; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResources>(d3Device,
			1, (UINT)m_renderItems.size(), (UINT)m_Materials.size()));
	}

}

/// <summary>
/// Populates the graphics and compute command lists.
/// </summary>
void Renderer::PopulateCommandLists()
{
		//Get the current frame index (from 0 to DX::c_frameCount)
		UINT currentFrameIndex = m_deviceResources->GetCurrentFrameIndex();
		//Reset the graphics and compute command allocators
		ThrowIfFailed(m_deviceResources->GetCommandAllocator()->Reset());
		ThrowIfFailed(m_deviceResources->GetComputeCommandAllocator()->Reset());
		//Reset the graphics and compute command lists with their pipeline states
		ThrowIfFailed(m_computeCommandList->Reset(m_deviceResources->GetComputeCommandAllocator(), m_computePipelineState.Get()));
		ThrowIfFailed(m_commandList->Reset(m_deviceResources->GetCommandAllocator(), m_pipelineState.Get()));

		// Record the compute commands that will cull instances and prevent them from being processed by the graphics pipeline.
		if (m_enableCulling)
		{ 
			//Render the occluders in the HiZ buffer and generate mip maps of the rendered occluders
			m_HizBuffer.RenderOccluders( mCurrFrameResource, m_occluderIndirectCommandData.size());
			m_HizBuffer.RenderMIPMap();
			//m_computeCommandList->EndQuery(m_deviceResources->GetTimestampQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, 2);
			//Set the root signature of the compute shader
			m_computeCommandList->SetComputeRootSignature(m_computeRootSignature.Get());
			//Set the currect descriptor heaps with the HiZ SRV descriptor heap
			ID3D12DescriptorHeap* ppHeaps[] = { m_srvHiZMapHeap.Get() };
			m_computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
			//Set the scene constant buffer and the HiZ buffer SRV
			m_computeCommandList->SetComputeRootConstantBufferView(Compute_SceneCBV, mCurrFrameResource->SceneCB->Resource()->GetGPUVirtualAddress());
			m_computeCommandList->SetComputeRootDescriptorTable(Compute_HiZBufferSRV, m_srvHiZMapHeap->GetGPUDescriptorHandleForHeapStart());

			//m_computeCommandList->EndQuery(m_deviceResources->GetTimestampQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, 2);
			ppHeaps[0] = m_InstanceDataHeap.Get();
			//Set the currect descriptor heaps with the instance data heap
			m_computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
			//Iterate over the render items and set necessary compute shader data 
			UINT i = 0;
			std::unordered_map<std::string, std::unique_ptr<RenderItem>>::iterator iter;
			for(iter = m_renderItems.begin(); iter != m_renderItems.end(); iter++, i++){
				//Do not cull the occluders, they are the one's that cull, not to be culled
				if (iter->second->isOccluder())
					continue; 
				//Set the instance data descriptor table using the instance data heap with the necessary offset
				m_computeCommandList->SetComputeRootDescriptorTable(Compute_InstanceDataTable, 
					CD3DX12_GPU_DESCRIPTOR_HANDLE(m_InstanceDataHeap->GetGPUDescriptorHandleForHeapStart(), 2*i, m_SrvCbvUavDescriptorSize));
				//Set the processed instance UAV buffer state to "copy destination"
				D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(iter->second->GetProcessedInstanceBufferGPU().Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
				m_computeCommandList->ResourceBarrier(1, &barrier);
				//Reset the UAV counter by copying zero-ed data to the counter (which is located at the last 4 bytes of the UAV buffer)
				m_computeCommandList->CopyBufferRegion(iter->second->GetProcessedInstanceBufferGPU().Get(), iter->second->GetInstanceBufferCounterOffset(), m_processedInstancesCounterReset->Resource(), 0, sizeof(UINT));
				//Set the processed instance UAV buffer state to unordered access which will allow the compute shader to add the approved instance data
				barrier = CD3DX12_RESOURCE_BARRIER::Transition(iter->second->GetProcessedInstanceBufferGPU().Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				m_computeCommandList->ResourceBarrier(1, &barrier);
				//Set the compute shader constants 
				ComputeShaderConstants csConstants;
				csConstants.instanceCount = iter->second->GetInstanceCount();
				csConstants.boundingBoxCenter = iter->second->GetBoundingBoxData().Center;
				csConstants.boundingBoxExtents = iter->second->GetBoundingBoxData().Extents;
				
				m_computeCommandList->SetComputeRoot32BitConstants(Compute_Constants, 7, reinterpret_cast<void*>(&csConstants), 0);
				//Start the compute shader execution with thread block count equal to instance count divided by thread count per block
				m_computeCommandList->Dispatch(static_cast<UINT>(ceil(iter->second->GetInstanceCount() / float(ComputeThreadBlockSize))), 1, 1);
			}
			//End timestamp query and resolve the collected data
			//m_computeCommandList->EndQuery(m_deviceResources->GetTimestampQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, 3);
			//m_computeCommandList->ResolveQueryData(m_deviceResources->GetTimestampQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, 2, 2, m_deviceResources->GetTimestampResultBuffer(), 2*sizeof(UINT64));
		}
		//Close the compute command list (even if no commands were recorded, i.e. no culling, we can still close empty list)
		ThrowIfFailed(m_computeCommandList->Close());
		
		// Record the rendering commands.
		{
			//Start collecting time information
			m_commandList->EndQuery(m_deviceResources->GetTimestampQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
			//Set the graphics root signature
			m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
			//Set viewport and scissor rectangle (same as the viewport)
			m_commandList->RSSetViewports(1, m_deviceResources->GetScreenViewport());
			m_commandList->RSSetScissorRects(1, &m_scissorRect);
			//Set the currently used descriptor heap with the texture SRV heap
			ID3D12DescriptorHeap* ppHeaps[] = { m_srvTextureHeap.Get() };
			m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
			//Set the constant buffer
			m_commandList->SetGraphicsRootConstantBufferView(Graphics_SceneCBV, mCurrFrameResource->SceneCB->Resource()->GetGPUVirtualAddress());
			//Set the material SRV (as it is structured buffer in the shader we can do it without using heap)
			m_commandList->SetGraphicsRootShaderResourceView(Graphics_MaterialsSRV, mCurrFrameResource->MaterialBuffer->Resource()->GetGPUVirtualAddress());
			//Set the texture descriptor table using the texture SRV descriptor heap 
			m_commandList->SetGraphicsRootDescriptorTable(Graphics_TextureTable, m_srvTextureHeap->GetGPUDescriptorHandleForHeapStart());
			//Calculate the byte offset of the instance count member of the indirect command structure
			IndirectCommand c;
			UINT commandOffset = (char*)&c.drawArguments.InstanceCount - (char*)&c;
			if (m_enableCulling) {
				//If we are culling we want to copy the UAV counter to the instance count member of the indirect command structure
				//So we change the commands buffer to copy destination state
				D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
					CommandBuffer.Get(),
					D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
					D3D12_RESOURCE_STATE_COPY_DEST);
				m_commandList->ResourceBarrier(1, &barrier);
				
				//If this is the first time we are populating command lists, we update the indirect commands by changing the GPU address of the 
				//instances data (if we are culling we are using the processed instance data (UAV))
				if (m_justChangedCulling) {
					m_justChangedCulling = false;
					UpdateIndirectCommands();
				}
				//Iterate over the render items and copy the UAV counter (the accepted instance data) into the command's instance counte field
				UINT i = 0;
				std::unordered_map<std::string, std::unique_ptr<RenderItem>>::iterator iter;
				for (iter = m_renderItems.begin(); iter != m_renderItems.end(); iter++, i++) {
					if (iter->second->isOccluder()) 
						continue;
					//change the processed instance data state to copy source
					barrier = CD3DX12_RESOURCE_BARRIER::Transition(
						iter->second->GetProcessedInstanceBufferGPU().Get(),
						D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
						D3D12_RESOURCE_STATE_COPY_SOURCE);
					m_commandList->ResourceBarrier(1, &barrier);
					//Calculate destination offset
					UINT dstOffset = i*sizeof(IndirectCommand) + commandOffset;
					//Copy the UAV counter in the InstanceCount field of the D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED argument of the command buffer	 
					m_commandList->CopyBufferRegion(CommandBuffer.Get(), dstOffset, iter->second->GetProcessedInstanceBufferGPU().Get(),
						iter->second->GetInstanceBufferCounterOffset(), sizeof(UINT));
					//Change the processed instance data state back to unordered access
					barrier = CD3DX12_RESOURCE_BARRIER::Transition(
						iter->second->GetProcessedInstanceBufferGPU().Get(),
						D3D12_RESOURCE_STATE_COPY_SOURCE,
						D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					m_commandList->ResourceBarrier(1, &barrier);
				}
				//Changed the command buffer state back to indirect arguement
				barrier = CD3DX12_RESOURCE_BARRIER::Transition(
					CommandBuffer.Get(),
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
				m_commandList->ResourceBarrier(1, &barrier);
			}
			else {
				//If this is the first frame rendered after switching off the culling, than we need to update the resource shader address
				//of the instance data buffer
				if (m_justChangedCulling) {
					m_justChangedCulling = false;
					UpdateIndirectCommands();
				}
			}

			D3D12_RESOURCE_BARRIER barriers[2] = { CD3DX12_RESOURCE_BARRIER::Transition(
				m_deviceResources->GetRenderTarget(),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET),
				CD3DX12_RESOURCE_BARRIER::Transition(
					CommandBuffer.Get(),
					D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
					D3D12_RESOURCE_STATE_COPY_SOURCE)

			};
			//Change the render target state from presenting to being a render target
			m_commandList->ResourceBarrier(1, &barriers[0]);
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_deviceResources->GetRenderTargetView();
			CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_deviceResources->GetDepthStencilView();
			//Set the render target's and depth stencil's views
			m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
			const float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
			//Clear the render target and depth stencil
			m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
			m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
			//Set the primitive topology
			m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);	
			//Begin indirect execution of the command buffer
			PIXBeginEvent(m_commandList.Get(), 0, L"Draw all triangles");
			m_commandList->ExecuteIndirect(
				m_commandSignature.Get(),
				m_indirectCommandData.size(),
				CommandBuffer.Get(),
				0,
				nullptr,
				0);
			PIXEndEvent(m_commandList.Get());
			//Change the render target state back to presenting 
			barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			m_commandList->ResourceBarrier(2, barriers);
			//Copy the drawn instances count to our readback buffer to visualize it to the user
			for (UINT i = 0; i < m_indirectCommandData.size(); i++) {
				UINT srcOffset = i*sizeof(IndirectCommand) + commandOffset;
				m_commandList->CopyBufferRegion(DrawnInstancesReadbackBuffer.Get(), i*sizeof(UINT), 
												CommandBuffer.Get(), srcOffset, sizeof(UINT)
					);
			}
			//Change the command buffer state back to indirect argument
			barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
			barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
			m_commandList->ResourceBarrier(1, &barriers[1]);
		}
		//End collecting timestamp data
		m_commandList->EndQuery(m_deviceResources->GetTimestampQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
		m_commandList->ResolveQueryData(m_deviceResources->GetTimestampQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, m_deviceResources->GetTimestampResultBuffer(), 0);
		ThrowIfFailed(m_commandList->Close());
}

void Renderer::InitializeHiZBuffer()
{
	m_HizBuffer.InitMIPMap();
	m_HizBuffer.InitDepth();
}

/// <summary>
/// Builds the indirect commands buffers.
/// </summary>
void Renderer::BuildIndirectCommands() {
	auto d3Device = m_deviceResources->GetD3DDevice();
	UINT riSize = m_renderItems.size();
	//Iterate over the render items
	for(std::unordered_map<std::string, std::unique_ptr<RenderItem>>::iterator iter = m_renderItems.begin(); iter != m_renderItems.end(); iter++) {
		//Every drawing command consists of vertex buffer view, index buffer view, shader resource view
		//and the arguments for the DrawIndexedInstanced call
		IndirectCommand command;
		command.drawArguments.BaseVertexLocation = 0;
		command.drawArguments.IndexCountPerInstance = iter->second->GetIndexCount();
		command.drawArguments.InstanceCount = iter->second->GetInstanceCount();
		command.drawArguments.StartIndexLocation = 0;
		command.drawArguments.StartInstanceLocation = 0;

		command.vertexBufferView = iter->second->VertexBufferView();
		command.indexBufferView = iter->second->IndexBufferView();
		command.instancesShaderView = iter->second->GetInstanceBufferGPU()->GetGPUVirtualAddress();

		//push back the command in the vector
		m_indirectCommandData.push_back(command);
		//if the item is occluder, than push it in the occluder's commands also
		if (iter->second->isOccluder())
			m_occluderIndirectCommandData.push_back(command);
	}
	//Create GPU resource - command buffer
	DX::CreateDefaultBuffer(d3Device, m_commandList.Get(), m_indirectCommandData.data(),
		m_indirectCommandData.size()*sizeof(IndirectCommand), CommandBufferUploader, CommandBuffer);

	//Create the GPU resource - occluder's command and uploader buffer
	ComPtr<ID3D12Resource>& OccluderCommandBuffer = m_HizBuffer.GetOccluderCommandBuffer();
	ComPtr<ID3D12Resource>& OccluderCommandBufferUploader = m_HizBuffer.GetOccluderCommandBufferUploader();

	DX::CreateDefaultBuffer(d3Device, m_commandList.Get(), m_occluderIndirectCommandData.data(), m_occluderIndirectCommandData.size()*sizeof(IndirectCommand),
		OccluderCommandBuffer, OccluderCommandBufferUploader);

	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CommandBuffer.Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));
}

/// <summary>
/// Updates the indirect commands. Every time we switch occlusion, we need to change the address of the 
/// Shader Resource View
/// </summary>
void Renderer::UpdateIndirectCommands()
{
	UINT indirectCommandDataInx = 0;
	for (std::unordered_map<std::string, std::unique_ptr<RenderItem>>::iterator iter = m_renderItems.begin(); iter != m_renderItems.end(); iter++, indirectCommandDataInx++) {
		//skip the item if it is an occluder
		if (iter->second->isOccluder())
			continue;
		//set the needed address of the SRV
		if (m_enableCulling) {
			m_indirectCommandData[indirectCommandDataInx].instancesShaderView = iter->second->GetProcessedInstanceBufferGPU()->GetGPUVirtualAddress();
		}
		else {
			m_indirectCommandData[indirectCommandDataInx].instancesShaderView = iter->second->GetInstanceBufferGPU()->GetGPUVirtualAddress();
		}
	}
	//update the command buffer on the GPU 
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = m_indirectCommandData.data();
	subResourceData.RowPitch = m_renderItemsSize * sizeof(IndirectCommand);
	subResourceData.SlicePitch = subResourceData.RowPitch;
	UpdateSubresources<1>(m_commandList.Get(), CommandBuffer.Get(), CommandBufferUploader.Get(), 0, 0, 1, &subResourceData);
}

/// <summary>
/// Loads the diffuse and normal maps.
/// </summary>
void Renderer::LoadTextures() {
	auto d3Device = m_deviceResources->GetD3DDevice();
	std::string texturePath;
	for (auto& texture : m_DiffuseMaps) {
		texturePath = texture.second->Filename;
		ThrowIfFailed(CreateDDSTextureFromFile12(d3Device, m_commandList.Get(), std::wstring(texturePath.begin(), texturePath.end()).c_str(),
													texture.second->Resource, texture.second->UploadHeap));
		//Change the state to add the non pixel shader resource state
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture.second->Resource.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
		
	}
	for (auto& normalMap : m_NormalMaps) {
		texturePath = normalMap.second->Filename;
		ThrowIfFailed(CreateDDSTextureFromFile12(d3Device, m_commandList.Get(), std::wstring(texturePath.begin(), texturePath.end()).c_str(),
			normalMap.second->Resource, normalMap.second->UploadHeap));
		//Change the state to add the non pixel shader resource state
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(normalMap.second->Resource.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	}
	
}


/// <summary>
/// Builds the descriptor heaps.
/// </summary>
void Renderer::BuildDescriptorHeaps() {
	auto d3dDevice = m_deviceResources->GetD3DDevice();

	{
		//Describe the descriptor heap for the diffuse and normal maps
		D3D12_DESCRIPTOR_HEAP_DESC SrvHeapDesc = {};
		SrvHeapDesc.NumDescriptors = m_DiffuseMaps.size() + m_NormalMaps.size();		
		SrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		SrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		//Create the descriptor heap
		ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&SrvHeapDesc, IID_PPV_ARGS(&m_srvTextureHeap)));
		m_srvTextureHeap->SetName(L"Texture SRV Heap");
		
		//Describe the descriptor heap for the instance data (both processed and non-processed)
		SrvHeapDesc.NumDescriptors = 2 * m_renderItems.size();
		SrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		SrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		//Create the descriptor heap
		ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&SrvHeapDesc, IID_PPV_ARGS(&m_InstanceDataHeap)));
		m_InstanceDataHeap->SetName(L"Instance Data Heap");

		//Describe the descriptor heap for the HiZ buffer - only one descriptor needed
		//It needs to be in a heap, because we can use root descriptors only for structured and raw buffers
		SrvHeapDesc.NumDescriptors = 1;
		SrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		SrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		//Create the descriptor heap
		ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&SrvHeapDesc, IID_PPV_ARGS(&m_srvHiZMapHeap)));
		m_srvHiZMapHeap->SetName(L"HiZ Buffer Heap");
	
		m_SrvCbvUavDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	//fill texture heap
	{
		//Describe the diffuse and normal SRV
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		//For each diffuse map create SRV at the needed offset
		for (auto& texture : m_DiffuseMaps)
		{
			//place in the descriptor heap
			CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_srvTextureHeap->GetCPUDescriptorHandleForHeapStart(), texture.second->index, m_SrvCbvUavDescriptorSize);
			//Set the needed format and mip levels of the texture
			srvDesc.Format = texture.second->Resource->GetDesc().Format;
			srvDesc.Texture2D.MipLevels = texture.second->Resource->GetDesc().MipLevels;
			//Create the shader resource view
			d3dDevice->CreateShaderResourceView(texture.second->Resource.Get(), &srvDesc, hDescriptor);
		}
		//For each normal map create SRV at the needed offset
		int offset = m_DiffuseMaps.size();
		for (auto& normalMap : m_NormalMaps)
		{
			//place in the descriptor heap
			CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_srvTextureHeap->GetCPUDescriptorHandleForHeapStart(), normalMap.second->index + offset, m_SrvCbvUavDescriptorSize);
			//Set the needed format and mip levels of the texture
			srvDesc.Format = normalMap.second->Resource->GetDesc().Format;
			srvDesc.Texture2D.MipLevels = normalMap.second->Resource->GetDesc().MipLevels;
			//Create the shader resource view
			d3dDevice->CreateShaderResourceView(normalMap.second->Resource.Get(), &srvDesc, hDescriptor);
		}
	}
	//Create the UAV counter reset 
	{
		m_processedInstancesCounterReset = new UploadBuffer<UINT>(d3dDevice, 1, false);
		m_processedInstancesCounterReset->CopyData(0, 0);
	}
	//Create a readback buffer to store the UAV counter for each render item
	{
		DX::CreateReadbackBuffer(d3dDevice, m_commandList.Get(), m_indirectCommandData.size()*sizeof(UINT), DrawnInstancesReadbackBuffer);
	}
	
	{
		//Describe the SRV for the non-processed instance data
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;		
		srvDesc.Buffer.StructureByteStride = sizeof(InstanceData);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		//Describe the UAV for the processed instance data
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.StructureByteStride = sizeof(InstanceData);
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		//Create a handle to help us create descriptors in the heap
		CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_InstanceDataHeap->GetCPUDescriptorHandleForHeapStart());
		std::unordered_map<std::string, std::unique_ptr<RenderItem>>::iterator iter;
		//Iterate over the render itms
		for (iter = m_renderItems.begin(); iter != m_renderItems.end(); iter++) {
			//Set the number of elements - instance count
			srvDesc.Buffer.NumElements = iter->second->GetInstanceCount();
			//Create the shader resource view
			d3dDevice->CreateShaderResourceView(iter->second->GetInstanceBufferGPU().Get(), &srvDesc, hDescriptor);
			//Offset the descriptor in the descriptor table
			hDescriptor.Offset(1, m_SrvCbvUavDescriptorSize);

			//Set the number of elements and the UAV counter offset in bytes
			uavDesc.Buffer.NumElements = iter->second->GetInstanceCount();
			uavDesc.Buffer.CounterOffsetInBytes = iter->second->GetInstanceBufferCounterOffset();
			//Create the unordered resource view
			d3dDevice->CreateUnorderedAccessView(iter->second->GetProcessedInstanceBufferGPU().Get(), iter->second->GetProcessedInstanceBufferGPU().Get(), &uavDesc, hDescriptor);
			//Offset the descriptor in the descriptor table
			hDescriptor.Offset(1, m_SrvCbvUavDescriptorSize);
		}
	}
	//Create SRV for the HiZ Buffer
	{		
		ComPtr<ID3D12Resource>& hizBuffer = m_HizBuffer.GetHiZBuffer();
		//Use the whole mip-map chain description 
		D3D12_SHADER_RESOURCE_VIEW_DESC *mipMapDesc = m_HizBuffer.GetMIPMapChainDesc();
		d3dDevice->CreateShaderResourceView(hizBuffer.Get(), mipMapDesc, m_srvHiZMapHeap->GetCPUDescriptorHandleForHeapStart());
	}
}


/// <summary>
/// Builds the vertex, index and instance buffers,
/// also creates the bounding box of the object
/// </summary>
void Renderer::BuildRenderItems()
{
	auto d3Device = m_deviceResources->GetD3DDevice();
	std::string name;
	//Iterate over the render items
	std::unordered_map<std::string, std::unique_ptr<RenderItem>>::iterator iter;
	for (iter = m_renderItems.begin(); iter != m_renderItems.end(); iter++) {
		//Create GPU vertex buffer
		DX::CreateDefaultBuffer(d3Device, m_commandList.Get(),
			iter->second->GetVertexBufferData(),
			iter->second->GetVertexCount() * sizeof(Vertex),
			iter->second->GetVertexBufferUploader(),
			iter->second->GetVertexBufferGPU());
		name = "Vertex Buffer for render item " + iter->first;
		iter->second->GetVertexBufferGPU()->SetName(DX::convertCharArrayToLPCWSTR(name.c_str()).c_str());
		//Create GPU index buffer
		DX::CreateDefaultBuffer(d3Device, m_commandList.Get(),
			iter->second->GetIndexBufferData(),
			iter->second->GetIndexCount() * sizeof(UINT),
			iter->second->GetIndexBufferUploader(),
			iter->second->GetIndexBufferGPU());
		name = "Index Buffer for render item " + iter->first;
		iter->second->GetIndexBufferGPU()->SetName(DX::convertCharArrayToLPCWSTR(name.c_str()).c_str());
		//Create GPU instance data buffer
		DX::CreateDefaultBuffer(d3Device, m_commandList.Get(),
			iter->second->GetInstances().data(),
			iter->second->GetInstancesByteSize(),
			iter->second->GetInstanceBufferUploader(),
			iter->second->GetInstanceBufferGPU());
		name = "Instance Buffer for render item " + iter->first;
		iter->second->GetInstanceBufferGPU()->SetName(DX::convertCharArrayToLPCWSTR(name.c_str()).c_str());
		//Create UAV counter offset
		iter->second->CreateCounterOffset();
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(iter->second->GetInstanceBufferGPU().Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
		//Create GPU UAV buffer for storing the processed instance data
		DX::CreateUAVBuffer(d3Device, m_commandList.Get(),
			iter->second->GetInstanceBufferCounterOffset() + sizeof(UINT),
			iter->second->GetProcessedInstanceBufferGPU());
		name = "Processed Instance Data Buffer for render item " + iter->first;
		iter->second->GetProcessedInstanceBufferGPU()->SetName(DX::convertCharArrayToLPCWSTR(name.c_str()).c_str());
		//Create the item's bounding box
		iter->second->CreateBoundingBox();
	}

}

/// <summary>
/// Builds the Pipeline State Objects and command lists.
/// </summary>
void Renderer::BuildPSOAndCommandLists() {
	auto d3dDevice = m_deviceResources->GetD3DDevice();
	//Read the compiled shaders 
	ThrowIfFailed(DX::ReadDataFromFile(DX::GetAssetFullPath(L"VertexShader.cso").c_str(), m_vertexShader));
	ThrowIfFailed(DX::ReadDataFromFile(DX::GetAssetFullPath(L"PixelShader.cso").c_str(), m_pixelShader));
	ThrowIfFailed(DX::ReadDataFromFile(DX::GetAssetFullPath(L"ComputeShader.cso").c_str(), m_computeShader));

	// Create the pipeline state once the shaders are loaded.
	{
		//Describe the shader input layout
		static const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		//Describe default rasterizer with solid mode and no back-face cull
		CD3DX12_RASTERIZER_DESC rsDesc(D3D12_DEFAULT);
		rsDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rsDesc.CullMode = D3D12_CULL_MODE_NONE;
		//Describe the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
		state.InputLayout = { inputLayout, _countof(inputLayout) };
		state.pRootSignature = m_rootSignature.Get();
		state.VS = { &m_vertexShader[0], m_vertexShader.size() };
		state.PS = { &m_pixelShader[0], m_pixelShader.size() };
		state.RasterizerState = rsDesc;
		state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		state.SampleMask = UINT_MAX;
		state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		state.NumRenderTargets = 1;
		state.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
		state.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		state.SampleDesc.Count = 1;
		//Create the graphics pipeline state object (PSO).
		ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&m_pipelineState)));
		m_pipelineState->SetName(L"Pipeline State");

		// Shader data can be deleted once the pipeline state is created.
		m_vertexShader.clear();
		m_pixelShader.clear();

		// Describe and create the compute pipeline state object (PSO).
		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = m_computeRootSignature.Get();
		computePsoDesc.CS = { &m_computeShader[0], m_computeShader.size() };

		ThrowIfFailed(d3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&m_computePipelineState)));
		m_computePipelineState->SetName(L"Compute Pipeline State");
		m_computeShader.clear();
	}
	// Create the graphic and compute command list.
	ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_deviceResources->GetCommandAllocator(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
	ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_deviceResources->GetComputeCommandAllocator(), m_computePipelineState.Get(), IID_PPV_ARGS(&m_computeCommandList)));
	m_commandList->SetName(L"Command List");
	m_computeCommandList->SetName(L"Compute Command List");
	//Close the compute command list, as the rest of the initialization will be handled by the graphics command list
	ThrowIfFailed(m_computeCommandList->Close());
}

/// <summary>
/// Creates the root signatures.
/// </summary>
void Renderer::CreateRootSignatures()
{
	auto d3dDevice = m_deviceResources->GetD3DDevice();
	//Check highest supported feature data version
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(d3dDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}
	//Describe root parameters 
	CD3DX12_ROOT_PARAMETER1 rootParameters[Graphics_RootParametersCount];
	CD3DX12_DESCRIPTOR_RANGE1 textureTable[2];
	textureTable[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, m_DiffuseMaps.size(), 0, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	textureTable[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, m_NormalMaps.size(), 0, 2, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	//Root parameters can be root descriptors, root constants or root descriptor tables
	rootParameters[Graphics_SceneCBV].InitAsConstantBufferView(0);
	rootParameters[Graphics_MaterialsSRV].InitAsShaderResourceView(0);
	rootParameters[Graphics_InstanceData].InitAsShaderResourceView(1);
	rootParameters[Graphics_TextureTable].InitAsDescriptorTable(2, textureTable);

	// A root signature is an array of root parameters.
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(Graphics_RootParametersCount, rootParameters, 6, GetStaticSamplers().data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	//Create the root signature
	ComPtr<ID3DBlob> signature;  
	ComPtr<ID3DBlob> error;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
	ThrowIfFailed(d3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	m_rootSignature->SetName(L"Root Signature");

	//describe the compute root signature
	CD3DX12_DESCRIPTOR_RANGE1 instanceData[2];
	instanceData[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	instanceData[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

	CD3DX12_DESCRIPTOR_RANGE1 HizBufferTable[1];
	HizBufferTable[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

	CD3DX12_ROOT_PARAMETER1 computeRootParameters[Compute_RootParametersCount];
	//Initialize Root parameters
	computeRootParameters[Compute_SceneCBV].InitAsConstantBufferView(0);
	computeRootParameters[Compute_HiZBufferSRV].InitAsDescriptorTable(1, HizBufferTable);
	computeRootParameters[Compute_InstanceDataTable].InitAsDescriptorTable(2, instanceData);
	computeRootParameters[Compute_Constants].InitAsConstants(7, 1);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
	computeRootSignatureDesc.Init_1_1(Compute_RootParametersCount, computeRootParameters, 1, m_HizBuffer.GetSamplerDesc());
	//Create the compute root signature
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&computeRootSignatureDesc, featureData.HighestVersion, &signature, &error));
	ThrowIfFailed(d3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_computeRootSignature)));
	m_computeRootSignature->SetName(L"Compute Root Signature");
}

/// <summary>
/// Creates the drawing command signature.
/// </summary>
void Renderer::CreateCommandSignature()
{
	auto d3dDevice = m_deviceResources->GetD3DDevice();
	//Describe the indirect arguments
	D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[4] = {};
	argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
	argumentDescs[0].VertexBuffer.Slot = 0;
	argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
	argumentDescs[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW;
	argumentDescs[2].ShaderResourceView.RootParameterIndex = Graphics_InstanceData;
	argumentDescs[3].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	//Describe the command signature
	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
	commandSignatureDesc.ByteStride = sizeof(IndirectCommand);
	//Create the command signature
	ThrowIfFailed(d3dDevice->CreateCommandSignature(&commandSignatureDesc, m_rootSignature.Get(), IID_PPV_ARGS(&m_commandSignature)));
	m_commandSignature->SetName(L"Command Signature");
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Renderer::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return{
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}