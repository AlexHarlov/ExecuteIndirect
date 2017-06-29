#pragma once
#include <fstream>
#include <unordered_map>
#include "d3dx12.h"
#include <sstream>
#include "RenderItem.h"
#include "tinyObjLoader.h"

using namespace DirectX;

namespace ExecuteIndirect {
	
	class OBJLoader
	{
	public:
		OBJLoader();
		const char * MapFile(size_t * len, const char * filename);
		void ReadBinFiles(std::unordered_map<std::string, std::unique_ptr<RenderItem>>& rItems,
							std::unordered_map<std::string, std::unique_ptr<Texture>>& diffuseMaps,
							std::unordered_map<std::string, std::unique_ptr<Texture>>& normalMaps,
							std::unordered_map<std::string, std::unique_ptr<Material>>& materials);

		void ReadOBJFiles(const char* fileNames[], UINT numFiles, 
						std::unordered_map<std::string, std::unique_ptr<RenderItem>>& rItems,
						std::unordered_map<std::string, std::unique_ptr<Texture>>& diffuseMaps,
						std::unordered_map<std::string, std::unique_ptr<Texture>>& normalMaps,
						std::unordered_map<std::string, std::unique_ptr<Material>>& materials);

		void WriteBinRenderItems(std::unordered_map<std::string, std::unique_ptr<RenderItem>>& rItems, const char * fileName);

		void WriteBinMaterialsAndTextures(std::unordered_map<std::string, std::unique_ptr<Texture>>& diffuseMaps, std::unordered_map<std::string, std::unique_ptr<Texture>>& normalMaps, std::unordered_map<std::string, std::unique_ptr<Material>>& to, const char * fileName);

		void ReadBinMaterialsAndTextures(std::unordered_map<std::string, std::unique_ptr<Texture>>& diffuseMaps, std::unordered_map<std::string, std::unique_ptr<Texture>>& normalMaps, std::unordered_map<std::string, std::unique_ptr<Material>>& to, const char * fileName);

		void ReadBinRenderItems(std::unordered_map<std::string, std::unique_ptr<RenderItem>>& rItems, const char * fileName);

	private:
		/*
		void LoadMeshData(std::vector<std::unique_ptr<RenderItem>>& rItems, std::unordered_map<std::string, std::unique_ptr<Material>>& materials, std::string meshName);
		void LoadMaterials(std::string matFileName, std::unordered_map<std::string, std::unique_ptr<Texture>>& textures,
						std::unordered_map<std::string, std::unique_ptr<Material>>& materials);

		*/
		void LoadMaterials(std::vector<tinyobj_opt::material_t>& from, 
			std::unordered_map<std::string, std::unique_ptr<Texture>>& diffuseMaps,
			std::unordered_map<std::string, std::unique_ptr<Texture>>& normalMaps,
			std::unordered_map<std::string, std::unique_ptr<Material>>& to);

		void LoadVertexData(tinyobj_opt::attrib_t& attributes, 
			std::vector<tinyobj_opt::shape_t>& shapes, 
			std::vector<tinyobj_opt::material_t>& from,
			std::unordered_map<std::string, std::unique_ptr<Material>>& to,
			std::unordered_map<std::string, std::unique_ptr<RenderItem>>& rItems);

		std::ifstream m_fileReader;
		std::ifstream m_mtlReader;
		std::stringstream m_memoryReader;
		XMFLOAT3* m_tempPos;
		XMFLOAT2* m_tempTexCoord;
		XMFLOAT3* m_tempNormal;
		Vertex* vertices;
		UINT* indices;
		UINT textureCounter;
		UINT materialCounter;
		UINT positionCounter;
		UINT textCoordCounter;
		UINT normalCounter;
	};

}