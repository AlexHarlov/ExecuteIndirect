#include "Scene.h"
#include "DirectXHelper.h"

using namespace ExecuteIndirect;

Scene::Scene()
{
	m_OccluderModelNames.push_back("terrain");
}


Scene::~Scene()
{
}

/// <summary>
/// Determines which render items are occluders
/// </summary>
/// <param name="renderItems">The render items.</param>
void Scene::SetOccluders(std::unordered_map<std::string, std::unique_ptr<RenderItem>>& renderItems) {
	for (UINT i = 0; i < m_OccluderModelNames.size(); i++) {
		renderItems[m_OccluderModelNames[i]]->SetOccluder(true);
	}
}


void ExecuteIndirect::Scene::BuildInstanceData(std::unordered_map<std::string, std::unique_ptr<RenderItem>>& renderItems)
{
	InstanceData instanceData;
	XMMATRIX scaleMatrix;
	XMMATRIX translateMatrix;
	XMMATRIX instanceWorldMatrix;
	Vertex* terrainVertices = renderItems["terrain"]->GetVertexBufferData();
	UINT terrainVerticesCount = renderItems["terrain"]->GetVertexCount();

	std::random_device rd;  //Will be used to obtain a seed for the random number engine
	std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
	std::uniform_int_distribution<> dis(0, terrainVerticesCount);	//distribution from range 0 to total vertex count for the terrain

	//generate terrain instance data 
	scaleMatrix = XMMatrixIdentity();
	translateMatrix = XMMatrixIdentity();
	instanceWorldMatrix = XMMatrixMultiply(scaleMatrix, translateMatrix);
	XMStoreFloat4x4(&instanceData.World, XMMatrixTranspose(instanceWorldMatrix));
	XMStoreFloat4x4(&instanceData.TexTransform, XMMatrixTranspose( XMLoadFloat4x4(&renderItems["terrain"]->GetTexTransformMatrix())));
	instanceData.MaterialIndex = renderItems["terrain"]->GetMaterialIndex();
	renderItems["terrain"]->GetInstances().push_back(instanceData);

	//make hills higher
	for (UINT i = 0; i < terrainVerticesCount; i++)
		terrainVertices[i].pos.y *= 2.0;
	
	//generate fir tree instance data
	scaleMatrix = XMMatrixScaling(0.05f, 0.05f, 0.05f);
	for (UINT i = 0; i < firCount; i++) {
		INT32 inx = dis(gen);
		translateMatrix = XMMatrixTranslation(terrainVertices[inx].pos.x, terrainVertices[inx].pos.y, terrainVertices[inx].pos.z);
		instanceWorldMatrix = XMMatrixMultiply(scaleMatrix, translateMatrix);
		XMStoreFloat4x4(&instanceData.TexTransform, XMMatrixTranspose(XMLoadFloat4x4(&renderItems["Branches"]->GetTexTransformMatrix())));
		instanceData.MaterialIndex = renderItems["Branches"]->GetMaterialIndex();
		XMStoreFloat4x4(&instanceData.World, XMMatrixTranspose(instanceWorldMatrix));
		renderItems["Branches"]->GetInstances().push_back(instanceData);
		XMStoreFloat4x4(&instanceData.TexTransform, XMMatrixTranspose(XMLoadFloat4x4(&renderItems["Trunk"]->GetTexTransformMatrix())));
		instanceData.MaterialIndex = renderItems["Trunk"]->GetMaterialIndex();
		renderItems["Trunk"]->GetInstances().push_back(instanceData);
	}

	//generate complex tree instance data
	scaleMatrix = XMMatrixScaling(2.0f, 2.0f, 2.0f);
	for (UINT i = 0; i < complexTreeCount; i++) {
		INT32 inx = dis(gen);
		translateMatrix = XMMatrixTranslation(terrainVertices[inx].pos.x, terrainVertices[inx].pos.y, terrainVertices[inx].pos.z);
		instanceWorldMatrix = XMMatrixMultiply(scaleMatrix, translateMatrix);
		XMStoreFloat4x4(&instanceData.TexTransform, XMMatrixTranspose(XMLoadFloat4x4(&renderItems["leaf"]->GetTexTransformMatrix())));
		instanceData.MaterialIndex = renderItems["leaf"]->GetMaterialIndex();
		XMStoreFloat4x4(&instanceData.World, XMMatrixTranspose(instanceWorldMatrix));
		renderItems["leaf"]->GetInstances().push_back(instanceData);
		XMStoreFloat4x4(&instanceData.TexTransform, XMMatrixTranspose(XMLoadFloat4x4(&renderItems["bark"]->GetTexTransformMatrix())));
		instanceData.MaterialIndex = renderItems["bark"]->GetMaterialIndex();
		renderItems["bark"]->GetInstances().push_back(instanceData);
	}

	//generate grass instance data
	scaleMatrix = XMMatrixScaling(2.0f, 2.0f, 2.0f);
	for (UINT i = 0; i < grassCount; i++) {
		INT32 inx = dis(gen);
		translateMatrix = XMMatrixTranslation(terrainVertices[inx].pos.x, terrainVertices[inx].pos.y, terrainVertices[inx].pos.z);
		instanceWorldMatrix = XMMatrixMultiply(scaleMatrix, translateMatrix);
		XMStoreFloat4x4(&instanceData.TexTransform, XMMatrixTranspose(XMLoadFloat4x4(&renderItems["grass"]->GetTexTransformMatrix())));
		instanceData.MaterialIndex = renderItems["grass"]->GetMaterialIndex();
		XMStoreFloat4x4(&instanceData.World, XMMatrixTranspose(instanceWorldMatrix));
		renderItems["grass"]->GetInstances().push_back(instanceData);
	}
	//generate stone instance data
	scaleMatrix = XMMatrixScaling(0.05f, 0.05f, 0.05f);
	for (UINT i = 0; i < stoneCount; i++) {
		INT32 inx = dis(gen);
		translateMatrix = XMMatrixTranslation(terrainVertices[inx].pos.x, terrainVertices[inx].pos.y, terrainVertices[inx].pos.z);
		instanceWorldMatrix = XMMatrixMultiply(scaleMatrix, translateMatrix);
		XMStoreFloat4x4(&instanceData.TexTransform, XMMatrixTranspose(XMLoadFloat4x4(&renderItems["Stone"]->GetTexTransformMatrix())));
		instanceData.MaterialIndex = renderItems["Stone"]->GetMaterialIndex();
		XMStoreFloat4x4(&instanceData.World, XMMatrixTranspose(instanceWorldMatrix));
		renderItems["Stone"]->GetInstances().push_back(instanceData);
	}

	//generate deer instance data
	scaleMatrix = XMMatrixScaling(3.0f, 3.0f, 3.0f);
	for (UINT i = 0; i < deerCount; i++) {
		INT32 inx = dis(gen);
		translateMatrix = XMMatrixTranslation(terrainVertices[inx].pos.x, terrainVertices[inx].pos.y, terrainVertices[inx].pos.z);
		instanceWorldMatrix = XMMatrixMultiply(scaleMatrix, translateMatrix);
		XMStoreFloat4x4(&instanceData.TexTransform, XMMatrixTranspose(XMLoadFloat4x4(&renderItems["Deer_body"]->GetTexTransformMatrix())));
		instanceData.MaterialIndex = renderItems["Deer_body"]->GetMaterialIndex();
		XMStoreFloat4x4(&instanceData.World, XMMatrixTranspose(instanceWorldMatrix));
		renderItems["Deer_body"]->GetInstances().push_back(instanceData);
		XMStoreFloat4x4(&instanceData.TexTransform, XMMatrixTranspose(XMLoadFloat4x4(&renderItems["Deer_horns"]->GetTexTransformMatrix())));
		instanceData.MaterialIndex = renderItems["Deer_horns"]->GetMaterialIndex();
		renderItems["Deer_horns"]->GetInstances().push_back(instanceData);
	}

	//generate bison instance data
	scaleMatrix = XMMatrixScaling(15.0f, 15.0f, 15.0f);
	for (UINT i = 0; i < bisonCount; i++) {
		INT32 inx = dis(gen);
		translateMatrix = XMMatrixTranslation(terrainVertices[inx].pos.x, terrainVertices[inx].pos.y+5.0f, terrainVertices[inx].pos.z);
		instanceWorldMatrix = XMMatrixMultiply(scaleMatrix, translateMatrix);
		XMStoreFloat4x4(&instanceData.TexTransform, XMMatrixTranspose(XMLoadFloat4x4(&renderItems["Bison"]->GetTexTransformMatrix())));
		instanceData.MaterialIndex = renderItems["Bison"]->GetMaterialIndex();
		XMStoreFloat4x4(&instanceData.World, XMMatrixTranspose(instanceWorldMatrix));
		renderItems["Bison"]->GetInstances().push_back(instanceData);
	}

	//generate tiger instance data
	scaleMatrix = XMMatrixScaling(0.005f, 0.005f, 0.005f);
	for (UINT i = 0; i < tigerCount; i++) {
		INT32 inx = dis(gen);
		translateMatrix = XMMatrixTranslation(terrainVertices[inx].pos.x, terrainVertices[inx].pos.y+5.0f, terrainVertices[inx].pos.z);
		instanceWorldMatrix = XMMatrixMultiply(scaleMatrix, translateMatrix);
		XMStoreFloat4x4(&instanceData.TexTransform, XMMatrixTranspose(XMLoadFloat4x4(&renderItems["Tiger"]->GetTexTransformMatrix())));
		instanceData.MaterialIndex = renderItems["Tiger"]->GetMaterialIndex();
		XMStoreFloat4x4(&instanceData.World, XMMatrixTranspose(instanceWorldMatrix));
		renderItems["Tiger"]->GetInstances().push_back(instanceData);
	}

	//generate wolf instance data
	scaleMatrix = XMMatrixScaling(5.0f, 5.0f, 5.0f);
	for (UINT i = 0; i < wolfCount; i++) {
		INT32 inx = dis(gen);
		translateMatrix = XMMatrixTranslation(terrainVertices[inx].pos.x, terrainVertices[inx].pos.y, terrainVertices[inx].pos.z);
		instanceWorldMatrix = XMMatrixMultiply(scaleMatrix, translateMatrix);
		XMStoreFloat4x4(&instanceData.TexTransform, XMMatrixTranspose(XMLoadFloat4x4(&renderItems["WolfBody"]->GetTexTransformMatrix())));
		instanceData.MaterialIndex = renderItems["WolfBody"]->GetMaterialIndex();
		XMStoreFloat4x4(&instanceData.World, XMMatrixTranspose(instanceWorldMatrix));
		renderItems["WolfBody"]->GetInstances().push_back(instanceData);
		XMStoreFloat4x4(&instanceData.TexTransform, XMMatrixTranspose(XMLoadFloat4x4(&renderItems["WolfFur"]->GetTexTransformMatrix())));
		instanceData.MaterialIndex = renderItems["WolfFur"]->GetMaterialIndex();
		renderItems["WolfFur"]->GetInstances().push_back(instanceData);
	}

	//generate house instance data
	scaleMatrix = XMMatrixScaling(5.0f, 5.0f, 5.0f);
	for (UINT i = 0; i < houseCount; i++) {
		INT32 inx = dis(gen);
		translateMatrix = XMMatrixTranslation(terrainVertices[inx].pos.x, terrainVertices[inx].pos.y, terrainVertices[inx].pos.z);
		instanceWorldMatrix = XMMatrixMultiply(scaleMatrix, translateMatrix);
		XMStoreFloat4x4(&instanceData.TexTransform, XMMatrixTranspose(XMLoadFloat4x4(&renderItems["house"]->GetTexTransformMatrix())));
		instanceData.MaterialIndex = renderItems["house"]->GetMaterialIndex();
		XMStoreFloat4x4(&instanceData.World, XMMatrixTranspose(instanceWorldMatrix));
		renderItems["house"]->GetInstances().push_back(instanceData);
	}

	//generate farmhouse instance data
	scaleMatrix = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	for (UINT i = 0; i < farmHouseCount; i++) {
		INT32 inx = dis(gen);
		translateMatrix = XMMatrixTranslation(terrainVertices[inx].pos.x, terrainVertices[inx].pos.y, terrainVertices[inx].pos.z);
		instanceWorldMatrix = XMMatrixMultiply(scaleMatrix, translateMatrix);
		XMStoreFloat4x4(&instanceData.TexTransform, XMMatrixTranspose(XMLoadFloat4x4(&renderItems["farmhouse"]->GetTexTransformMatrix())));
		instanceData.MaterialIndex = renderItems["farmhouse"]->GetMaterialIndex();
		XMStoreFloat4x4(&instanceData.World, XMMatrixTranspose(instanceWorldMatrix));
		renderItems["farmhouse"]->GetInstances().push_back(instanceData);
	}
}
