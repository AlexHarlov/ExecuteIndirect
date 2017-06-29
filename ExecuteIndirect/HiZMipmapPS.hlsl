Texture2D<float> LastMip : register(t0);  //The last mip-map level SRV

/// <summary>
/// Mip map level generation - Pixel Shader main function
/// </summary>
/// <returns>Maximum depth</returns>
/// <param name="input">The Pixel position</param>
float4 main(float4 PositionSS : SV_POSITION) : SV_TARGET
{
	// get integer pixel coordinates
	uint3 nCoords = uint3(PositionSS.xy, 0);

	// fetch a 2x2 neighborhood and compute the max
	nCoords.xy *= 2;

	float4 vTexels;
	vTexels.x = LastMip.Load(nCoords);
	vTexels.y = LastMip.Load(nCoords, uint2(1, 0));
	vTexels.z = LastMip.Load(nCoords, uint2(0, 1));
	vTexels.w = LastMip.Load(nCoords, uint2(1, 1));

	// Determine the largest depth value and use it as the new down sampled
	// depth.
	float fMaxDepth = max(max(vTexels.x, vTexels.y), max(vTexels.z, vTexels.w));
	return fMaxDepth;
}
