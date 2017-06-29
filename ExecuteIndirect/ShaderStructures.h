#pragma once
#include "DeviceResources.h"
namespace ExecuteIndirect
{
	using namespace DirectX;
	
	struct Light
	{
		XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
		float FalloffStart = 1.0f;                          // point/spot light only
		XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };// directional/spot light only
		float FalloffEnd = 10.0f;                           // point/spot light only
		XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  // point/spot light only
		float SpotPower = 64.0f;                            // spot light only
	};
	#define MaxLights 16

	struct MaterialData
	{
		XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
		float Roughness = 0.5f;

		// Used in texture mapping.
		DirectX::XMFLOAT4X4 MatTransform;
		UINT DiffuseMapIndex = 0;
		UINT NormalMapIndex = 0;
		UINT MaterialPad1;
		UINT MaterialPad2;
	};

	struct Material
	{
		// Unique material name for lookup.
		std::string Name;

		// Index into constant buffer corresponding to this material.
		int MatCBIndex = -1;
		UINT NumFramesDirty = DX::c_frameCount;
		MaterialData data;
	};

	struct Texture
	{
		std::string Name;
		std::string Filename;
		UINT index;
		Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
	};

	struct ModelConstantBuffer {
		XMFLOAT4X4 World;
		XMFLOAT4X4 TexTransform;
		UINT MaterialIndex;
		UINT mCBpadding1;
		UINT mCBpadding2;
		UINT mCBpadding3;
	};

	struct InstanceData {
		XMFLOAT4X4 World;
		XMFLOAT4X4 TexTransform;
		UINT MaterialIndex;
		UINT InstancePad0;
		UINT InstancePad1;
		UINT InstancePad2;
	};

	struct SceneConstantBuffer
	{
		XMFLOAT4X4 viewMatrix;
		XMFLOAT4X4 projectionMatrix;
		XMFLOAT4 viewportSize;
		XMFLOAT3 eyePosW;
		float sCBPadd1;
		XMFLOAT4 ambientLight;
		Light Lights[MaxLights];
	};

	// Used to send per-vertex data to the scene vertex shader.
	struct Vertex
	{
		XMFLOAT3 pos;
		XMFLOAT3 normal;
		XMFLOAT2 textureCoordinates;
		XMFLOAT3 tangent;
		
	};

	struct ComputeShaderConstants {
		UINT instanceCount;
		XMFLOAT3 boundingBoxCenter;
		XMFLOAT3 boundingBoxExtents;
	};

	// Data structure to match the command signature used for ExecuteIndirect.
	struct IndirectCommand
	{
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
		D3D12_INDEX_BUFFER_VIEW indexBufferView;
		D3D12_GPU_VIRTUAL_ADDRESS instancesShaderView;
		D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
	};
}