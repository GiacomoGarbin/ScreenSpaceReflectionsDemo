// http://remi-genin.fr/blog/screen-space-plane-indexed-reflection-in-ghost-recon-wildlands/

RWTexture2D<float4> gReflectionsMap : register(u0);

#define ThreadGroupSize 256

[numthreads(ThreadGroupSize, 1, 1)]
void main(int3 GroupThreadID : SV_GroupThreadID, int3 DispatchThreadID : SV_DispatchThreadID)
{


	gReflectionsMap[DispatchThreadID.xy] = 0;
}