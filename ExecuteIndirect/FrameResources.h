#pragma once
#include "d3dx12.h"
#include "DirectXHelper.h"
#include "ShaderStructures.h"

namespace ExecuteIndirect {

	template<typename T>
	class UploadBuffer
	{
	public:
		UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer) 
			
		{
			m_elementCount = elementCount;
			mElementByteSize = sizeof(T);
			if (isConstantBuffer)
				mElementByteSize = DX::CalcConstantBufferByteSize(sizeof(T));

			ThrowIfFailed(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize*elementCount),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&mUploadBuffer)));

			ThrowIfFailed(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));

			// We do not need to unmap until we are done with the resource.  However, we must not write to
			// the resource while it is in use by the GPU (so we must use synchronization techniques).
		}

		UploadBuffer(const UploadBuffer& rhs) = delete;
		UploadBuffer& operator=(const UploadBuffer& rhs) = delete;
		~UploadBuffer()
		{
			if (mUploadBuffer != nullptr)
				mUploadBuffer->Unmap(0, nullptr);

			mMappedData = nullptr;
		}

		ID3D12Resource* Resource()const
		{
			return mUploadBuffer.Get();
		}

		void CopyData(int elementIndex, const T& data)
		{
			memcpy(&mMappedData[elementIndex*mElementByteSize], &data, mElementByteSize);
		}

		UINT ElementCount() { return m_elementCount; }

		BYTE* GetData(int elementIndex) {
			return mMappedData + (elementIndex*mElementByteSize);
		}

		UINT GetElementByteSize() { return mElementByteSize; }

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
		BYTE* mMappedData = nullptr;
		UINT mElementByteSize = 0;
		UINT m_elementCount = 0;
		bool mIsConstantBuffer = false;
	};



	class FrameResources
	{
	public:
		FrameResources(ID3D12Device* device, UINT passCount, UINT modelCount, UINT materialCount);
		~FrameResources();

		// We cannot update a cbuffer until the GPU is done processing the commands
		// that reference it.  So each frame needs their own cbuffers.
		std::unique_ptr<UploadBuffer<SceneConstantBuffer>> SceneCB = nullptr;
		std::unique_ptr<UploadBuffer<ModelConstantBuffer>> ModelCB = nullptr;
		std::unique_ptr<UploadBuffer<MaterialData>> MaterialBuffer = nullptr;
	};

}