struct VertexOut
{
	float3 PositionW : POSITION;
	float4 PositionH : SV_POSITION;
	float3 NormalW   : NORMAL;
	float3 TangentW  : TANGENT;
	float2 TexCoord  : TEXCOORD;
	float TessFactor : TESSFACTOR;
};

struct PatchTess
{
	float EdgeTess[3] : SV_TessFactor;
	float InsideTess : SV_InsideTessFactor;
};

PatchTess PatchHS(InputPatch<VertexOut, 3> patch, uint PatchID : SV_PrimitiveID)
{
	PatchTess p;

	p.EdgeTess[0] = 0.5f * (patch[1].TessFactor + patch[2].TessFactor);
	p.EdgeTess[1] = 0.5f * (patch[2].TessFactor + patch[0].TessFactor);
	p.EdgeTess[2] = 0.5f * (patch[0].TessFactor + patch[1].TessFactor);
	p.InsideTess = p.EdgeTess[0];

	return p;
}

struct HullOut
{
	float3 PositionW : POSITION;
	float3 NormalW   : NORMAL;
	float3 TangentW  : TANGENT;
	float2 TexCoord  : TEXCOORD;
};

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("PatchHS")]
HullOut main(InputPatch<VertexOut, 3> p, uint i : SV_OutputControlPointID, uint PatchId : SV_PrimitiveID)
{
	HullOut hout;

	hout.PositionW = p[i].PositionW;
	hout.NormalW = p[i].NormalW;
	hout.TangentW = p[i].TangentW;
	hout.TexCoord = p[i].TexCoord;

	return hout;
}