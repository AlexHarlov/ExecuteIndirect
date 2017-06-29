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

struct MaterialData
{
	float4   DiffuseAlbedo;
	float3   FresnelR0;
	float    Roughness;
	float4x4 MatTransform;
	uint     DiffuseMapIndex;
	uint     NormalMapIndex;
	uint     MatPad1;
	uint     MatPad2;
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
	
StructuredBuffer<MaterialData> materials : register(t0);			//Material SRV
StructuredBuffer<InstanceData> instanceData : register(t1);			//Instance Data SRV

// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
	float3 PosL    : POSITION;		//Position in local space
	float3 NormalL : NORMAL;		//Normal in local space
	float2 TexC    : TEXCOORD;		//Texture coordinates
	float3 TangentU : TANGENT;		//Tangent in local space
};

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	float4 PosH    : SV_POSITION;
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float3 TangentW : TANGENT;
	float2 TexC    : TEXCOORD;

	nointerpolation uint MatIndex  : MATINDEX;
};

/// <summary>
/// Vertex Shader main function
/// </summary>
/// <returns>Pixel Shader Input</returns>
/// <param name="input">The Vertex Shader Input</param>
/// <param name="instanceID">System value - the current instance index</param>
PixelShaderInput main(VertexShaderInput input, uint instanceID : SV_InstanceID)
{
	PixelShaderInput output;
	float4 pos = float4(input.PosL, 1.0f);
	//Load the current instance data
	InstanceData instData = instanceData[instanceID];
	float4x4 worldMatrix = instData.World;
	float4x4 textureTranform = instData.TexTransform;
	uint materialIndex = instData.materialIndex;
	//Load object's material data
	MaterialData matData = materials[materialIndex];
	output.MatIndex = materialIndex;

	// Transform to world space.
	float4 posW = mul(float4(input.PosL, 1.0f), worldMatrix);
	output.PosW = posW.xyz;
	output.NormalW = mul(input.NormalL, (float3x3)worldMatrix);
	output.TangentW = mul(input.TangentU, (float3x3)worldMatrix);

	// Transform position to view space.
	output.PosH = mul(posW, viewMatrix);
	// Transform position to homogeneous clip space.
	output.PosH = mul(output.PosH, projectionMatrix);

	// Transform texture coordinates 
	float4 texC = mul(float4(input.TexC, 0.0f, 1.0f), textureTranform);
	output.TexC = mul(texC, matData.MatTransform).xy;

	return output;
}
