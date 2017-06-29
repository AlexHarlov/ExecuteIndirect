#include "Lighting.hlsli"
#define threadBlockSize 64		//should be a multiple of WAPR size (32 threads for NVidia cards) 

cbuffer SceneConstantBuffer : register(b0)
{
	float4x4 viewMatrix;
	float4x4 projectionMatrix;
	float4 viewportSize;
	float3 eyePosW;
	float sCBPadding1;
	float4 ambientLight;
	Light lights[MaxLights];
};

struct InstanceData
{
	float4x4 World;
	float4x4 TexTransform;
	uint materialIndex;
	uint InstPad0;
	uint InstPad1;
	uint InstPad2;
};


cbuffer ComputeConstants : register(b1)			
{
	uint instanceCount;
	float3 center;
	float3 extents;
}

Texture2D<float> HizMap										: register(t0, space1);			// HiZ MIP Map Chain
SamplerState HizMapSampler									: register(s0);					// Sampler state for the HiZ Buffer
StructuredBuffer<InstanceData> inputInstanceData			: register(t0);					// Instance data SRV
AppendStructuredBuffer<InstanceData> outputInstanceData		: register(u0);					// Approved instance data UAV


/// <summary>
/// Calculates the bounding box corners from its center and extents, and transforms them to homogeneous clip space
/// </summary>
/// <param name="worldMatrix">The world matrix for this instance</param>
/// <param name="BoundingBoxCorners">The object's bounding box corners, in local space</param>
void transformCorners(in float4x4 worldMatrix, inout float4 BoundingBoxCorners[8]) {
	//fill the global array with the corners
	BoundingBoxCorners[0] = float4 (center + float3(extents.x, extents.y, extents.z), 1.0f);
	BoundingBoxCorners[1] = float4 (center + float3(-extents.x, extents.y, extents.z), 1.0f);
	BoundingBoxCorners[2] = float4 (center + float3(extents.x, -extents.y, extents.z), 1.0f);
	BoundingBoxCorners[3] = float4 (center + float3(-extents.x, -extents.y, extents.z), 1.0f);
	BoundingBoxCorners[4] = float4 (center + float3(extents.x, extents.y, -extents.z), 1.0f);
	BoundingBoxCorners[5] = float4 (center + float3(-extents.x, extents.y, -extents.z), 1.0f);
	BoundingBoxCorners[6] = float4 (center + float3(extents.x, -extents.y, -extents.z), 1.0f);
	BoundingBoxCorners[7] = float4 (center + float3(-extents.x, -extents.y, -extents.z), 1.0f);
	//multiply each of the corners by the WVP matrices
	for (int j = 0; j < 8; j++) {
		BoundingBoxCorners[j] = mul(BoundingBoxCorners[j], worldMatrix);
		BoundingBoxCorners[j] = mul(BoundingBoxCorners[j], viewMatrix);
		BoundingBoxCorners[j] = mul(BoundingBoxCorners[j], projectionMatrix);
	}
}

/// <summary>
/// Checks if current patch bounding box is inside the view frustum
/// </summary>
int frustumCull(in float4 BoundingBoxCorners[8]) {
	//helper array to determine whether all corners are outside frustum
	int outOfBound[6] = { 0, 0, 0, 0, 0, 0 };
	//iterating over all corners we check if one of them fails a condition 
	for (int i = 0; i<8; i++)
	{
		if (BoundingBoxCorners[i].x >  BoundingBoxCorners[i].w) outOfBound[0]++;
		if (BoundingBoxCorners[i].x < -BoundingBoxCorners[i].w) outOfBound[1]++;
		if (BoundingBoxCorners[i].y >  BoundingBoxCorners[i].w) outOfBound[2]++;
		if (BoundingBoxCorners[i].y < -BoundingBoxCorners[i].w) outOfBound[3]++;
		if (BoundingBoxCorners[i].z >  BoundingBoxCorners[i].w) outOfBound[4]++;
		if (BoundingBoxCorners[i].z < 0.f) outOfBound[5]++;
	}

	int inFrustum = 1;
	//if we find a condition where all corners have failed, the bounding box is outside the frustum 
	for (int j = 0; j<6; j++)
		if (outOfBound[j] == 8) inFrustum = 0;

	return inFrustum;
}

/// <summary>
/// Checks if current bounding box is occluded
/// </summary>
int occlusionCull(in float4 BoundingBoxCorners[8]) {
	int numSamples = 4; //TODO: make this a root constant 
	//Transform each corner to NDC by a perspective divide
	for (int i = 0; i < 8; i++)
		BoundingBoxCorners[i].xyz /= BoundingBoxCorners[i].w;
	//Calculate the bounding rectangle of our bounding box in NDC
	float2 BoundingRect[2];
	//find the min x and y - they will be top left corner
	BoundingRect[0].x = min(min(min(BoundingBoxCorners[0].x, BoundingBoxCorners[1].x),
		min(BoundingBoxCorners[2].x, BoundingBoxCorners[3].x)),
		min(min(BoundingBoxCorners[4].x, BoundingBoxCorners[5].x),
			min(BoundingBoxCorners[6].x, BoundingBoxCorners[7].x))) / 2.0 + 0.5;

	BoundingRect[0].y = min(min(min(BoundingBoxCorners[0].y, BoundingBoxCorners[1].y),
		min(BoundingBoxCorners[2].y, BoundingBoxCorners[3].y)),
		min(min(BoundingBoxCorners[4].y, BoundingBoxCorners[5].y),
			min(BoundingBoxCorners[6].y, BoundingBoxCorners[7].y))) / 2.0 + 0.5;
	//find the min x and y - they will be bottom right corner
	BoundingRect[1].x = max(max(max(BoundingBoxCorners[0].x, BoundingBoxCorners[1].x),
		max(BoundingBoxCorners[2].x, BoundingBoxCorners[3].x)),
		max(max(BoundingBoxCorners[4].x, BoundingBoxCorners[5].x),
			max(BoundingBoxCorners[6].x, BoundingBoxCorners[7].x))) / 2.0 + 0.5;

	BoundingRect[1].y = max(max(max(BoundingBoxCorners[0].y, BoundingBoxCorners[1].y),
		max(BoundingBoxCorners[2].y, BoundingBoxCorners[3].y)),
		max(max(BoundingBoxCorners[4].y, BoundingBoxCorners[5].y),
			max(BoundingBoxCorners[6].y, BoundingBoxCorners[7].y))) / 2.0 + 0.5;

	//find the minimum z value - foremost point of the bounding box
	float InstanceDepth = min(min(min(BoundingBoxCorners[0].z, BoundingBoxCorners[1].z),
		min(BoundingBoxCorners[2].z, BoundingBoxCorners[3].z)),
		min(min(BoundingBoxCorners[4].z, BoundingBoxCorners[5].z),
			min(BoundingBoxCorners[6].z, BoundingBoxCorners[7].z)));

	//find bounding rectangle width and height (in NDC)
	float width = (BoundingRect[1].x - BoundingRect[0].x);
	float height = (BoundingRect[1].y - BoundingRect[0].y);

	//find the bigger size in viewport coordinates and divide it by the number of samples
	const	float	maxSize = max(width * viewportSize.x, height * viewportSize.y) / numSamples;
	//find which mipmap level we should check for occlusion
	const	float	mip = max(0, ceil(log2(maxSize)));
	//prepare an offset value to make sure each iteration will go over all covered by the rectangle pixels
	const	float2	bOffset = 0.5f / viewportSize.xy;
	//get the begining y coordinate
	float yPos = 1.0f - BoundingRect[1].y;
	//calculate each step in x and y direction
	const float stepX = width / (numSamples + 1);
	const float stepY = height / (numSamples + 1);
	float MaxDepth = 0;
	//iterate over rectangles pixels
	for (int y = 0; y < numSamples + 1; ++y)
	{
		//get the start x position for this row (y)
		float xPos = BoundingRect[0].x;
		for (int x = 0; x < numSamples + 1; ++x)
		{
			//get coordinates to sample from the Hiz Buffer 
			const float2 sampleCoords = float2(xPos, yPos) + bOffset;
			//get sampled depth
			float sampledDepth = HizMap.SampleLevel(HizMapSampler, sampleCoords, mip);
			//reassign max depth in case current sampled is bigger 
			MaxDepth = max(MaxDepth, sampledDepth);
			xPos += stepX;
		}
		yPos += stepY;
	}
	//if foremost point is closer to us, then the patch is visible
	if (InstanceDepth < MaxDepth)
		return 1;
	return 0;
}

/// <summary>
/// Compute shader's main function
/// </summary>
[numthreads(threadBlockSize, 1, 1)]
void main(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
	
	// Each thread of the CS operates on one of the indirect commands.
	uint index = (groupId.x * threadBlockSize) + groupIndex;
	if (index >= instanceCount)
		return;
	
	float4 BoundingBoxCorners[8];
	float4x4 worldMatrix = inputInstanceData[index].World;
	//transform the current bounding box to homogenous clip space coordinates
	transformCorners(worldMatrix, BoundingBoxCorners);
	//execute frustum culling
	int visible = frustumCull(BoundingBoxCorners);
	if (visible == 0)
		return;

	//execute occlusion culling
	visible = occlusionCull(BoundingBoxCorners);
	if(visible == 1)
		outputInstanceData.Append(inputInstanceData[index]);
	
}