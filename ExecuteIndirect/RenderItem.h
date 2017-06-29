#pragma once
#include <unordered_map>
#include <DirectXCollision.h>
#include "ShaderStructures.h"
#include "DeviceResources.h"

namespace ExecuteIndirect {
	
	class RenderItem
	{
	public:
		RenderItem(int vertexCount, int indexCount);
		void CreateCounterOffset();
		~RenderItem();
		void ReleaseCPUBuffers();

	private:
		Vertex* VertexBufferCPU;
		UINT* IndexBufferCPU;

		Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> Texture = nullptr;

		Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> TextureUploader = nullptr;

		Microsoft::WRL::ComPtr<ID3D12Resource> InstanceBufferGPU = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> ProcessedInstanceBufferGPU = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> InstanceBufferUploader = nullptr;

		// Data about the buffers.
		UINT VertexByteStride = 0;
		UINT VertexBufferByteSize = 0;
		DXGI_FORMAT IndexFormat = DXGI_FORMAT_R32_UINT;
		UINT IndexBufferByteSize = 0;
		UINT InstanceBufferCounterOffset = 0;

		UINT VertexCount;
		UINT IndexCount;
		UINT CommandCount;
		UINT TriangleCount;
		UINT NumFramesDirty = DX::c_frameCount;
		XMFLOAT4X4 World;
		XMFLOAT4X4 TexTransformMatrix;
		UINT materialInx;

		std::vector<InstanceData> Instances;
		BoundingBox boundingBox;
		bool isItemOccluder = false;

	public:
		UINT GetVertexCount() { return VertexCount; }
		UINT GetIndexCount() { return IndexCount; }
		UINT GetCommandCount() { return CommandCount; }
		UINT GetTriangleCount() { return TriangleCount; }

		Microsoft::WRL::ComPtr<ID3D12Resource>& GetVertexBufferGPU() { return VertexBufferGPU; }
		Microsoft::WRL::ComPtr<ID3D12Resource>& GetIndexBufferGPU()  { return IndexBufferGPU; }

		Microsoft::WRL::ComPtr<ID3D12Resource>& GetVertexBufferUploader() { return VertexBufferUploader; }
		Microsoft::WRL::ComPtr<ID3D12Resource>& GetIndexBufferUploader() { return IndexBufferUploader; }

		Microsoft::WRL::ComPtr<ID3D12Resource>& GetInstanceBufferGPU() { return InstanceBufferGPU; }
		Microsoft::WRL::ComPtr<ID3D12Resource>& GetInstanceBufferUploader() { return InstanceBufferUploader; }
		Microsoft::WRL::ComPtr<ID3D12Resource>& GetProcessedInstanceBufferGPU() { return ProcessedInstanceBufferGPU; }

		Vertex* GetVertexBufferData() { return VertexBufferCPU; }
		UINT* GetIndexBufferData() { return IndexBufferCPU; }
		DirectX::BoundingBox& GetBoundingBoxData() { return boundingBox; }
		UINT GetInstanceBufferCounterOffset() { return InstanceBufferCounterOffset; }
		UINT GetNumFramesDirty() { return NumFramesDirty; }
		XMFLOAT4X4& GetWorldMatrix() { return World; }
		XMFLOAT4X4& GetTexTransformMatrix() { return TexTransformMatrix; }
		std::vector<InstanceData>& GetInstances() { return Instances; }

		bool isOccluder() { return isItemOccluder; }

		UINT GetMaterialIndex() { return materialInx; }
		UINT GetVertexByteStride() { return VertexByteStride; }
		UINT GetVertexBufferByteSize() { return VertexBufferByteSize; }
		UINT GetIndexBufferByteSize() { return IndexBufferByteSize; }
		UINT GetInstancesByteSize() { return Instances.size() * sizeof(InstanceData); }
		UINT GetInstanceCount() { return Instances.size(); }

		void SetVertexByteStride(UINT byteStride) { VertexByteStride = byteStride; }
		void SetVertexBufferByteSize(UINT byteSize) { VertexBufferByteSize = byteSize; }
		void SetIndexBufferByteSize(UINT byteSize) { IndexBufferByteSize = byteSize; }
		void SetMaterialIndex(UINT m) { materialInx = m; }
		void SetNumFramesDirty(UINT n) { NumFramesDirty = n; }
		void SetWorldMatrix(XMFLOAT4X4& m) { World = m; }
		void SetTextureTransformMatrix(XMFLOAT4X4& m) { TexTransformMatrix = m; }
		void SetOccluder(bool state) { isItemOccluder = state; }
		void CreateBoundingBox();

		D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
		{
			D3D12_VERTEX_BUFFER_VIEW vbv;
			vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
			vbv.StrideInBytes = VertexByteStride;
			vbv.SizeInBytes = VertexBufferByteSize;

			return vbv;
		}

		D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
		{
			D3D12_INDEX_BUFFER_VIEW ibv;
			ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
			ibv.Format = IndexFormat;
			ibv.SizeInBytes = IndexBufferByteSize;

			return ibv;
		}

		// We can free this memory after we finish upload to the GPU.
		void DisposeUploaders()
		{
			VertexBufferUploader = nullptr;
			IndexBufferUploader = nullptr;
		}
	};
}

