struct VertexIn
{
	float3 PositionL : POSITION;
	float2 TexCoord  : TEXCOORD;
};

struct VertexOut
{
	float4 PositionH : SV_POSITION;
	float2 TexCoord  : TEXCOORD;
};

VertexOut main(VertexIn vin)
{
	VertexOut vout;

	// already in NDC space
	vout.PositionH = float4(vin.PositionL, 1.0f);
	vout.TexCoord = vin.TexCoord;

	return vout;
}