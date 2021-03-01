RWTexture2D<float4> gOutputTexture : register(u0);

#define ThreadGroupSize 256

[numthreads(ThreadGroupSize, 1, 1)]
void main(int3 GroupThreadID : SV_GroupThreadID, int3 DispatchThreadID : SV_DispatchThreadID)
{
	float2 size;
	gOutputTexture.GetDimensions(size.x, size.y);

	gOutputTexture[DispatchThreadID.xy] = float4(1 - DispatchThreadID.xy / size, 0, 1);
}