#ifndef SSR_H
#define SSR_H

#include <D3DApp.h>

class SSR
{
public:
	UINT mWidth;
	UINT mHeight;

	D3D11_VIEWPORT mReflectionsMapViewport;
	ID3D11RenderTargetView* mReflectionsMapRTV;
	ID3D11ShaderResourceView* mReflectionsMapSRV;
	//ID3D11VertexShader* mVertexShader;
	//ID3D11InputLayout* mInputLayout;
	ID3D11PixelShader* mPixelShader;
	ID3D11SamplerState* mSamplerState; // normal depth map

	struct ConstantBuffer
	{
		XMFLOAT4 FrustumFarCorner[4];
		XMFLOAT4X4 proj;

		XMFLOAT4X4 view;
		XMFLOAT4X4 ViewInverse;
		XMFLOAT4X4 reflect;

		XMFLOAT4X4 ViewProj;
		XMFLOAT4X4 ViewProjInverse;

		XMFLOAT4X4 ProjInverse;

		XMFLOAT3 CameraPosition;
		
		float padding;
	};

	static_assert((sizeof(ConstantBuffer) % 16) == 0, "constant buffer size must be 16-byte aligned");

	XMFLOAT4 mFrustumFarCorner[4];
	ID3D11Buffer* mConstantBuffer;

	GameObject mReflectionsMapQuad;
	DebugQuad mDebugQuad;

	SSR();
	~SSR();

	void Init(ID3D11Device* device, UINT width, UINT height, float FieldOfViewY, float NearZ, float FarZ);
	void OnResize(ID3D11Device* device, UINT width, UINT height, float FieldOfViewY, float NearZ, float FarZ);

	void ComputeReflectionsMap(ID3D11DeviceContext* context, const CameraObject& camera, ID3D11ShaderResourceView* NormalDepthSRV);

	// hierarchical depth buffer

	ID3D11ComputeShader* mCopyDepthBufferCS;
	//ID3D11UnorderedAccessView* mCopyDepthBufferUAV;
	//ID3D11ShaderResourceView* mCopyDepthBufferSRV;

	//ID3D11ComputeShader* mHierarchicalDepthBufferCS;

	//ID3D11VertexShader* mHierarchicalDepthBufferVS;
	//ID3D11InputLayout* mHierarchicalDepthBufferIL;
	//ID3D11PixelShader* mHierarchicalDepthBufferPS;

	DebugQuad mHierarchicalDepthBufferQuad;

	//ID3D11RenderTargetView* mPingPongTextureRTV[2];
	//ID3D11ShaderResourceView* mPingPongTextureSRV[2];
	//ID3D11UnorderedAccessView* mPingPongTextureUAV[2];

	ID3D11SamplerState* mHierarchicalDepthBufferSS;

	UINT mMipLevels;

	//std::vector<ID3D11RenderTargetView*> mHierarchicalDepthBufferRTV;
	ID3D11ShaderResourceView* mHierarchicalDepthBufferSRV;

	void ComputeHierarchicalDepthBuffer(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11ShaderResourceView* NormalDepthSRV);
};

#endif // SSR_H