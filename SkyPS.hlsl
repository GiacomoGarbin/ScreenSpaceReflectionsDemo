TextureCube gCubeMap : register(t0);
SamplerState gSamplerState;

struct VertexOut
{
	float3 PositionL : POSITION;
	float4 PositionH : SV_POSITION;
};

float4 main(VertexOut pin) : SV_TARGET
{
	return gCubeMap.Sample(gSamplerState, pin.PositionL);
}