Texture2D<float4> gDepthInput : register(t0);
RWTexture2D<float> gDepthOutput : register(u0);

#define ThreadGroupSize 256

[numthreads(ThreadGroupSize, 1, 1)]
void main(uint3 GroupThreadID : SV_GroupThreadID, uint3 DispatchThreadID : SV_DispatchThreadID)
{
	gDepthOutput[DispatchThreadID.xy] = gDepthInput[DispatchThreadID.xy].w;
}