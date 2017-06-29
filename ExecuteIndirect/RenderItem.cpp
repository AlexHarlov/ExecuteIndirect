#include "pch.h"
#include "RenderItem.h"
#include "DirectXHelper.h"

using namespace ExecuteIndirect;

RenderItem::RenderItem(int vertexCount , int indexCount) : 
	VertexCount(vertexCount), 
	IndexCount(indexCount)
{
	VertexBufferCPU = new Vertex[VertexCount];
	IndexBufferCPU = new UINT[IndexCount];

	VertexByteStride = sizeof(Vertex);
	VertexBufferByteSize = vertexCount*sizeof(Vertex);
	IndexBufferByteSize = indexCount*sizeof(UINT);

	TriangleCount = indexCount / 3;
}

/// <summary>
/// Creates the UAV counter offset.
/// </summary>
void RenderItem::CreateCounterOffset() {
	//UAV buffer is 4096 byte aligned
	InstanceBufferCounterOffset = DX::AlignForUavCounter(Instances.size()*sizeof(InstanceData));
}

RenderItem::~RenderItem() {
	ReleaseCPUBuffers();

}

/// <summary>
/// Free the unused arrays (note: GPU resources based on these arrays don't need them to be live,
/// if the GPU resource is in a default buffer
/// </summary>
void RenderItem::ReleaseCPUBuffers() {
	if (VertexBufferCPU)
		delete[] VertexBufferCPU;
	if (IndexBufferCPU)
		delete[] IndexBufferCPU;

}

/// <summary>
/// Creates the render item bounding box.
/// </summary>
void RenderItem::CreateBoundingBox()
{
	BoundingBox::CreateFromPoints(boundingBox, VertexCount, &VertexBufferCPU[0].pos, sizeof(Vertex));
}
