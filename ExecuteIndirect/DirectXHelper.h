#pragma once
#include "pch.h"
#include <ppltasks.h>	// For create_task
#include <comdef.h>

using Microsoft::WRL::ComPtr;
namespace DX
{
	

	class DxException
	{
	public:
		DxException() = default;
		DxException(HRESULT hr, const std::wstring& filename, int lineNumber) :
			ErrorCode(hr),
			Filename(filename),
			LineNumber(lineNumber)
		{

		}

		std::wstring ToString() const {
			_com_error err(ErrorCode);
			std::wstring msg = err.ErrorMessage();

			return L" Exception thrown in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
		}

		HRESULT ErrorCode = S_OK;
		std::wstring Filename;
		int LineNumber = -1;
	};

	inline std::wstring convertCharArrayToLPCWSTR(const char* charArray)
	{
		WCHAR buffer[512];
		MultiByteToWideChar(CP_ACP, 0, charArray, -1, buffer, 512);
		return std::wstring(buffer);
	}

	#ifndef ThrowIfFailed
	#define ThrowIfFailed(x)                                              \
		{                                                                     \
			HRESULT hr__ = (x);                                               \
			std::wstring wfn = DX::convertCharArrayToLPCWSTR(__FILE__);                       \
			if(FAILED(hr__)) { throw DX::DxException(hr__, wfn, __LINE__); } \
		}
	#endif

	inline UINT CalcConstantBufferByteSize(UINT byteSize)
	{
		
		return (byteSize + 255) & ~255;
	}

	// Converts a length in device-independent pixels (DIPs) to a length in physical pixels.
	inline float ConvertDipsToPixels(float dips, float dpi)
	{
		static const float dipsPerInch = 96.0f;
		return floorf(dips * dpi / dipsPerInch + 0.5f); // Round to nearest integer.
	}

	

	inline std::wstring GetAssetFullPath(std::wstring assetName)
	{
		WCHAR path[512];

		DWORD size = GetModuleFileName(nullptr, path, 512);
		if (size == 0)
		{
			// Method failed or path was truncated.
			throw std::exception();
		}
		WCHAR* lastSlash = wcsrchr(path, L'\\');
		if (lastSlash)
		{
			*(lastSlash + 1) = L'\0';
		}
		std::wstring fullPath(path);
		fullPath += assetName;
		return fullPath;

	}

	inline HRESULT ReadDataFromFile(LPCWSTR filename, std::vector<byte>& data)
	{
		using namespace Microsoft::WRL;

		CREATEFILE2_EXTENDED_PARAMETERS extendedParams = {};
		extendedParams.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
		extendedParams.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
		extendedParams.dwFileFlags = FILE_FLAG_SEQUENTIAL_SCAN;
		extendedParams.dwSecurityQosFlags = SECURITY_ANONYMOUS;
		extendedParams.lpSecurityAttributes = nullptr;
		extendedParams.hTemplateFile = nullptr;

		Wrappers::FileHandle file(CreateFile2(filename, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, &extendedParams));
		if (file.Get() == INVALID_HANDLE_VALUE)
		{
			throw std::exception();
		}

		FILE_STANDARD_INFO fileInfo = {};
		if (!GetFileInformationByHandleEx(file.Get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
		{
			throw std::exception();
		}

		if (fileInfo.EndOfFile.HighPart != 0)
		{
			throw std::exception();
		}

		data.resize(fileInfo.EndOfFile.LowPart);

		if (!ReadFile(file.Get(), &data[0], fileInfo.EndOfFile.LowPart, nullptr, nullptr))
		{
			throw std::exception();
		}

		return S_OK;
	}

	inline float RandF()
	{
		
		return (float)(rand()) / (float)RAND_MAX;
	}

	// Returns random float in [a, b).
	inline float RandF(float a, float b)
	{
		
		return a + RandF()*(b - a);
	}

	inline int Rand(int a, int b)
	{
		
		return a + rand() % ((b - a) + 1);
	}

	template<typename T>
	inline T Min(const T& a, const T& b)
	{
		return a < b ? a : b;
	}

	template<typename T>
	inline T Max(const T& a, const T& b)
	{
		return a > b ? a : b;
	}

	inline void CreateReadbackBuffer(ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& readbackBuffer)
	{
		D3D12_RESOURCE_DESC readbackBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize, D3D12_RESOURCE_FLAG_NONE);
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
			D3D12_HEAP_FLAG_NONE,
			&readbackBufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&readbackBuffer)));
	}

	inline void CreateUAVBuffer(ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uavBuffer)
	{
		D3D12_RESOURCE_DESC uavBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&uavBufferDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&uavBuffer)));
	}

	inline void CreateDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer,
		Microsoft::WRL::ComPtr<ID3D12Resource>& defaultBuffer)
	{
		// Create the actual default buffer resource.
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&defaultBuffer)));

		// In order to copy CPU memory data into our default buffer, we need to create
		// an intermediate upload heap. 
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffer)));

			// Describe the data we want to copy into the default buffer.
			D3D12_SUBRESOURCE_DATA subResourceData = {};
			subResourceData.pData = initData;
			subResourceData.RowPitch = byteSize;
			subResourceData.SlicePitch = subResourceData.RowPitch;

			// Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
			// will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
			// the intermediate upload heap data will be copied to mBuffer.
			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
				D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
			UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);
			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

		// Note: uploadBuffer has to be kept alive after the above function calls because
		// the command list has not been executed yet that performs the actual copy.
		// The caller can Release the uploadBuffer after it knows the copy has been executed.
	}

	inline UINT AlignForUavCounter(UINT bufferSize)
	{
		const UINT alignment = D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT;
		return (bufferSize + (alignment - 1)) & ~(alignment - 1);
	}


}