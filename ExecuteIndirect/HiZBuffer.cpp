#include "pch.h"
#include "HiZBuffer.h"
#include "DirectXHelper.h"
#include "ShaderStructures.h"
#include "FrameResources.h"
#include "RenderItem.h"

using namespace ExecuteIndirect;
using namespace DirectX;
using namespace Microsoft::WRL;

const float CBlack[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
const float CWhite[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
const float CRed[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
const float CBlue[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
const float CYellow[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
const float CGray[4] = { 0.7f, 0.7f, 0.7f, 0.7f };

HiZBuffer::HiZBuffer(const std::shared_ptr<DX::DeviceResources>& devResources) :
	m_deviceResources(devResources), m_width(1024), m_height(768), m_fenceValue(0)
{
	CreateResources();
}

void HiZBuffer::RenderOccluders(FrameResources* currFrameResource, UINT occluderCount)
{
	//describe a viewport with the HiZ buffer's width and height
	D3D12_VIEWPORT vp;
	vp.Width = (FLOAT)m_width;
	vp.Height = (FLOAT)m_height;
	vp.MinDepth = 0;
	vp.MaxDepth = 1;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	//describe scissor rectangle (same dimensions as the viewport)
	D3D12_RECT scissorRect = { 0,0,m_width,m_height };
	//Create a barrier to change the HiZ buffer state to render target (we will render the occluders in it)
	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pHizBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE , D3D12_RESOURCE_STATE_RENDER_TARGET );
	//Reset command allocator and command list
	m_HiZDepthCommandAllocator->Reset();
	ThrowIfFailed(m_HiZDepthCommandList->Reset(m_HiZDepthCommandAllocator.Get(), m_HiZDepthPSO.Get()));
	
	//m_HiZDepthCommandList->EndQuery(m_deviceResources->GetTimestampQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
	//Set the graphics root signature
	m_HiZDepthCommandList->SetGraphicsRootSignature(m_HiZDepthRootSignature.Get());
	//Set the scene constant buffer view - we need it for the view and projection matrices, as we render the occluders
	//from the way we see them in the scene
	m_HiZDepthCommandList->SetGraphicsRootConstantBufferView(0, currFrameResource->SceneCB->Resource()->GetGPUVirtualAddress());
	//Change HiZ buffer state
	m_HiZDepthCommandList->ResourceBarrier(1, &barrier);
	//Set the RTV and DSV, using the previously created heaps
	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = m_MIPMapRTVHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = m_HiZDepthDSVHeap->GetCPUDescriptorHandleForHeapStart();
	m_HiZDepthCommandList->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);
	//Clear the render target and depth stencils
	m_HiZDepthCommandList->ClearRenderTargetView(renderTargetView, CWhite, 0, nullptr);
	m_HiZDepthCommandList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0, 0, 0, nullptr);
	//Set viewport and scissor rectangle
	m_HiZDepthCommandList->RSSetViewports(1, &vp);
	m_HiZDepthCommandList->RSSetScissorRects(1, &scissorRect);
	//Set the primitive topology
	m_HiZDepthCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//Execute indirectly the commands which will render the occluder
	PIXBeginEvent(m_HiZDepthCommandList.Get(), 0, L"Draw potential occluders");
	{
		m_HiZDepthCommandList->ExecuteIndirect(m_HiZDepthCommandSignature.Get(), occluderCount, OccluderCommandBuffer.Get(), 0, nullptr, 0);
	}
	PIXEndEvent(m_HiZDepthCommandList.Get());

	//Describe copy source and destination
	//Source will be the depth buffer, filled from rendering the occluders 
	//Destination will be the HiZ Buffer
	D3D12_TEXTURE_COPY_LOCATION source;
	source.pResource = m_pHizDepthBuffer.Get();
	source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	source.SubresourceIndex = 0;
	D3D12_TEXTURE_COPY_LOCATION dest;
	dest.pResource = m_pHizBuffer.Get();
	dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dest.SubresourceIndex = 0;
	//Barriers to change the state of both the source and destination
	D3D12_RESOURCE_BARRIER barriers[2] = {
		CD3DX12_RESOURCE_BARRIER::Transition(m_pHizBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST),
		CD3DX12_RESOURCE_BARRIER::Transition(m_pHizDepthBuffer.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_SOURCE)
	};
	//Execute barriers and change state - now HiZ buffer is copy destination and Depth buffer is copy source
	m_HiZDepthCommandList->ResourceBarrier(2, barriers);
	//Copy the Depth buffer in the HiZ buffer
	m_HiZDepthCommandList->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);
	//Change barriers before and after states
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	//Execute barriers and change state - now HiZ buffer is render target and Depth buffer is ..well depth buffer
	m_HiZDepthCommandList->ResourceBarrier(2, barriers);

	m_HiZDepthCommandList->EndQuery(m_deviceResources->GetTimestampQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
	m_HiZDepthCommandList->ResolveQueryData(m_deviceResources->GetTimestampQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, m_deviceResources->GetTimestampResultBuffer(), 0);
	ThrowIfFailed(m_HiZDepthCommandList->Close());
	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_HiZDepthCommandList.Get() };
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	ThrowIfFailed(m_deviceResources->GetCommandQueue()->Signal(m_fence.Get(), m_fenceValue));
	m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
	WaitForSingleObject(m_fenceEvent, INFINITE);
	
	m_fenceValue++;
	//OutputDebugStringW(L"Occluder drawing time: ");
	//m_deviceResources->ReadTimestamps(false);
}

void HiZBuffer::RenderMIPMap()
{
	//describe a viewport with the HiZ buffer's width and height
	D3D12_VIEWPORT vp;
	vp.Width = (FLOAT)m_width;
	vp.Height = (FLOAT)m_height;
	vp.MinDepth = 0;
	vp.MaxDepth = 1;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	//describe scissor rectangle (same dimensions as the viewport)
	D3D12_RECT scissorRect = { 0, 0, m_width, m_height };
	//Describe copy locations - we will copy from n-1 mip-map level to the n mip-map level
	//using the temporal SRV
	D3D12_TEXTURE_COPY_LOCATION source;
	source.pResource = m_pHizBuffer.Get();
	source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	//source.SubresourceIndex = mipLevel - 1;
	D3D12_TEXTURE_COPY_LOCATION dest;
	dest.pResource = m_pTempBuffer.Get();
	dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	//dest.SubresourceIndex = mipLevel - 1;

	m_HiZMIPMapCommandAllocator->Reset();
	ThrowIfFailed(m_HiZMIPMapCommandList->Reset(m_HiZMIPMapCommandAllocator.Get(), m_HiZMIPMapPSO.Get()));
	m_HiZMIPMapCommandList->EndQuery(m_deviceResources->GetTimestampQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
	PIXBeginEvent(m_HiZMIPMapCommandList.Get(), 0, L"Draw MIP Maps");
	{
		m_HiZMIPMapCommandList->RSSetViewports(1, &vp);
		m_HiZMIPMapCommandList->RSSetScissorRects(1, &scissorRect);
		D3D12_RESOURCE_BARRIER barriers[2] = {
			CD3DX12_RESOURCE_BARRIER::Transition(m_pHizBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(m_pTempBuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST)
		};

		m_HiZMIPMapCommandList->SetGraphicsRootSignature(m_HiZMIPMapRootSignature.Get());
		ID3D12DescriptorHeap* ppHeaps[] = { m_MIPMapSRVHeap.Get() };
		m_HiZMIPMapCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		m_HiZMIPMapCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		m_HiZMIPMapCommandList->IASetVertexBuffers(0, 1, &m_MIPMapVertexBufferView);

		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = m_HiZDepthDSVHeap->GetCPUDescriptorHandleForHeapStart();
		for (UINT mipLevel = 1; mipLevel < m_mipLevels; mipLevel++) {
			m_HiZMIPMapCommandList->ResourceBarrier(2, barriers);
			source.SubresourceIndex = mipLevel - 1;
			dest.SubresourceIndex = mipLevel - 1;
			m_HiZMIPMapCommandList->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);
			//commandList->CopyResource(m_pTempBuffer.Get(), m_pHizBuffer.Get());
			barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
			barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			m_HiZMIPMapCommandList->ResourceBarrier(2, barriers);
			CD3DX12_CPU_DESCRIPTOR_HANDLE renderTargetView(m_MIPMapRTVHeap->GetCPUDescriptorHandleForHeapStart(), mipLevel, m_MIPMapRTVDescriptorSize);
			CD3DX12_GPU_DESCRIPTOR_HANDLE shaderResourceGPUHandle(m_MIPMapSRVHeap->GetGPUDescriptorHandleForHeapStart(), mipLevel - 1, m_MIPMapSRVDescriptorSize);
			m_HiZMIPMapCommandList->OMSetRenderTargets(1, &renderTargetView, false, NULL);
			m_HiZMIPMapCommandList->SetGraphicsRootDescriptorTable(0, shaderResourceGPUHandle);
			m_HiZMIPMapCommandList->DrawInstanced(4, 1, 0, 0);
			//return states to copy destination/source
			barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
			barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		}
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		m_HiZMIPMapCommandList->ResourceBarrier(1, &barriers[0]);
	}
	PIXEndEvent(m_HiZMIPMapCommandList.Get());

	//m_HiZMIPMapCommandList->EndQuery(m_deviceResources->GetTimestampQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
	//m_HiZMIPMapCommandList->ResolveQueryData(m_deviceResources->GetTimestampQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, m_deviceResources->GetTimestampResultBuffer(), 0);

	ThrowIfFailed(m_HiZMIPMapCommandList->Close());
	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_HiZMIPMapCommandList.Get() };
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	ThrowIfFailed(m_deviceResources->GetCommandQueue()->Signal(m_fence.Get(), m_fenceValue));
	m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
	WaitForSingleObject(m_fenceEvent, INFINITE);
	m_fenceValue++;

	//OutputDebugStringW(L"MipMap generation time: ");
	//m_deviceResources->ReadTimestamps(false);
}

/// <summary>
/// Initializes the resources, needed for the rendering of the occluders.
/// </summary>
void HiZBuffer::InitDepth() {
	
	auto d3dDevice = m_deviceResources->GetD3DDevice();
	//Describe the input layout (the structure of the vertices feeded to the vertex shader)
	const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	//Read the compiled shaders
	ThrowIfFailed(DX::ReadDataFromFile(DX::GetAssetFullPath(L"HiZDepthVS.cso").c_str(), m_HiZDepthVS));
	ThrowIfFailed(DX::ReadDataFromFile(DX::GetAssetFullPath(L"HiZDepthPS.cso").c_str(), m_HiZDepthPS));
	//Describe root parameters - the occluder's rendering require the scene constant buffer and the instance's data SRV
	CD3DX12_ROOT_PARAMETER rootParameters[2];
	rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParameters[1].InitAsShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	//Create the root signature description
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	//Create the root signature using its description
	ComPtr<ID3DBlob> signature;  
	ComPtr<ID3DBlob> error;
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
	ThrowIfFailed(d3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_HiZDepthRootSignature)));
	m_HiZDepthRootSignature->SetName(L"HiZ Depth Root Signature");

	//We will be using indirect commands to draw the occluders, so we must define command signature also
	//First describe the commands arguments
	D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[4] = {};
	//Each command will consist of the following function calls
	//IASetVertexBufferView, IASetIndexBufferView, SetShaderResourceView, DrawIndexedInstanced
	argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
	argumentDescs[0].VertexBuffer.Slot = 0;
	argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
	argumentDescs[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW;
	argumentDescs[2].ShaderResourceView.RootParameterIndex = 1;
	argumentDescs[3].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	//Create command signature description, using its argument descriptions
	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
	commandSignatureDesc.ByteStride = sizeof(IndirectCommand);
	//Create the command signature
	ThrowIfFailed(d3dDevice->CreateCommandSignature(&commandSignatureDesc, m_HiZDepthRootSignature.Get(), IID_PPV_ARGS(&m_HiZDepthCommandSignature)));
	// Describe a rasterizer state with no back-face culling and solid rendering mode (opposed to wireframe)
	CD3DX12_RASTERIZER_DESC rsDesc(D3D12_DEFAULT);
	rsDesc.CullMode = D3D12_CULL_MODE_NONE;
	rsDesc.FillMode = D3D12_FILL_MODE_SOLID;
	// Describe the occluder rendering pipeline state
	D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
	state.InputLayout = { inputLayout, _countof(inputLayout) };
	state.pRootSignature = m_HiZDepthRootSignature.Get();
	state.VS = { &m_HiZDepthVS[0], m_HiZDepthVS.size() };
	state.PS = { &m_HiZDepthPS[0], m_HiZDepthPS.size() };
	state.RasterizerState = rsDesc;
	state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	state.SampleMask = UINT_MAX;
	state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	state.NumRenderTargets = 1;
	state.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
	state.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	state.SampleDesc.Count = 1;
	//Create the occluder rendering pipeline state and command list
	ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&m_HiZDepthPSO)));
	m_HiZDepthPSO->SetName(L"HiZ Depth Pipeline State");
	ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_HiZDepthCommandAllocator.Get(), m_HiZDepthPSO.Get(), IID_PPV_ARGS(&m_HiZDepthCommandList)));
	// Close and execute the command list 
	ThrowIfFailed(m_HiZDepthCommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_HiZDepthCommandList.Get() };
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	//Synchronize CPU execution - wait for the command list to finish executing it's commands
	//First signal the command queue with the current fence value
	ThrowIfFailed(m_deviceResources->GetCommandQueue()->Signal(m_fence.Get(), m_fenceValue));
	//Set event to be fired when the command queue passes the fence value
	m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
	//Wait for the event to be fired
	WaitForSingleObject(m_fenceEvent, INFINITE);
	//Increase fence value to be used in next sync
	m_fenceValue++;
}

/// <summary>
/// Initializes the HiZ Buffer mip map chain.
/// </summary>
void HiZBuffer::InitMIPMap() {
	auto d3dDevice = m_deviceResources->GetD3DDevice();
	//Describe the input layout (the structure of the vertices feeded to the vertex shader)
	const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	//Read the compiled shaders
	ThrowIfFailed(DX::ReadDataFromFile(DX::GetAssetFullPath(L"HiZMIPMapVS.cso").c_str(), m_HiZMIPMapVS));
	ThrowIfFailed(DX::ReadDataFromFile(DX::GetAssetFullPath(L"HiZMIPMapPS.cso").c_str(), m_HiZMIPMapPS));

	//Root signature consists of only one two-dimentional texture, which is the last mip map level
	//As it is texture, and not a structured buffer we must use descriptor table and not a root descriptor
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); 

	// Initialize the first and only parameter as descriptor table
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_ALL);

	//Describe root signature - only one parameter which is described by CD3DX12_ROOT_PARAMETER array
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, NULL,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// Create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()));
	ThrowIfFailed(d3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&m_HiZMIPMapRootSignature)));
	
	// Describe a rasterizer state with no back-face culling and solid rendering mode (opposed to wireframe)
	CD3DX12_RASTERIZER_DESC rsDesc(D3D12_DEFAULT);
	rsDesc.CullMode = D3D12_CULL_MODE_NONE;
	rsDesc.FillMode = D3D12_FILL_MODE_SOLID;
	// Describe an empty depth stencil, where we turn off depth testing for the mip map generation itsels
	// Note that we need to use depth testing when rendering the occluders
	D3D12_DEPTH_STENCIL_DESC emptyDepthStencil = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	emptyDepthStencil.DepthEnable = false;
	// Describe the mip map generation pipeline state
	D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
	state.InputLayout = { inputLayout, _countof(inputLayout) };
	state.pRootSignature = m_HiZMIPMapRootSignature.Get();
	state.VS = { &m_HiZMIPMapVS[0], m_HiZMIPMapVS.size() };
	state.PS = { &m_HiZMIPMapPS[0], m_HiZMIPMapPS.size() };
	state.RasterizerState = rsDesc;
	state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	state.DepthStencilState = emptyDepthStencil;
	state.SampleMask = UINT_MAX;
	state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	state.NumRenderTargets = 1;
	state.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
	state.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	state.SampleDesc.Count = 1;
	//Create the mip map generation pipeline state and command list
	ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&m_HiZMIPMapPSO)));
	m_HiZMIPMapPSO->SetName(L"HiZ MIP Map Pipeline State");
	ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_HiZMIPMapCommandAllocator.Get(), m_HiZMIPMapPSO.Get(), IID_PPV_ARGS(&m_HiZMIPMapCommandList)));

	//Only four vertices needed, given directly in normalized device coordinates and covering the whole render target
	const int VerticiesCount = 4;
	//DirectX uses NDC from -1 (leftmost) to 1 (rightmost)
	XMFLOAT3 vertices[] =
	{
		{ XMFLOAT3(-1.0f,  1.0f, 0.0f) },
		{ XMFLOAT3(1.0f,  1.0f, 0.0f)},
		{ XMFLOAT3(-1.0f, -1.0f, 0.0f)},
		{ XMFLOAT3(1.0f, -1.0f, 0.0f)},
	};
	//Create the mip map generation vertex buffer
	DX::CreateDefaultBuffer(d3dDevice, m_HiZMIPMapCommandList.Get(), vertices, VerticiesCount*sizeof(XMFLOAT3), m_MIPMapVertexBufferUpload, m_pMIPMapVertexBuffer);
	//Set debug names
	m_pMIPMapVertexBuffer->SetName(L"MIP Map Vertex Buffer Resource");
	m_MIPMapVertexBufferUpload->SetName(L"MIP Map Vertex Buffer Upload Resource");
	// Describe the vertex buffer view
	m_MIPMapVertexBufferView.BufferLocation = m_pMIPMapVertexBuffer->GetGPUVirtualAddress();
	m_MIPMapVertexBufferView.StrideInBytes = sizeof(XMFLOAT3);
	m_MIPMapVertexBufferView.SizeInBytes = VerticiesCount*sizeof(XMFLOAT3);
	// Close and execute the command list 
	ThrowIfFailed(m_HiZMIPMapCommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_HiZMIPMapCommandList.Get() };
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	//Synchronize CPU execution - wait for the command list to finish executing it's commands
	// First signal the command queue with the current fence value
	ThrowIfFailed(m_deviceResources->GetCommandQueue()->Signal(m_fence.Get(), m_fenceValue));
	//Set event to be fired when the command queue passes the fence value
	m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
	//Wait for the event to be fired
	WaitForSingleObject(m_fenceEvent, INFINITE);
	//Increase fence value to be used in next sync
	m_fenceValue++;
}

/// <summary>
/// Creates all the needed resources - the mip map descriptor heap, sampler, synchronization objects
/// </summary>
void HiZBuffer::CreateResources() {
	auto d3Device = m_deviceResources->GetD3DDevice();
	//Create graphics command allocators - one for rendering the occluders and one for generating the mip maps
	ThrowIfFailed(d3Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_HiZDepthCommandAllocator)));
	ThrowIfFailed(d3Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_HiZMIPMapCommandAllocator)));
	
	//Create HiZ Buffer description - it will be a two dimentional texture, filled with floats (the occluder's depths)
	D3D12_RESOURCE_DESC hizDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R32_FLOAT,
		static_cast<UINT>(m_width),
		static_cast<UINT>(m_height),
		1,
		8,
		1,
		0,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

	//Use white as clear color for the HiZ Buffer 
	D3D12_CLEAR_VALUE ClearValue = {};
	ClearValue.Format = DXGI_FORMAT_R32_FLOAT;
	ClearValue.Color[0] = ClearValue.Color[1] = ClearValue.Color[2] = ClearValue.Color[3] = 1.0f;
	//Create the HiZ buffer based on its description
	ThrowIfFailed(d3Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&hizDesc,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		&ClearValue,
		IID_PPV_ARGS(&m_pHizBuffer)));
	m_pHizBuffer->SetName(L"HIZ Buffer");

	// Get the new description now that the mip levels have been generated 
	hizDesc = m_pHizBuffer->GetDesc();

	//Create HiZ Depth buffer description - it is used ONLY when rendering the occluders, we will need to depth test them
	D3D12_RESOURCE_DESC hizDepthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_D32_FLOAT,
		static_cast<UINT>(m_width),
		static_cast<UINT>(m_height),
		1,
		1,
		1,
		0,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	//Create default depth clear value
	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;
	//Create HiZ Depth buffer based on its description 
	ThrowIfFailed(d3Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&hizDepthDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthOptimizedClearValue,
		IID_PPV_ARGS(&m_pHizDepthBuffer)));
	m_pHizDepthBuffer->SetName(L"HIZ Depth Buffer");

	//Create a resource with the HiZ Buffer description, which we will use as a copy destination when generating the mip maps
	//And later use it as SRV in the compute shader
	ThrowIfFailed(d3Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&hizDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&ClearValue,
		IID_PPV_ARGS(&m_pTempBuffer)));
	m_pTempBuffer->SetName(L"Temporal buffer");
	//Save the number of mip map levels
	m_mipLevels = hizDesc.MipLevels;
	//Create a descriptor heap where we will store each of the mip map levels
	D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavHeapDesc = {};
	cbvSrvUavHeapDesc.NumDescriptors = m_mipLevels;
	cbvSrvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvSrvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(d3Device->CreateDescriptorHeap(&cbvSrvUavHeapDesc, IID_PPV_ARGS(&m_MIPMapSRVHeap)));
	m_MIPMapSRVHeap->SetName(L"Mip MAP CBV_SRV_UAV Descriptor Heap");
	m_MIPMapSRVDescriptorSize = d3Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//Create Depth Stencil heap for the rendering of the occluders - only one DSV will be used
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(d3Device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_HiZDepthDSVHeap)));
	m_HiZDepthDSVHeap->SetName(L"Depth DSV Heap");

	//Create render target heap where we will render the mipmap levels
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = m_mipLevels;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(d3Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_MIPMapRTVHeap)));
	m_MIPMapRTVDescriptorSize = d3Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_MIPMapRTVHeap->SetName(L"MIPMap RTV Descriptor Heap");

	//Create the depth stencil view (it will be stored in the DSV heap)
	D3D12_DEPTH_STENCIL_VIEW_DESC hizDSVDesc;
	hizDSVDesc.Format = hizDepthDesc.Format;
	hizDSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	hizDSVDesc.Texture2D.MipSlice = 0;
	hizDSVDesc.Flags = D3D12_DSV_FLAG_NONE;
	d3Device->CreateDepthStencilView(m_pHizDepthBuffer.Get(), &hizDSVDesc, m_HiZDepthDSVHeap->GetCPUDescriptorHandleForHeapStart());
	// Generate the Render target and shader resource views that we'll need for rendering
	// the mip levels in the downsampling step.
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(m_MIPMapRTVHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvDescriptor(m_MIPMapSRVHeap->GetCPUDescriptorHandleForHeapStart());
	//We will fill each of the descriptors in the descriptor heaps 
	for (UINT miplevel = 0; miplevel < m_mipLevels; miplevel++)
	{
		//Describe render target view, at the current mip level
		D3D12_RENDER_TARGET_VIEW_DESC downSampleHizRTVD;
		downSampleHizRTVD.Format = hizDesc.Format;
		downSampleHizRTVD.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		downSampleHizRTVD.Texture2D.MipSlice = miplevel;
		downSampleHizRTVD.Texture2D.PlaneSlice = 0;
		//Create the RTV itself using the handle, pointing to its place in the descriptor heap 
		d3Device->CreateRenderTargetView(m_pHizBuffer.Get(), &downSampleHizRTVD, rtvDescriptor);
		//Offset the handle to point the place in the heap where the next RTV should be created
		rtvDescriptor.Offset(m_MIPMapRTVDescriptorSize);

		//Describe an SRV for the current mip level (will be used in the mip map generation pixel shader)
		D3D12_SHADER_RESOURCE_VIEW_DESC lastMipSRVD;
		lastMipSRVD.Format = hizDesc.Format;
		lastMipSRVD.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		lastMipSRVD.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		lastMipSRVD.Texture2D.MipLevels = 1;
		lastMipSRVD.Texture2D.MostDetailedMip = miplevel;
		lastMipSRVD.Texture2D.PlaneSlice = 0;
		lastMipSRVD.Texture2D.ResourceMinLODClamp = 0.0f;
		//Create the SRV using its description, using the handle, pointin its place in the descriptor heap
		d3Device->CreateShaderResourceView(m_pTempBuffer.Get(), &lastMipSRVD, srvDescriptor);
		//Offset the handle to point the place in the heap where the next SRV should be created
		srvDescriptor.Offset(m_MIPMapSRVDescriptorSize);
	}

	// Create a shader resource view that can see the entire mip chain, it will see all mipmap levels
	m_MIPMapChainDesc.Format = hizDesc.Format;
	m_MIPMapChainDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	m_MIPMapChainDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	m_MIPMapChainDesc.Texture2D.MipLevels = hizDesc.MipLevels;
	m_MIPMapChainDesc.Texture2D.MostDetailedMip = 0;
	m_MIPMapChainDesc.Texture2D.PlaneSlice = 0;
	m_MIPMapChainDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	
	// Create the sampler to use in the culling CS on the HZB texture
	// It uses point sampling as we are downsampling the next mip map level by finding the max depth of 4 pixels from the last level
	m_HizSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	m_HizSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	m_HizSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	m_HizSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	m_HizSamplerDesc.MipLODBias = 0.0f;
	m_HizSamplerDesc.MaxAnisotropy = 0;
	m_HizSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	m_HizSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	m_HizSamplerDesc.MinLOD = -FLT_MAX;
	m_HizSamplerDesc.MaxLOD = FLT_MAX;
	// Create synchronization objects.
	ThrowIfFailed(d3Device->CreateFence(m_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	m_fenceValue++;

	m_fenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
}