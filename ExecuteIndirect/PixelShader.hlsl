
// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

#include "Lighting.hlsli"

//static samplers
SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

//The scene constant buffer 
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

Texture2D diffuseMaps[15] : register(t0, space1);
Texture2D normalMaps[15] : register(t0, space2);
StructuredBuffer<MaterialData> materials : register(t0);

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	float4 PosH    : SV_POSITION;		//Position in homogeneous clip space
	float3 PosW    : POSITION;			//Position in world space
	float3 NormalW : NORMAL;			//Normal in world space
	float3 TangentW : TANGENT;			//Tangent in world space
	float2 TexC    : TEXCOORD;			//Texture coordinates
	//non interpolated between pixels value - the material index
	nointerpolation uint MatIndex  : MATINDEX;
};

/// <summary>
/// Transforms normal map sample to world space
/// </summary>
/// <returns>The sample in world space</returns>
/// <param name="normalMapSample">The sample from the map</param>
/// <param name="unitNormalW">The vertex normal in world space</param>
/// <param name="tangentW">The vertex tangent in world space</param>
float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
	// Uncompress each component from [0,1] to [-1,1].
	float3 normalT = 2.0f*normalMapSample - 1.0f;

	// Build orthonormal basis.
	float3 N = unitNormalW;
	float3 T = normalize(tangentW - dot(tangentW, N)*N);
	float3 B = cross(N, T);

	float3x3 TBN = float3x3(T, B, N);

	// Transform from tangent space to world space.
	float3 bumpedNormalW = mul(normalT, TBN);

	return bumpedNormalW;
}

/// <summary>
/// Pixel Shader main function
/// </summary>
/// <returns>Pixel Color</returns>
/// <param name="input">The Pixel Shader Input</param>
float4 main(PixelShaderInput input) : SV_TARGET
{

	// Load the material data.
	MaterialData matData = materials[input.MatIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
	float3 fresnelR0 = matData.FresnelR0;
	float  roughness = matData.Roughness;
	uint diffuseMapIndex = matData.DiffuseMapIndex;
	uint normalMapIndex = matData.NormalMapIndex;
	float4 diffuseColor = diffuseMaps[diffuseMapIndex].Sample(gsamAnisotropicWrap, input.TexC);
	//clip nearly transperant pixels
	clip(diffuseColor.a - 0.1f);

	// Interpolating normal can unnormalize it, so renormalize it.
	input.NormalW = normalize(input.NormalW);
	// Sample the normal map of this material and transform it to world space
	float4 normalMapSample = normalMaps[normalMapIndex].Sample(gsamAnisotropicWrap, input.TexC);
	float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, input.NormalW, input.TangentW);

	// Dynamically look up the texture in the array.
	diffuseAlbedo *= diffuseColor;

	// Vector from point being lit to eye. 
	float3 toEyeW = normalize(eyePosW - input.PosW);

	// Calculate ambient and shininess.
	float4 ambient = ambientLight*diffuseAlbedo;
	const float shininess = (1.0f - roughness) * normalMapSample.a;
	Material mat = { diffuseAlbedo, fresnelR0, shininess };
	float3 shadowFactor = 1.0f;
	//Calculate direct lighting
	float4 directLight = ComputeLighting(lights, mat, input.PosW,
		bumpedNormalW, toEyeW, shadowFactor);

	float4 litColor = ambient + directLight;
	// Add in specular reflections.
	float3 r = reflect(-toEyeW, bumpedNormalW);
	float3 fresnelFactor = SchlickFresnel(fresnelR0, bumpedNormalW, r);
	litColor.rgb += shininess * fresnelFactor;
	// Common convention to take alpha from diffuse albedo.
	litColor.a = diffuseAlbedo.a;
	return litColor;
}
