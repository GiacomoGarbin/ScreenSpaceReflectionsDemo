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
		XMFLOAT4X4 view;
		XMFLOAT4X4 proj;
	};

	static_assert((sizeof(ConstantBuffer) % 16) == 0, "constant buffer size must be 16-byte aligned");

	ID3D11Buffer* mConstantBuffer;

	DebugQuad mDebugQuad;

	SSSR();
	~SSSR();

	void init(ID3D11Device* device, UINT width, UINT height);
	void OnResize(ID3D11Device* device, UINT width, UINT height);

	void draw(ID3D11DeviceContext* context, const CameraObject& camera, ID3D11ShaderResourceView* NormalDepthSRV);
};

#endif SSSR_H