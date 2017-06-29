#include "Lighting.hlsli"
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

StructuredBuffer<InstanceData> instanceData : register(t0);
// Per-vertex data used as input to the vertex shader.
struct VS_IN
{
	float3 PosL    : POSITION;			//Vertex position in local space
	float3 NormalL : NORMAL;			//Normal in local space
	float2 TexC    : TEXCOORD;			//Texture coordinates
	float3 TangentU : TANGENT;			//Tangent in local space
};

// Per-pixel color data passed through the pixel shader.
struct PS_IN
{	
	float4 pos : SV_POSITION;			//Vertex Position in homogeneous clip space
	float4 color : COLOR0;				//Vertex color
};

/// <summary>
/// Occluder drawing - Vertex Shader main function
/// </summary>
/// <returns>Pixel Shader Input</returns>
/// <param name="input">The Vertex Shader Input</param>
/// <param name="instanceID">System value - the current instance index</param>
PS_IN main(VS_IN input, uint instanceID : SV_InstanceID)
{
	PS_IN output;
	//Load the instance data
	InstanceData instData = instanceData[instanceID];
	float4x4 worldMatrix = instData.World;

	float4 pos = float4(input.PosL, 1.0f);
	// Transform the position to world space.
	pos = mul(pos, worldMatrix);
	// Transform the position to view space.
	pos = mul(pos, viewMatrix);
	// Transform the position to homogeneous space.
	pos = mul(pos, projectionMatrix);
	output.pos = pos;
	// Draw always black, we need this drawing only to use it's depth buffer afterwards
	output.color = float4(0.0, 0.0, 0.0, 1.0);
	return output;
}