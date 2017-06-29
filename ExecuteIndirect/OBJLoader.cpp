#include "pch.h"
#include "OBJLoader.h"
#include "ShaderStructures.h"
#include <string>
#include "DirectXHelper.h"
#include <stdio.h>
#include <errno.h>

#include <windows.h>


using namespace DirectX;
using namespace ExecuteIndirect;

/// <summary>
/// Calculates the normal.
/// </summary>
/// <param name="vertex1">vertex 1.</param>
/// <param name="vertex2">vertex 2.</param>
/// <param name="vertex3">vertex 3.</param>
/// <param name="normal">The normal.</param>
void CalculateNormal(Vertex& vertex1, Vertex& vertex2, Vertex& vertex3, XMFLOAT3& normal) {
	XMFLOAT3 vector1, vector2;
	//Calculate the vectors for the cross product
	vector1.x = vertex2.pos.x - vertex1.pos.x;
	vector1.y = vertex2.pos.y - vertex1.pos.y;
	vector1.z = vertex2.pos.z - vertex1.pos.z;

	vector2.x = vertex3.pos.x - vertex1.pos.x;
	vector2.y = vertex3.pos.y - vertex1.pos.y;
	vector2.z = vertex3.pos.z - vertex1.pos.z;
	//Calculate the cross product and normalize it
	XMVECTOR cross = XMVector3Cross(XMLoadFloat3(&vector1), XMLoadFloat3(&vector2));
	XMStoreFloat3(&normal, XMVector3Normalize(cross));
}

/// <summary>
/// Calculates the tangent.
/// </summary>
/// <param name="vertex1">Vertex1.</param>
/// <param name="vertex2">Vertex2.</param>
/// <param name="vertex3">Vertex3.</param>
/// <param name="tangent">tangent.</param>
void CalculateTangent(Vertex& vertex1, Vertex& vertex2, Vertex& vertex3, XMFLOAT3& tangent)
{
	XMFLOAT3 vector1, vector2;
	XMFLOAT2 tuVector, tvVector;
	float den;

	// Calculate the two vectors for this face.
	vector1.x = vertex2.pos.x - vertex1.pos.x;
	vector1.y = vertex2.pos.y - vertex1.pos.y;
	vector1.z = vertex2.pos.z - vertex1.pos.z;

	vector2.x = vertex3.pos.x - vertex1.pos.x;
	vector2.y = vertex3.pos.y - vertex1.pos.y;
	vector2.z = vertex3.pos.z - vertex1.pos.z;

	// Calculate the tu and tv texture space vectors.
	tuVector.x = vertex2.textureCoordinates.x - vertex1.textureCoordinates.x;
	tvVector.x = vertex2.textureCoordinates.y - vertex1.textureCoordinates.y;

	tuVector.y = vertex3.textureCoordinates.x - vertex1.textureCoordinates.x;
	tvVector.y = vertex3.textureCoordinates.y - vertex1.textureCoordinates.y;

	// Calculate the denominator of the tangent/binormal equation.
	den = 1.0f / (tuVector.x * tvVector.y - tuVector.y * tvVector.x);

	// Calculate the cross products and multiply by the coefficient to get the tangent and binormal.
	tangent.x = (tvVector.y * vector1.x - tvVector.x * vector2.x) * den;
	tangent.y = (tvVector.y * vector1.y - tvVector.x * vector2.y) * den;
	tangent.z = (tvVector.y * vector1.z - tvVector.x * vector2.z) * den;

	// Normalize the tangent and then store it.
	XMStoreFloat3(&tangent, XMVector3Normalize(XMLoadFloat3(&tangent)));
}

OBJLoader::OBJLoader() : textureCounter(0), materialCounter(0), normalCounter(0)
{
}



/// <summary>
/// Helper function to map the file to pointer
/// </summary>
/// <param name="len">The length of the file.</param>
/// <param name="filename">File's name.</param>
/// <returns></returns>
const char* OBJLoader::MapFile(size_t *len, const char* filename)
{
	(*len) = 0;
	HANDLE file = CreateFile2(DX::convertCharArrayToLPCWSTR(filename).c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NULL);
	assert(file != INVALID_HANDLE_VALUE);

	HANDLE fileMapping = CreateFileMapping(file, NULL, PAGE_READONLY, 0, 0, NULL);
	assert(fileMapping != INVALID_HANDLE_VALUE);

	LPVOID fileMapView = MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);
	auto fileMapViewChar = (const char*)fileMapView;
	assert(fileMapView != NULL);

	LARGE_INTEGER fileSize;
	fileSize.QuadPart = 0;
	GetFileSizeEx(file, &fileSize);

	(*len) = static_cast<size_t>(fileSize.QuadPart);
	return fileMapViewChar;

}

/// <summary>
/// Reads the application's structures from binary file
/// </summary>
/// <param name="rItems">The render items map.</param>
/// <param name="diffuseMaps">The diffuse maps.</param>
/// <param name="normalMaps">The normal maps.</param>
/// <param name="materials">The materials.</param>
void OBJLoader::ReadBinFiles(std::unordered_map<std::string, std::unique_ptr<RenderItem>>& rItems,
							std::unordered_map<std::string, std::unique_ptr<Texture>>& diffuseMaps,
							std::unordered_map<std::string, std::unique_ptr<Texture>>& normalMaps,
							std::unordered_map<std::string, std::unique_ptr<Material>>& materials)
{
	ReadBinRenderItems(rItems, "models\\scene.bin");
	ReadBinMaterialsAndTextures(diffuseMaps, normalMaps, materials, "models\\materials.bin");
}

void OBJLoader::ReadOBJFiles(const char* fileNames[], UINT numFiles, 
							std::unordered_map<std::string, std::unique_ptr<RenderItem>>& rItems,
							std::unordered_map<std::string, std::unique_ptr<Texture>>& diffuseMaps,
							std::unordered_map<std::string, std::unique_ptr<Texture>>& normalMaps,
							std::unordered_map<std::string, std::unique_ptr<Material>>& materials)
{
	
	for (UINT i = 0; i < numFiles; i++) {
		tinyobj_opt::attrib_t attrib;
		std::vector<tinyobj_opt::shape_t> shapes;
		std::vector<tinyobj_opt::material_t> materialz;
		std::string err;
		const char* fileName = fileNames[i];
		size_t data_len = 0;
		const char* data;
		data = MapFile(&data_len, fileName);

		tinyobj_opt::LoadOption option;
		option.req_num_threads = 8;
		option.verbose = false;
		bool ret = parseObj(&attrib, &shapes, &materialz, data, data_len, option);

		if (!ret) {
			std::cerr << "Failed to parse .obj" << std::endl;

		}
		else {
			LoadMaterials(materialz, diffuseMaps, normalMaps, materials);
			LoadVertexData(attrib, shapes, materialz, materials, rItems);
				
		}
	}
	WriteBinRenderItems(rItems, "models\\scene.bin");
	WriteBinMaterialsAndTextures(diffuseMaps, normalMaps, materials, "models\\materials.bin");
}

/// <summary>
/// Write the application's render item structures in a binary file for faster loading
/// </summary>
/// <param name="rItems">The render items map.</param>
/// <param name="binFileName">Name of the bin file.</param>
void OBJLoader::WriteBinRenderItems(std::unordered_map<std::string, std::unique_ptr<RenderItem>>& rItems, const char* binFileName) {
	std::ofstream stream;
	stream.open(binFileName, std::ios::trunc | std::ios::binary);
	if (stream.is_open()) {
		UINT riSize = rItems.size();
		stream.write((const char*)&riSize, sizeof(UINT));
		for (std::unordered_map<std::string, std::unique_ptr<RenderItem>>::iterator iter = rItems.begin(); iter != rItems.end(); iter++) {
			std::string riName = iter->first;
			UINT riNameSize = riName.size() + 1;
			stream.write((const char*)&riNameSize, sizeof(UINT));
			stream.write((const char*)riName.data(), riNameSize);

			UINT vByteSize = iter->second->GetVertexBufferByteSize();
			UINT iByteSize = iter->second->GetIndexBufferByteSize();

			stream.write((const char*)&vByteSize, sizeof(UINT));
			stream.write((const char*)&iByteSize, sizeof(UINT));

			stream.write((const char*)iter->second->GetVertexBufferData(), vByteSize);
			stream.write((const char*)iter->second->GetIndexBufferData(), iByteSize);
			UINT materialIndex = iter->second->GetMaterialIndex();

			stream.write((const char*)&materialIndex, sizeof(UINT));
			XMFLOAT4X4 WorldMatrix(iter->second->GetWorldMatrix());
			XMFLOAT4X4 TextTransformMatrix(iter->second->GetTexTransformMatrix());
			stream.write((const char*)&WorldMatrix, sizeof(XMFLOAT4X4));
			stream.write((const char*)&TextTransformMatrix, sizeof(XMFLOAT4X4));

		}
		stream.close();
	}
	else {
		char errorStr[100];
		strerror_s(errorStr, 100,  errno);
	}
	
}

/// <summary>
/// Read the application's structures from a binary file for faster loading
/// </summary>
/// <param name="rItems">The render items map.</param>
/// <param name="origFileName">Name of the binary file.</param>
void OBJLoader::ReadBinRenderItems(std::unordered_map<std::string, std::unique_ptr<RenderItem>>& rItems, const char* origFileName) {
	std::wstring binFileName(DX::convertCharArrayToLPCWSTR(origFileName));
	binFileName = binFileName.substr(0, binFileName.find_last_of('.'));
	binFileName.append(L".bin");
	std::ifstream stream;
	stream.open(binFileName, std::ios::binary);
	UINT riSize;
	if (stream.is_open()) {
		stream.read((char*)&riSize, sizeof(UINT));
		while(riSize--) {
			UINT vByteSize;
			UINT iByteSize;
			UINT materialIndex;
			UINT modelCBIndex;
			UINT riNameSize;
	
			stream.read((char*)&riNameSize, sizeof(UINT));
			char* riName = new char[riNameSize];
			stream.read(riName, riNameSize);
			stream.read((char*)&vByteSize, sizeof(UINT));
			stream.read((char*)&iByteSize, sizeof(UINT));			
			auto ri = std::make_unique<RenderItem>(vByteSize/sizeof(Vertex), iByteSize/sizeof(UINT));
			stream.read((char*)ri->GetVertexBufferData(), vByteSize);
			stream.read((char*)ri->GetIndexBufferData(), iByteSize);
			stream.read((char*)&materialIndex, sizeof(UINT));
			ri->SetMaterialIndex(materialIndex);
			XMFLOAT4X4 WorldMatrix;
			XMFLOAT4X4 TextTransformMatrix;
			stream.read((char*)&WorldMatrix, sizeof(XMFLOAT4X4));
			stream.read((char*)&TextTransformMatrix, sizeof(XMFLOAT4X4));
			ri->SetWorldMatrix(WorldMatrix);
			ri->SetTextureTransformMatrix(TextTransformMatrix);
			rItems[riName] = std::move(ri);
		}
		stream.close();
	}
	else {
		char errorStr[100];
		strerror_s(errorStr, 100, errno);
	}

}


/// <summary>
/// Write the application's material structures in a binary file for faster loading
/// </summary>
/// <param name="diffuseMaps">The diffuse maps.</param>
/// <param name="normalMaps">The normal maps.</param>
/// <param name="to">The material map</param>
/// <param name="binFileName">Name of the binary file.</param>
void OBJLoader::WriteBinMaterialsAndTextures(std::unordered_map<std::string, std::unique_ptr<Texture>>& diffuseMaps,
											 std::unordered_map<std::string, std::unique_ptr<Texture>>& normalMaps,
											 std::unordered_map<std::string, std::unique_ptr<Material>>& to, const char* binFileName) {
	std::ofstream stream;
	stream.open(binFileName, std::ios::trunc | std::ios::binary);
	if (stream.is_open()) {
		UINT diffuseMapsSize = diffuseMaps.size();
		UINT normalMapsSize = normalMaps.size();
		UINT materialSize = to.size();
		UINT fileNameSize;
		UINT nameSize;
		stream.write((const char*)&diffuseMapsSize, sizeof(UINT));
		for (auto& map : diffuseMaps) {
			fileNameSize = map.second->Filename.size() + 1;
			stream.write((const char*)&fileNameSize, sizeof(UINT));
			stream.write(map.second->Filename.c_str(), fileNameSize);

			nameSize = map.second->Name.size() + 1;
			stream.write((const char*)&nameSize, sizeof(UINT));
			stream.write(map.second->Name.c_str(), nameSize);
			stream.write((const char*)&map.second->index, sizeof(UINT));
		}		
		stream.write((const char*)&normalMapsSize, sizeof(UINT));
		for (auto& map : normalMaps) {
			fileNameSize = map.second->Filename.size() + 1;
			stream.write((const char*)&fileNameSize, sizeof(UINT));
			stream.write(map.second->Filename.data(), fileNameSize);

			nameSize = map.second->Name.size() + 1;
			stream.write((const char*)&nameSize, sizeof(UINT));
			stream.write(map.second->Name.data(), nameSize);
			stream.write((const char*)&map.second->index, sizeof(UINT));
		}	
		stream.write((const char*)&materialSize, sizeof(UINT));
		for (auto& material : to) {
			stream.write((const char*)&material.second->data, sizeof(MaterialData));
			stream.write((const char*)&material.second->MatCBIndex, sizeof(UINT));
			nameSize = material.second->Name.size() + 1;
			stream.write((const char*)&nameSize, sizeof(UINT));
			stream.write(material.second->Name.data(), nameSize);
		}

		stream.close();
	}
	else {
		char errorStr[100];
		strerror_s(errorStr, 100, errno);
	}

}

/// <summary>
/// Read the application's material structures from a binary file for faster loading
/// </summary>
/// <param name="diffuseMaps">The diffuse maps.</param>
/// <param name="normalMaps">The normal maps.</param>
/// <param name="to">The material map</param>
/// <param name="binFileName">Name of the binary file.</param>
void OBJLoader::ReadBinMaterialsAndTextures(std::unordered_map<std::string, std::unique_ptr<Texture>>& diffuseMaps,
									std::unordered_map<std::string, std::unique_ptr<Texture>>& normalMaps,
									std::unordered_map<std::string, std::unique_ptr<Material>>& to, const char* binFileName){

	std::ifstream stream;
	stream.open(binFileName, std::ios::binary);
	if (stream.is_open()) {
		UINT diffuseMapsSize, normalMapsSize, materialSize;
		UINT fileNameSize;
		UINT nameSize;
		char* fileName = nullptr;
		char* name = nullptr;
		stream.read((char*)&diffuseMapsSize, sizeof(UINT));
		while (diffuseMapsSize--) {
			auto map = std::make_unique<Texture>();
			stream.read((char*)&fileNameSize, sizeof(UINT));
			fileName = new char[fileNameSize];
			stream.read(fileName, fileNameSize);
			map->Filename = std::string(fileName);
			delete[] fileName;

			stream.read((char*)&nameSize, sizeof(UINT));
			name = new char[nameSize];
			stream.read(name, nameSize);
			map->Name = std::string(name);
			delete[] name;

			stream.read((char*)&map->index, sizeof(UINT));
			diffuseMaps[map->Name] = std::move(map);
		}
		stream.read((char*)&normalMapsSize, sizeof(UINT));
		while (normalMapsSize--) {
			auto map = std::make_unique<Texture>();
			stream.read((char*)&fileNameSize, sizeof(UINT));
			fileName = new char[fileNameSize];
			stream.read(fileName, fileNameSize);
			map->Filename = std::string(fileName);
			delete[] fileName;

			stream.read((char*)&nameSize, sizeof(UINT));
			name = new char[nameSize];
			stream.read(name, nameSize);
			map->Name = std::string(name);
			delete[] name;

			stream.read((char*)&map->index, sizeof(UINT));
			normalMaps[map->Name] = std::move(map);
		}
		stream.read((char*)&materialSize, sizeof(UINT));
		while(materialSize--){
			auto mat = std::make_unique<Material>();

			stream.read((char*)&mat->data, sizeof(MaterialData));
			stream.read((char*)&mat->MatCBIndex, sizeof(UINT));

			stream.read((char*)&nameSize, sizeof(UINT));
			name = new char[nameSize];
			stream.read(name, nameSize);
			mat->Name = std::string(name);
			delete[] name;
			to[mat->Name] = std::move(mat);
		}
		stream.close();
	}
	else {
		char errorStr[100];
		strerror_s(errorStr, 100, errno);
	}

}


/// <summary>
/// Loads the material data from the tinyobj loader structures to the structures used by the application.
/// </summary>
/// <param name="from">The materials loaded from tinyobj</param>
/// <param name="diffuseMaps">The diffuse maps.</param>
/// <param name="normalMaps">The normal maps.</param>
/// <param name="to">Application's material map</param>
void OBJLoader::LoadMaterials(std::vector<tinyobj_opt::material_t>& from, std::unordered_map<std::string, std::unique_ptr<Texture>>& diffuseMaps,
							std::unordered_map<std::string, std::unique_ptr<Texture>>& normalMaps, std::unordered_map<std::string, std::unique_ptr<Material>>& to)
{
	std::string difuseName, difuseFileName, normalName, normalFileName;
	for (int i = 0; i < from.size(); i++) {
		auto mat = std::make_unique<Material>();
		mat->Name = from[i].name;
		//Load texture name and filename 
		difuseName = from[i].diffuse_texname;
		difuseFileName = from[i].diffuse_texname;
		//change the name to find .dds extention file
		int t = difuseName.rfind('.');
		int sl = difuseName.rfind('/');
		difuseName = difuseName.substr(sl + 1, t - sl-1);
		difuseFileName = difuseFileName.substr(0, t);
		difuseFileName.append(".dds");
		//most MTL files don't contain normal maps so use the diffuse name, and append postfix _NORM
		if (from[i].normal_texname.empty()) {
			normalName = from[i].diffuse_texname;
			normalFileName = from[i].diffuse_texname;
			t = normalName.rfind('.');
			sl = normalName.rfind('/');
			normalName = normalName.substr(sl + 1, t - sl - 1);
			normalFileName = normalFileName.substr(0, t);
			normalFileName.append("_NORM.dds");
		}
		else {
			normalName = from[i].normal_texname;
			normalFileName = from[i].normal_texname;
		}
		
		//Check if diffuse map already exists in the unordered_map
		if (diffuseMaps.find(difuseName) == diffuseMaps.end()) {
			auto texture = std::make_unique<Texture>();
			texture->Name = difuseName;
			texture->Filename = difuseFileName;
			texture->index = textureCounter++;
			diffuseMaps[texture->Name] = std::move(texture);
		}
		//Check if normal map already exists in the unordered_map
		if (normalMaps.find(normalName) == normalMaps.end()) {
			auto normalMap = std::make_unique<Texture>();
			normalMap->Name = normalName;
			normalMap->Filename = normalFileName;
			normalMap->index = normalCounter++;
			normalMaps[normalMap->Name] = std::move(normalMap);
		}
		//Initialize the material object with the indices and default values for fresnel, albedo and roughness
		mat->data.DiffuseMapIndex = diffuseMaps[difuseName]->index;
		mat->data.NormalMapIndex = normalMaps[normalName]->index;
		mat->data.DiffuseAlbedo = XMFLOAT4(0.9f, 0.9f, 0.9f, 1.0f);
		mat->data.FresnelR0 = XMFLOAT3(0.5f, 0.5f, 0.5f);
		mat->data.Roughness = 0.5f;
		XMStoreFloat4x4(&mat->data.MatTransform, XMMatrixIdentity());
		mat->MatCBIndex = to.size();
		//store the object in the material map
		to[mat->Name] = std::move(mat);
	}
}

/// <summary>
/// Loads the vertex data from the tinyobj loader structures to the structures used by the application.
/// </summary>
/// <param name="attributes">The attributes loaded from tinyobj.</param>
/// <param name="shapes">The shapes loaded from tinyobj.</param>
/// <param name="from">The materials loaded from tinyobj</param>
/// <param name="to">Application's material map</param>
/// <param name="rItems">Application's render item map</param>
void OBJLoader::LoadVertexData(tinyobj_opt::attrib_t & attributes,
								std::vector<tinyobj_opt::shape_t>& shapes, 
								std::vector<tinyobj_opt::material_t>& from,
								std::unordered_map<std::string, std::unique_ptr<Material>>& to,
								std::unordered_map<std::string, std::unique_ptr<RenderItem>>& rItems)
{
	
	size_t index_offset = 0;
	for (size_t i = 0; i < shapes.size(); i++) {
		std::vector<Vertex> vertexBuffer;
		std::vector<UINT> indexBuffer;
		size_t indexCounter = 0;
		//each shape consists of faces, stored in the attribute's arrays 
		int faceOffset = shapes[i].face_offset;
		int faceCount = shapes[i].length;
		//some models don't have normals, so we can calculate them
		bool hasNormals = !attributes.normals.empty();
		for (int inx = faceOffset; inx < faceOffset + faceCount; inx++) {
			//get the number of vertices per face - if the triangulation option was enabled than vCount is always 3
			int vCount = attributes.face_num_verts[inx];
			for (int v = 0; v < vCount; v++) {
				//get the vertex index offset
				tinyobj_opt::index_t idx = attributes.indices[index_offset + v];
				Vertex tempVertex;
				//get the vertex position
				tempVertex.pos.x = attributes.vertices[3 * idx.vertex_index];
				tempVertex.pos.y = attributes.vertices[3 * idx.vertex_index + 1];
				tempVertex.pos.z = attributes.vertices[3 * idx.vertex_index + 2];
				if (hasNormals) {
					//get the vertex normal
					tempVertex.normal.x = attributes.normals[3 * idx.normal_index];
					tempVertex.normal.y = attributes.normals[3 * idx.normal_index + 1];
					tempVertex.normal.z = attributes.normals[3 * idx.normal_index + 2];
				}
				//get the vertex texture coordinates
				tempVertex.textureCoordinates.x = attributes.texcoords[2 * idx.texcoord_index];
				tempVertex.textureCoordinates.y = 1.0f - attributes.texcoords[2 * idx.texcoord_index+1];
				//store data in temporary vectors
				vertexBuffer.push_back(tempVertex);
				indexBuffer.push_back(indexCounter++);

			}
			XMFLOAT3 tangent;
			size_t last = vertexBuffer.size() - 1;
			//Calculate the vertex tangents
			CalculateTangent(vertexBuffer[last], vertexBuffer[last - 1], vertexBuffer[last - 2], tangent);
			vertexBuffer[last].tangent = tangent;
			vertexBuffer[last-1].tangent = tangent;
			vertexBuffer[last-2].tangent = tangent;
			if (!hasNormals) {
				//if the OBJ file didn't have vn (normal) attribute, compute it using cross product
				XMFLOAT3 normal;
				size_t last = vertexBuffer.size() - 1;
				CalculateNormal(vertexBuffer[last], vertexBuffer[last - 1], vertexBuffer[last - 2], normal);
				vertexBuffer[last].normal = normal;
				vertexBuffer[last - 1].normal = normal;
				vertexBuffer[last - 2].normal = normal;
			}
			index_offset += vCount;
		}
		//get the material index and material name
		int matInx = attributes.material_ids[faceOffset];
		std::string matName = from[matInx].name;
		//create the render item
		auto ri = std::make_unique<RenderItem>(vertexBuffer.size(), indexBuffer.size());
		//copy the vertex and index data
		memcpy(ri->GetVertexBufferData(), vertexBuffer.data(), vertexBuffer.size() * sizeof(Vertex));
		memcpy(ri->GetIndexBufferData(), indexBuffer.data(), indexBuffer.size() * sizeof(UINT));
		//save the material index for accessing the material buffer
		ri->SetMaterialIndex(to[matName]->MatCBIndex);
		//Initialize the world and texture transformation matrices to identity matrix
		XMStoreFloat4x4(&ri->GetWorldMatrix(), XMMatrixIdentity());
		XMStoreFloat4x4(&ri->GetTexTransformMatrix(), XMMatrixIdentity());
		rItems[shapes[i].name] = std::move(ri);
	}
}

