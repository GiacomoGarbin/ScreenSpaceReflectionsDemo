#ifndef SSSR_H
#define SSSR_H

#include <D3DApp.h>

class SSSR
{
public:
	UINT mWidth;
	UINT mHeight;

	ID3D11ComputeShader* mComputeShader;

	ID3D11UnorderedAccessView* mUAV;
	ID3D11ShaderResourceView* mSRV;

	struct ConstantBuffer
	{
		XMFLOAT4X4 ViewInverse;
		XMFLOAT4X4 proj;
		XMFLOAT4X4 ProjInverse;
		XMFLOAT4X4 ViewProjInverse;

		UINT FrameIndex;

		XMFLOAT3 padding;
	};

	static_assert((sizeof(ConstantBuffer) % 16) == 0, "constant buffer size must be 16-byte aligned");

	ID3D11Buffer* mConstantBuffer;

	ID3D11Buffer* mSobolBuffer;
	ID3D11Buffer* mRankingTileBuffer;
	ID3D11Buffer* mScramblingTileBuffer;

	ID3D11ShaderResourceView* mSobolSRV;
	ID3D11ShaderResourceView* mRankingTileSRV;
	ID3D11ShaderResourceView* mScramblingTileSRV;

	DebugQuad mDebugQuad;

	SSSR();
	~SSSR();

	void init(ID3D11Device* device, UINT width, UINT height);
	void OnResize(ID3D11Device* device, UINT width, UINT height);

	void draw(ID3D11DeviceContext* context, const CameraObject& camera, ID3D11ShaderResourceView* LightPass, ID3D11ShaderResourceView* DepthBufferHierarchy, ID3D11ShaderResourceView* normals, UINT FrameIndex);
};

#endif // SSSR_H