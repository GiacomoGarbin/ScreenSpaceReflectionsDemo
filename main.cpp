#include <D3DApp.h>

#include <cassert>
#include <string>
#include <sstream>

#include "SSR.h"
#include "SSPR.h"

#include "SSSR.h"

class TestApp : public D3DApp
{
public:
	TestApp();
	~TestApp();

	bool Init() override;
	void OnResize(GLFWwindow* window, int width, int height) override;
	void UpdateScene(float dt) override;
	void DrawScene() override;

	struct PerFrameCB
	{
		std::array<LightDirectional, 3> mLights;

		XMFLOAT3 mEyePositionW;
		float    pad1;

		float    mFogStart;
		float    mFogRange;
		XMFLOAT2 pad2;
		XMFLOAT4 mFogColor;

		float mHeightScale;
		float mMaxTessDistance;
		float mMinTessDistance;
		float mMinTessFactor;
		float mMaxTessFactor;
		XMFLOAT3 pad3;

		XMFLOAT4X4 mViewProj;
	};

	static_assert((sizeof(PerFrameCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

	ID3D11Buffer* mPerFrameCB;

	struct PerObjectCB
	{
		XMFLOAT4X4 mWorld;
		XMFLOAT4X4 mWorldInverseTranspose;
		XMFLOAT4X4 mWorldViewProj;
		Material mMaterial;
		XMFLOAT4X4 mTexCoordTransform;
		XMFLOAT4X4 mShadowTransform;
		XMFLOAT4X4 mWorldViewProjTexture;
	};

	static_assert((sizeof(PerObjectCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

	ID3D11Buffer* mPerObjectCB;

	struct PerSkinnedCB
	{
		XMFLOAT4X4 mBoneTransforms[96];
	};

	static_assert((sizeof(PerSkinnedCB) % 16) == 0, "constant buffer size must be 16-byte aligned");

	ID3D11Buffer* mPerSkinnedCB;

	GameObject mSkull;
	GameObject mBox;
	GameObject mGrid;
	GameObject mSphere;
	GameObject mCylinder;
	GameObject mSky;

	//GameObject mTree;
	//GameObject mBase;
	//GameObject mStairs;
	//GameObject mPillar1;
	//GameObject mPillar2;
	//GameObject mPillar3;
	//GameObject mPillar4;
	//GameObject mRock;

	GameObject mCharacter;
	GameObjectInstance mCharacterInstance1;
	GameObjectInstance mCharacterInstance2;

	std::vector<GameObjectInstance*> mObjectInstances;

	std::array<LightDirectional, 3> mLights;
	XMFLOAT3 mLightsCache[3];
	float mLightAngle;

	ID3D11SamplerState* mSamplerState;

	BoundingSphere mSceneBounds;

	ShadowMap mShadowMap;
	SSAO mSSAO;

	void DrawSceneToShadowMap();
	void DrawSceneToSSAONormalDepthMap();

	SSR mSSR;
	SSPR mSSPR;
	bool mEnableSSPR;

	SSSR mSSSR;

	//void DrawSceneToReflectionsMap();

	ID3D11RenderTargetView* mSceneAlbedoRTV;
	ID3D11ShaderResourceView* mSceneAlbedoSRV;
	DebugQuad mDebugQuad;
};

TestApp::TestApp() :
	D3DApp(),
	mPerFrameCB(nullptr),
	mPerObjectCB(nullptr),
	mPerSkinnedCB(nullptr),
	mSamplerState(nullptr),

	mSceneAlbedoRTV(nullptr),
	mSceneAlbedoSRV(nullptr),

	mEnableSSPR(false)
{
	mMainWindowTitle = "SSR demo";

	//m4xMSAAEnabled = true;

	mLights[0].mAmbient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mLights[0].mDiffuse = XMFLOAT4(1.0f, 0.9f, 0.9f, 1.0f);
	mLights[0].mSpecular = XMFLOAT4(0.8f, 0.8f, 0.7f, 1.0f);
	mLights[0].mDirection = XMFLOAT3(-0.57735f, -0.57735f, 0.57735f);

	mLights[1].mAmbient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mLights[1].mDiffuse = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);
	mLights[1].mSpecular = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[1].mDirection = XMFLOAT3(0.707f, -0.707f, 0.0f);

	mLights[2].mAmbient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mLights[2].mDiffuse = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);
	mLights[2].mSpecular = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mLights[2].mDirection = XMFLOAT3(0.0f, 0.0, -1.0f);

	for (UINT i = 0; i < 3; ++i)
	{
		mLightsCache[i] = mLights[i].mDirection;
	}

	mCamera.mPosition = XMFLOAT3(0, 2, -15);

	mSceneBounds.Center = XMFLOAT3(0, 0, 0);
	mSceneBounds.Radius = std::sqrt(10 * 10 + 15 * 15);
}

TestApp::~TestApp()
{
	SafeRelease(mPerFrameCB);
	SafeRelease(mPerObjectCB);
	SafeRelease(mPerSkinnedCB);
	SafeRelease(mSamplerState);

	SafeRelease(mSceneAlbedoRTV);
	SafeRelease(mSceneAlbedoSRV);
}

bool TestApp::Init()
{
	if (!D3DApp::Init())
	{
		return false;
	}

	std::wstring base = L"";
	std::wstring proj = L"";

	// VS
	{
		Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
		std::wstring path = base + proj + L"VS.hlsl";

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
		HR(mDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

		mSkull.mVertexShader = shader;
		mGrid.mVertexShader = shader;
		mBox.mVertexShader = shader;
		mCylinder.mVertexShader = shader;
		mSphere.mVertexShader = shader;

		//mTree.mVertexShader = shader;
		//mBase.mVertexShader = shader;
		//mStairs.mVertexShader = shader;
		//mPillar1.mVertexShader = shader;
		//mPillar2.mVertexShader = shader;
		//mPillar3.mVertexShader = shader;
		//mPillar4.mVertexShader = shader;
		//mRock.mVertexShader = shader;

		// input layout
		{
			std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
			};

			Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;

			HR(mDevice->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &layout));

			mSkull.mInputLayout = layout;
			mGrid.mInputLayout = layout;
			mBox.mInputLayout = layout;
			mCylinder.mInputLayout = layout;
			mSphere.mInputLayout = layout;

			//mTree.mInputLayout = layout;
			//mBase.mInputLayout = layout;
			//mStairs.mInputLayout = layout;
			//mPillar1.mInputLayout = layout;
			//mPillar2.mInputLayout = layout;
			//mPillar3.mInputLayout = layout;
			//mPillar4.mInputLayout = layout;
			//mRock.mInputLayout = layout;
		}
	}

	// HS
	{
		Microsoft::WRL::ComPtr<ID3D11HullShader> shader;
		std::wstring path = base + proj + L"HS.hlsl";

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "hs_5_0", 0, 0, &pCode, nullptr));
		HR(mDevice->CreateHullShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

		mGrid.mHullShader = shader;
		mBox.mHullShader = shader;
		mCylinder.mHullShader = shader;
	}

	// DS
	{
		Microsoft::WRL::ComPtr<ID3D11DomainShader> shader;
		std::wstring path = base + proj + L"DS.hlsl";

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "ds_5_0", 0, 0, &pCode, nullptr));
		HR(mDevice->CreateDomainShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

		mGrid.mDomainShader = shader;
		mBox.mDomainShader = shader;
		mCylinder.mDomainShader = shader;
	}

	// PS
	{
		Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
		std::wstring path = base + proj + L"PS.hlsl";

		std::vector<D3D_SHADER_MACRO> defines;
		defines.push_back({ "ENABLE_TEXTURE",         "1" });
		defines.push_back({ "ENABLE_SPHERE_TEXCOORD", "0" });
		defines.push_back({ "ENABLE_NORMAL_MAPPING",  "1" });
		defines.push_back({ "ENABLE_ALPHA_CLIPPING",  "0" });
		//defines.push_back({ "ENABLE_LIGHTING",       "1" });
		defines.push_back({ "ENABLE_REFLECTION",      "0" });
		defines.push_back({ "ENABLE_SSR",             "0" });
		defines.push_back({ "ENABLE_FOG",             "0" });
		defines.push_back({ nullptr, nullptr });

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
		HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &shader));

		mGrid.mPixelShader = shader;
		mBox.mPixelShader = shader;
		mCylinder.mPixelShader = shader;

		//mBase.mPixelShader = shader;
		//mStairs.mPixelShader = shader;
		//mPillar1.mPixelShader = shader;
		//mPillar2.mPixelShader = shader;
		//mPillar3.mPixelShader = shader;
		//mPillar4.mPixelShader = shader;
		//mRock.mPixelShader = shader;

		mCharacter.mPixelShader = shader;
	}

	// build per frame costant buffer
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(PerFrameCB);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		HR(mDevice->CreateBuffer(&desc, nullptr, &mPerFrameCB));
	}

	// build per object constant buffer
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(PerObjectCB);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		HR(mDevice->CreateBuffer(&desc, nullptr, &mPerObjectCB));
	}

	// build per skinned constant buffer
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(PerSkinnedCB);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		HR(mDevice->CreateBuffer(&desc, nullptr, &mPerSkinnedCB));
	}

	// build skull geometry
	{
		GeometryGenerator::CreateSkull(mSkull.mMesh);

		mSkull.mWorld = XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(0.0f, 1.0f, 0.0f);

		mSkull.mMaterial.mAmbient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
		mSkull.mMaterial.mDiffuse = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
		mSkull.mMaterial.mSpecular = XMFLOAT4(0.5f, 0.5f, 0.5f, 16.0f);
		mSkull.mMaterial.mReflect = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);

		// PS
		{
			std::wstring path = base + proj + L"PS.hlsl";

			std::vector<D3D_SHADER_MACRO> defines;
			defines.push_back({ "ENABLE_TEXTURE",         "0" });
			defines.push_back({ "ENABLE_SPHERE_TEXCOORD", "0" });
			defines.push_back({ "ENABLE_NORMAL_MAPPING",  "0" });
			defines.push_back({ "ENABLE_ALPHA_CLIPPING",  "0" });
			//defines.push_back({ "ENABLE_LIGHTING",       "1" });
			defines.push_back({ "ENABLE_REFLECTION",      "1" });
			defines.push_back({ "ENABLE_FOG",             "0" });
			defines.push_back({ nullptr, nullptr });

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mSkull.mPixelShader));
		}

		// animation keyframes
		{
			XMVECTOR Q0 = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMConvertToRadians(+30.0f));
			XMVECTOR Q1 = XMQuaternionRotationAxis(XMVectorSet(1.0f, 1.0f, 2.0f, 0.0f), XMConvertToRadians(+45.0f));
			XMVECTOR Q2 = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMConvertToRadians(-30.0f));
			XMVECTOR Q3 = XMQuaternionRotationAxis(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMConvertToRadians(+70.0f));

			mSkull.mAnimation.keyframes.resize(5);

			mSkull.mAnimation.keyframes[0].time = 0.0f;
			mSkull.mAnimation.keyframes[0].translation = XMFLOAT3(-7.0f, 0.0f, 0.0f);
			mSkull.mAnimation.keyframes[0].scale = XMFLOAT3(0.25f, 0.25f, 0.25f);
			XMStoreFloat4(&mSkull.mAnimation.keyframes[0].rotation, Q0);

			mSkull.mAnimation.keyframes[1].time = 2.0f;
			mSkull.mAnimation.keyframes[1].translation = XMFLOAT3(0.0f, 2.0f, 10.0f);
			mSkull.mAnimation.keyframes[1].scale = XMFLOAT3(0.5f, 0.5f, 0.5f);
			XMStoreFloat4(&mSkull.mAnimation.keyframes[1].rotation, Q1);

			mSkull.mAnimation.keyframes[2].time = 4.0f;
			mSkull.mAnimation.keyframes[2].translation = XMFLOAT3(7.0f, 0.0f, 0.0f);
			mSkull.mAnimation.keyframes[2].scale = XMFLOAT3(0.25f, 0.25f, 0.25f);
			XMStoreFloat4(&mSkull.mAnimation.keyframes[2].rotation, Q2);

			mSkull.mAnimation.keyframes[3].time = 6.0f;
			mSkull.mAnimation.keyframes[3].translation = XMFLOAT3(0.0f, 1.0f, -10.0f);
			mSkull.mAnimation.keyframes[3].scale = XMFLOAT3(0.5f, 0.5f, 0.5f);
			XMStoreFloat4(&mSkull.mAnimation.keyframes[3].rotation, Q3);

			mSkull.mAnimation.keyframes[4].time = 8.0f;
			mSkull.mAnimation.keyframes[4].translation = XMFLOAT3(-7.0f, 0.0f, 0.0f);
			mSkull.mAnimation.keyframes[4].scale = XMFLOAT3(0.25f, 0.25f, 0.25f);
			XMStoreFloat4(&mSkull.mAnimation.keyframes[4].rotation, Q0);
		}
	}

	// build grid geometry
	{
		GeometryGenerator::CreateGrid(20, 30, 50, 40, mGrid.mMesh);

		mGrid.mTexCoordTransform = XMMatrixScaling(8, 10, 1);

		mGrid.mMaterial.mAmbient = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
		mGrid.mMaterial.mDiffuse = XMFLOAT4(0.6f, 0.6f, 0.6f, 1.0f);
		mGrid.mMaterial.mSpecular = XMFLOAT4(0.4f, 0.4f, 0.4f, 16.0f);
		mGrid.mMaterial.mReflect = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

		//mGrid.mPrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;

		mGrid.mAlbedoSRV = mTextureManager.CreateSRV(L"floor.dds");
		mGrid.mNormalSRV = mTextureManager.CreateSRV(L"floor_nmap.dds");
	}

	// build box geometry
	{
		GeometryGenerator::CreateBox(1, 1, 1, mBox.mMesh);

		mBox.mWorld = XMMatrixScaling(3.0f, 1.0f, 3.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f);
		mBox.mTexCoordTransform = XMMatrixScaling(2, 1, 1);

		mBox.mMaterial.mAmbient = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
		mBox.mMaterial.mDiffuse = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);
		mBox.mMaterial.mSpecular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
		mBox.mMaterial.mReflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		//mBox.mPrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;

		mBox.mAlbedoSRV = mTextureManager.CreateSRV(L"bricks.dds");
		mBox.mNormalSRV = mTextureManager.CreateSRV(L"bricks_nmap.dds");
	}

	// build cylinder geometry
	{
		GeometryGenerator::CreateCylinder(0.5f, 0.5f, 3, 15, 15, mCylinder.mMesh);

		mCylinder.mTexCoordTransform = XMMatrixScaling(1, 2, 1);

		mCylinder.mMaterial.mAmbient = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
		mCylinder.mMaterial.mDiffuse = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);
		mCylinder.mMaterial.mSpecular = XMFLOAT4(1.0f, 1.0f, 1.0f, 32.0f);
		mCylinder.mMaterial.mReflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		//mCylinder.mPrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;

		mCylinder.mAlbedoSRV = mTextureManager.CreateSRV(L"bricks.dds");
		mCylinder.mNormalSRV = mTextureManager.CreateSRV(L"bricks_nmap.dds");
	}

	// build sphere geometry
	{
		GeometryGenerator::CreateSphere(0.5f, 3, mSphere.mMesh);

		mSphere.mMaterial.mAmbient = XMFLOAT4(0.3f, 0.4f, 0.5f, 1.0f);
		mSphere.mMaterial.mDiffuse = XMFLOAT4(0.2f, 0.3f, 0.4f, 1.0f);
		mSphere.mMaterial.mSpecular = XMFLOAT4(0.9f, 0.9f, 0.9f, 16.0f);
		mSphere.mMaterial.mReflect = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);

		// PS
		{
			std::wstring path = base + proj + L"PS.hlsl";

			std::vector<D3D_SHADER_MACRO> defines;
			defines.push_back({ "ENABLE_TEXTURE",         "0" });
			defines.push_back({ "ENABLE_SPHERE_TEXCOORD", "1" });
			defines.push_back({ "ENABLE_NORMAL_MAPPING",  "0" });
			defines.push_back({ "ENABLE_ALPHA_CLIPPING",  "0" });
			//defines.push_back({ "ENABLE_LIGHTING",       "1" });
			defines.push_back({ "ENABLE_REFLECTION",      "1" });
			defines.push_back({ "ENABLE_FOG",             "0" });
			defines.push_back({ nullptr, nullptr });

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mSphere.mPixelShader));
		}
	}

	// build sky geometry
	{
		GeometryGenerator::CreateSphere(5000, 3, mSky.mMesh);

		mSky.mRasterizerState = mNoCullRS;
		mSky.mDepthStencilState = mLessEqualDSS;

		mSky.mAlbedoSRV = mTextureManager.CreateSRV(L"desertcube1024.dds");

		// VS
		{
			std::wstring path = base + proj + L"SkyVS.hlsl";

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mSky.mVertexShader));

			// input layout
			{
				std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
				{
					{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				};
				HR(mDevice->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &mSky.mInputLayout));
			}
		}

		// PS
		{
			std::wstring path = base + proj + L"SkyPS.hlsl";

			std::vector<D3D_SHADER_MACRO> defines;
			defines.push_back({ nullptr, nullptr });

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mSky.mPixelShader));
		}
	}

	// objects
	{
		//mTree.LoadModel(mDevice, mTextureManager, "tree.m3d");
		//mTree.mRasterizerState = mNoCullRS;

		//// PS
		//{
		//	std::wstring path = base + proj + L"PS.hlsl";

		//	std::vector<D3D_SHADER_MACRO> defines;
		//	defines.push_back({ "ENABLE_TEXTURE",         "1" });
		//	defines.push_back({ "ENABLE_SPHERE_TEXCOORD", "0" });
		//	defines.push_back({ "ENABLE_NORMAL_MAPPING",  "1" });
		//	defines.push_back({ "ENABLE_ALPHA_CLIPPING",  "1" });
		//	//defines.push_back({ "ENABLE_LIGHTING",       "1" });
		//	defines.push_back({ "ENABLE_REFLECTION",      "0" });
		//	defines.push_back({ "ENABLE_FOG",             "0" });
		//	defines.push_back({ nullptr, nullptr });

		//	ID3DBlob* pCode;
		//	HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
		//	HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mTree.mPixelShader));
		//}

		//mBase.LoadModel(mDevice, mTextureManager, "base.m3d");
		//mStairs.LoadModel(mDevice, mTextureManager, "stairs.m3d");
		//mPillar1.LoadModel(mDevice, mTextureManager, "pillar1.m3d");
		//mPillar2.LoadModel(mDevice, mTextureManager, "pillar2.m3d");
		//mPillar3.LoadModel(mDevice, mTextureManager, "pillar5.m3d");
		//mPillar4.LoadModel(mDevice, mTextureManager, "pillar6.m3d");
		//mRock.LoadModel(mDevice, mTextureManager, "rock.m3d");

		mCharacter.LoadModel(mDevice, mTextureManager, "soldier.m3d", true);

		// VS
		{
			Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
			std::wstring path = base + proj + L"SkinnedVS.hlsl";

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mCharacter.mVertexShader));

			// input layout
			{
				std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
				{
					{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"WEIGHTS",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,   0, 56, D3D11_INPUT_PER_VERTEX_DATA, 0}
				};

				HR(mDevice->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &mCharacter.mInputLayout));
			}
		}
	}

	// object instances
	{
		//GameObjectInstance TreeInstance;
		//TreeInstance.obj = &mTree;
		//mObjectInstances.push_back(TreeInstance);

		//GameObjectInstance BaseInstance;
		//BaseInstance.obj = &mBase;
		//mObjectInstances.push_back(BaseInstance);

		//GameObjectInstance StairsInstance;
		//StairsInstance.obj = &mStairs;
		//StairsInstance.world = XMMatrixRotationY(0.5f * XM_PI) * XMMatrixTranslation(0.0f, -2.5f, -12.0f);
		//mObjectInstances.push_back(StairsInstance);

		//GameObjectInstance Pillar1Instance;
		//Pillar1Instance.obj = &mPillar1;
		//Pillar1Instance.world = XMMatrixScaling(0.8f, 0.8f, 0.8f) * XMMatrixTranslation(-5.0f, 1.5f, +5.0f);
		//mObjectInstances.push_back(Pillar1Instance);

		//GameObjectInstance Pillar2Instance;
		//Pillar2Instance.obj = &mPillar2;
		//Pillar2Instance.world = XMMatrixScaling(0.8f, 0.8f, 0.8f) * XMMatrixTranslation(+5.0f, 1.5f, +5.0f);
		//mObjectInstances.push_back(Pillar2Instance);

		//GameObjectInstance Pillar3Instance;
		//Pillar3Instance.obj = &mPillar3;
		//Pillar3Instance.world = XMMatrixScaling(0.8f, 0.8f, 0.8f) * XMMatrixTranslation(+5.0f, 1.5f, -5.0f);
		//mObjectInstances.push_back(Pillar3Instance);

		//GameObjectInstance Pillar4Instance;
		//Pillar4Instance.obj = &mPillar4;
		//Pillar4Instance.world = XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(-5.0f, 1.0f, -5.0f);
		//mObjectInstances.push_back(Pillar4Instance);

		//GameObjectInstance RockInstance1;
		//RockInstance1.obj = &mRock;
		//RockInstance1.world = XMMatrixScaling(0.8f, 0.8f, 0.8f) * XMMatrixTranslation(-1.0f, 1.4f, -7.0f);
		//mObjectInstances.push_back(RockInstance1);

		//GameObjectInstance RockInstance2;
		//RockInstance2.obj = &mRock;
		//RockInstance2.world = XMMatrixScaling(0.8f, 0.8f, 0.8f) * XMMatrixTranslation(+5.0f, 1.2f, -2.0f);
		//mObjectInstances.push_back(RockInstance2);

		//GameObjectInstance RockInstance3;
		//RockInstance3.obj = &mRock;
		//RockInstance3.world = XMMatrixScaling(0.8f, 0.8f, 0.8f) * XMMatrixTranslation(-4.0f, 1.3f, +3.0f);
		//mObjectInstances.push_back(RockInstance3);

		mCharacterInstance1.obj = &mCharacter;
		mCharacterInstance1.world = XMMatrixScaling(0.05f, 0.05f, -0.05f) * XMMatrixRotationY(XM_PI) * XMMatrixTranslation(-2.0f, 0.0f, -7.0f);
		mCharacterInstance1.ClipName = "Take1";
		mCharacterInstance1.transforms.resize(mCharacterInstance1.obj->mSkinnedData.mBoneHierarchy.size());
		//mObjectInstances.push_back(&mCharacterInstance1);

		mCharacterInstance2.obj = &mCharacter;
		mCharacterInstance2.world = XMMatrixScaling(0.05f, 0.05f, -0.05f) * XMMatrixRotationY(XM_PI) * XMMatrixTranslation(+2.0f, 0.0f, -7.0f);;
		mCharacterInstance2.ClipName = "Take1";
		mCharacterInstance2.transforms.resize(mCharacterInstance2.obj->mSkinnedData.mBoneHierarchy.size());
		//mObjectInstances.push_back(&mCharacterInstance2);
	}

	// create vertex and input buffers
	{
		std::array<GameObject*, 7> objects =
		{
			&mSkull,
			&mGrid,
			&mBox,
			&mCylinder,
			&mSphere,
			&mSky,

			//&mTree,
			//&mBase,
			//&mStairs,
			//&mPillar1,
			//&mPillar2,
			//&mPillar3,
			//&mPillar4,
			//&mRock,

			& mCharacter
		};

		std::vector<GeometryGenerator::Vertex> vertices;
		std::vector<UINT> indices;

		auto AddVertex = [&vertices](GameObject& obj) -> void
		{
			obj.mVertexStart = vertices.size();
			vertices.insert(vertices.end(), obj.mMesh.mVertices.begin(), obj.mMesh.mVertices.end());
		};

		auto AddIndex = [&indices](GameObject& obj) -> void
		{
			obj.mIndexStart = indices.size();
			indices.insert(indices.end(), obj.mMesh.mIndices.begin(), obj.mMesh.mIndices.end());
		};

		for (GameObject* obj : objects)
		{
			AddVertex(*obj);
			AddIndex(*obj);
		}

		Microsoft::WRL::ComPtr<ID3D11Buffer> VB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> IB;

		// VB
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(GeometryGenerator::Vertex) * vertices.size();
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA InitData;
			InitData.pSysMem = vertices.data();
			InitData.SysMemPitch = 0;
			InitData.SysMemSlicePitch = 0;

			HR(mDevice->CreateBuffer(&desc, &InitData, &VB));
		}

		// IB
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof(UINT) * indices.size();
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA InitData;
			InitData.pSysMem = indices.data();
			InitData.SysMemPitch = 0;
			InitData.SysMemSlicePitch = 0;

			HR(mDevice->CreateBuffer(&desc, &InitData, &IB));
		}

		for (GameObject* obj : objects)
		{
			obj->mVertexBuffer = VB;
			obj->mIndexBuffer = IB;
		}
	}

	// sampler state
	{
		D3D11_SAMPLER_DESC desc;
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.MipLODBias = 0;
		desc.MaxAnisotropy = 1;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		ZeroMemory(desc.BorderColor, sizeof(desc.BorderColor));
		desc.MinLOD = 0;
		desc.MaxLOD = 0;

		HR(mDevice->CreateSamplerState(&desc, &mSamplerState));

		mContext->DSSetSamplers(0, 1, &mSamplerState);
		mContext->PSSetSamplers(0, 1, &mSamplerState);
	}

	mShadowMap.Init(mDevice, 2048, 2048);
	mShadowMap.mDebugQuad.Init(mDevice, mMainWindowWidth, mMainWindowHeight, AspectRatio(), DebugQuad::WindowCorner::BottomLeft, 1);
	mContext->PSSetSamplers(1, 1, &mShadowMap.GetSS());

	mSSAO.Init(mDevice, mMainWindowWidth, mMainWindowHeight, mCamera.mFovAngleY, mCamera.mFarZ);
	mSSAO.mDebugQuad.Init(mDevice, mMainWindowWidth, mMainWindowHeight, AspectRatio(), DebugQuad::WindowCorner::BottomRight, AspectRatio());

	mSSR.Init(mDevice, mMainWindowWidth, mMainWindowHeight, mCamera.mFovAngleY, mCamera.mNearZ, mCamera.mFarZ);
	mSSR.mDebugQuad.Init(mDevice, mMainWindowWidth, mMainWindowHeight, AspectRatio(), DebugQuad::WindowCorner::TopRight, AspectRatio());

	mSSPR.Init(mDevice, mMainWindowWidth, mMainWindowHeight, mCamera.mFovAngleY, mCamera.mNearZ, mCamera.mFarZ);
	mSSPR.mDebugQuad.Init(mDevice, mMainWindowWidth, mMainWindowHeight, AspectRatio(), DebugQuad::WindowCorner::TopRight, AspectRatio());

	mSSSR.init(mDevice, mMainWindowWidth, mMainWindowHeight);
	mSSSR.mDebugQuad.Init(mDevice, mMainWindowWidth, mMainWindowHeight, AspectRatio(), DebugQuad::WindowCorner::TopRight, AspectRatio());

	{
		std::wstring path = L"DebugQuadPS.hlsl";

		std::vector<D3D_SHADER_MACRO> defines;
		defines.push_back({ "ENABLE_SSPR", "1" });
		defines.push_back({ nullptr, nullptr });

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
		HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mSSPR.mDebugQuad.mPixelShader));
	}

	//// scene bounds
	//if (!mObjectInstances.empty())
	//{
	//	XMFLOAT3 minima = XMFLOAT3(+FLT_MAX, +FLT_MAX, +FLT_MAX);
	//	XMFLOAT3 maxima = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	//	XMVECTOR min = XMLoadFloat3(&minima);
	//	XMVECTOR max = XMLoadFloat3(&maxima);

	//	for (GameObjectInstance* instance : mObjectInstances)
	//	{
	//		for (const auto& vertex : instance->obj->mMesh.mVertices)
	//		{
	//			XMVECTOR P = XMLoadFloat3(&vertex.mPosition);
	//			min = XMVectorMin(min, P);
	//			max = XMVectorMax(max, P);
	//		}
	//	}

	//	XMStoreFloat3(&mSceneBounds.Center, (min + max) * 0.5f);

	//	XMVECTOR extents = (max - min) * 0.5f;
	//	float RadiusSquared = XMVectorGetByIndex(XMVector3Dot(extents, extents), 0);
	//	mSceneBounds.Radius = std::sqrt(RadiusSquared);
	//}

	{
		// scene albedo RTV and SRV
		{
			SafeRelease(mSceneAlbedoRTV);
			SafeRelease(mSceneAlbedoSRV);

			D3D11_TEXTURE2D_DESC desc;
			desc.Width = mMainWindowWidth;
			desc.Height = mMainWindowHeight;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;

			ID3D11Texture2D* texture = nullptr;
			HR(mDevice->CreateTexture2D(&desc, nullptr, &texture));

			HR(mDevice->CreateRenderTargetView(texture, nullptr, &mSceneAlbedoRTV));
			HR(mDevice->CreateShaderResourceView(texture, nullptr, &mSceneAlbedoSRV));

			SafeRelease(texture);
		}

		mDebugQuad.Init(mDevice, mMainWindowWidth, mMainWindowHeight, AspectRatio(), DebugQuad::WindowCorner::FullWindow, AspectRatio());

		// pixel shader
		{
			std::wstring path = L"DebugQuadPS.hlsl";

			std::vector<D3D_SHADER_MACRO> defines;
			defines.push_back({ "ENABLE_SSR", "1" });
			defines.push_back({ nullptr, nullptr });

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			HR(mDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mDebugQuad.mPixelShader));
		}
	}

	return true;
}

void TestApp::OnResize(GLFWwindow* window, int width, int height)
{
	D3DApp::OnResize(window, width, height);

	mShadowMap.mDebugQuad.OnResize(mMainWindowWidth, mMainWindowHeight, AspectRatio());

	mSSAO.OnResize(mDevice, mMainWindowWidth, mMainWindowHeight, mCamera.mFovAngleY, mCamera.mFarZ);
	mSSAO.mDebugQuad.OnResize(mMainWindowWidth, mMainWindowHeight, AspectRatio());

	mSSR.OnResize(mDevice, mMainWindowWidth, mMainWindowHeight, mCamera.mFovAngleY, mCamera.mNearZ, mCamera.mFarZ);
	mSSPR.OnResize(mDevice, mMainWindowWidth, mMainWindowHeight, mCamera.mFovAngleY, mCamera.mNearZ, mCamera.mFarZ);

	mSSSR.OnResize(mDevice, mMainWindowWidth, mMainWindowHeight);

	// scene albedo RTV and SRV
	{
		SafeRelease(mSceneAlbedoRTV);
		SafeRelease(mSceneAlbedoSRV);

		D3D11_TEXTURE2D_DESC desc;
		desc.Width = mMainWindowWidth;
		desc.Height = mMainWindowHeight;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		ID3D11Texture2D* texture = nullptr;
		HR(mDevice->CreateTexture2D(&desc, nullptr, &texture));

		HR(mDevice->CreateRenderTargetView(texture, nullptr, &mSceneAlbedoRTV));
		HR(mDevice->CreateShaderResourceView(texture, nullptr, &mSceneAlbedoSRV));

		SafeRelease(texture);
	}

	mDebugQuad.OnResize(mMainWindowWidth, mMainWindowHeight, AspectRatio());
}

void TestApp::UpdateScene(float dt)
{
	if (IsKeyPressed(GLFW_KEY_W))
	{
		mCamera.walk(+10 * dt);
	}

	if (IsKeyPressed(GLFW_KEY_S))
	{
		mCamera.walk(-10 * dt);
	}

	if (IsKeyPressed(GLFW_KEY_A))
	{
		mCamera.strafe(-10 * dt);
	}

	if (IsKeyPressed(GLFW_KEY_D))
	{
		mCamera.strafe(+10 * dt);
	}

	//// animate lights
	//{
	//	mLightAngle += 0.1f * dt;

	//	XMMATRIX R = XMMatrixRotationY(mLightAngle);

	//	for (UINT i = 0; i < 3; ++i)
	//	{
	//		XMVECTOR D = XMLoadFloat3(&mLightsCache[i]);
	//		D = XMVector3TransformNormal(D, R);
	//		XMStoreFloat3(&mLights[i].mDirection, D);
	//	}
	//}

	//// update skull animation
	//{
	//	mSkull.mAnimation.mCurrTime += dt;

	//	if (mSkull.mAnimation.mCurrTime >= mSkull.mAnimation.GetTimeEnd())
	//	{
	//		mSkull.mAnimation.mCurrTime = 0;
	//	}

	//	mSkull.mAnimation.interpolate(mSkull.mAnimation.mCurrTime, mSkull.mWorld);
	//}

	// update characters animation
	//mCharacterInstance1.update(dt);
	//mCharacterInstance2.update(dt);

	// build shadow transform
	mShadowMap.BuildTranform(mLights[0].mDirection, mSceneBounds);

	mCamera.UpdateView();
}

void TestApp::DrawSceneToShadowMap()
{
	XMMATRIX view = XMLoadFloat4x4(&mShadowMap.mLightView);
	XMMATRIX proj = XMLoadFloat4x4(&mShadowMap.mLightProj);
	XMMATRIX ViewProj = XMMatrixMultiply(view, proj);

	auto SetPerObjectCB = [this, &ViewProj](GameObject* obj) -> void
	{
		ShadowMap::PerObjectCB buffer;
		XMStoreFloat4x4(&buffer.mWorldViewProj, obj->mWorld * ViewProj);
		XMStoreFloat4x4(&buffer.mTexTransform, obj->mTexCoordTransform);
		mContext->UpdateSubresource(mShadowMap.GetCB(), 0, nullptr, &buffer, 0, 0);
		mContext->VSSetConstantBuffers(0, 1, &mShadowMap.GetCB());
	};

	auto DrawGameObject = [this, &SetPerObjectCB](GameObject* obj) -> void
	{
		FLOAT BlendFactor[] = { 0, 0, 0, 0 };

		// shaders
		mContext->VSSetShader(mShadowMap.GetVS(), nullptr, 0);
		mContext->PSSetShader(nullptr, nullptr, 0);

		// input layout
		mContext->IASetInputLayout(mShadowMap.GetIL());

		// primitive topology
		mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// vertex and index buffers
		if (obj->mInstancedBuffer)
		{
			UINT stride[2] = { sizeof(GeometryGenerator::Vertex), sizeof(GameObject::InstancedData) };
			UINT offset[2] = { 0, 0 };

			ID3D11Buffer* vbs[2] = { obj->mVertexBuffer.Get(), obj->mInstancedBuffer };

			mContext->IASetVertexBuffers(0, 2, vbs, stride, offset);
			mContext->IASetIndexBuffer(obj->mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		}
		else
		{
			UINT stride = sizeof(GeometryGenerator::Vertex);
			UINT offset = 0;

			mContext->IASetVertexBuffers(0, 1, obj->mVertexBuffer.GetAddressOf(), &stride, &offset);
			mContext->IASetIndexBuffer(obj->mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		}

		//  per object constant buffer
		SetPerObjectCB(obj);

		// textures
		{
			mContext->PSSetShaderResources(0, 1, &obj->mAlbedoSRV);
			//mContext->DSSetShaderResources(1, 1, obj->mNormalSRV.GetAddressOf());
			//mContext->PSSetShaderResources(1, 1, obj->mNormalSRV.GetAddressOf());
		}

		// rasterizer, blend and depth-stencil states
		mContext->RSSetState(mShadowMap.GetRS());
		mContext->OMSetBlendState(obj->mBlendState.Get(), BlendFactor, 0xFFFFFFFF);
		mContext->OMSetDepthStencilState(obj->mDepthStencilState.Get(), obj->mStencilRef);

		// draw call
		if (obj->mIndexBuffer && obj->mInstancedBuffer)
		{
			mContext->DrawIndexedInstanced(obj->mMesh.mIndices.size(), obj->mVisibleInstanceCount, 0, 0, 0);
		}
		else if (obj->mIndexBuffer)
		{
			mContext->DrawIndexed(obj->mMesh.mIndices.size(), obj->mIndexStart, obj->mVertexStart);
		}
		else
		{
			mContext->Draw(obj->mMesh.mVertices.size(), obj->mVertexStart);
		}

		// unbind SRV
		ID3D11ShaderResourceView* const NullSRV[2] = { nullptr, nullptr };
		mContext->PSSetShaderResources(0, 2, NullSRV);
	};

	DrawGameObject(&mGrid);
	DrawGameObject(&mBox);
	DrawGameObject(&mSkull);

	for (UINT i = 0; i < 5; ++i)
	{
		mCylinder.mWorld = XMMatrixTranslation(-5, 1.5f, -10 + i * 5.0f);
		DrawGameObject(&mCylinder);
		mCylinder.mWorld = XMMatrixTranslation(+5, 1.5f, -10 + i * 5.0f);
		DrawGameObject(&mCylinder);

		mSphere.mWorld = XMMatrixTranslation(-5, 3.5f, -10 + i * 5.0f);
		DrawGameObject(&mSphere);
		mSphere.mWorld = XMMatrixTranslation(+5, 3.5f, -10 + i * 5.0f);
		DrawGameObject(&mSphere);
	}

	for (GameObjectInstance* instance : mObjectInstances)
	{
		GameObject* obj = instance->obj;

		// shaders
		{
			mContext->VSSetShader(mShadowMap.GetVS(obj->mIsSkinned), nullptr, 0);
			mContext->PSSetShader(mShadowMap.GetPS(), nullptr, 0);
		}

		// input layout
		mContext->IASetInputLayout(mShadowMap.GetIL(obj->mIsSkinned));

		// primitive topology
		mContext->IASetPrimitiveTopology(obj->mPrimitiveTopology);

		// vertex and index buffers
		{
			UINT stride = sizeof(GeometryGenerator::Vertex);
			UINT offset = 0;

			mContext->IASetVertexBuffers(0, 1, obj->mVertexBuffer.GetAddressOf(), &stride, &offset);
			mContext->IASetIndexBuffer(obj->mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		}

		// rasterizer, blend and depth-stencil states
		{
			mContext->RSSetState(mShadowMap.GetRS());

			FLOAT BlendFactor[] = { 0, 0, 0, 0 };
			mContext->OMSetBlendState(obj->mBlendState.Get(), BlendFactor, 0xFFFFFFFF);

			mContext->OMSetDepthStencilState(obj->mDepthStencilState.Get(), obj->mStencilRef);
		}

		// per object constant buffer
		{
			ShadowMap::PerObjectCB buffer;
			XMStoreFloat4x4(&buffer.mWorldViewProj, instance->world * ViewProj);
			XMStoreFloat4x4(&buffer.mTexTransform, obj->mTexCoordTransform);
			mContext->UpdateSubresource(mShadowMap.GetCB(), 0, nullptr, &buffer, 0, 0);
			mContext->VSSetConstantBuffers(0, 1, &mShadowMap.GetCB());
		}

		// per skinned constant buffer
		{
			PerSkinnedCB buffer;
			ZeroMemory(&buffer.mBoneTransforms, sizeof(buffer.mBoneTransforms));
			CopyMemory(&buffer.mBoneTransforms, instance->transforms.data(), instance->transforms.size() * sizeof(XMFLOAT4X4));

			mContext->UpdateSubresource(mPerSkinnedCB, 0, 0, &buffer, 0, 0);
			mContext->VSSetConstantBuffers(2, 1, &mPerSkinnedCB);
		}

		for (UINT i = 0; i < obj->mSubsets.size(); ++i)
		{
			// bind SRVs
			{
				mContext->PSSetShaderResources(0, 1, &obj->mDiffuseMapSRVs[i]);
			}

			// draw call
			const Subset& subset = obj->mSubsets[i];
			mContext->DrawIndexed(subset.FaceCount * 3, obj->mIndexStart + subset.FaceStart * 3, obj->mVertexStart);

			// unbind SRVs
			{
				ID3D11ShaderResourceView* const NullSRVs[1] = { nullptr };
				mContext->PSSetShaderResources(0, 1, NullSRVs);
			}
		}
	}
}

void TestApp::DrawSceneToSSAONormalDepthMap()
{
	auto SetPerObjectCB = [this](GameObject* obj) -> void
	{
		XMMATRIX view = XMLoadFloat4x4(&mCamera.mView);
		XMMATRIX WorldView = obj->mWorld * view;

		SSAO::NormalDepthCB buffer;
		XMStoreFloat4x4(&buffer.WorldView, WorldView);
		XMStoreFloat4x4(&buffer.WorldViewProj, WorldView * mCamera.mProj);
		XMStoreFloat4x4(&buffer.WorldInverseTransposeView, GameMath::InverseTranspose(obj->mWorld) * view);
		XMStoreFloat4x4(&buffer.TexCoordTransform, obj->mTexCoordTransform);
		mContext->UpdateSubresource(mSSAO.GetNormalDepthCB(), 0, nullptr, &buffer, 0, 0);
		mContext->VSSetConstantBuffers(0, 1, &mSSAO.GetNormalDepthCB());
	};

	auto DrawGameObject = [this, &SetPerObjectCB](GameObject* obj) -> void
	{
		FLOAT BlendFactor[] = { 0, 0, 0, 0 };

		// shaders
		mContext->VSSetShader(mSSAO.GetNormalDepthVS(), nullptr, 0);
		mContext->PSSetShader(mSSAO.GetNormalDepthPS(), nullptr, 0);

		// input layout
		mContext->IASetInputLayout(mSSAO.GetNormalDepthIL());

		// primitive topology
		mContext->IASetPrimitiveTopology(obj->mPrimitiveTopology);

		// vertex and index buffers
		if (obj->mInstancedBuffer)
		{
			UINT stride[2] = { sizeof(GeometryGenerator::Vertex), sizeof(GameObject::InstancedData) };
			UINT offset[2] = { 0, 0 };

			ID3D11Buffer* vbs[2] = { obj->mVertexBuffer.Get(), obj->mInstancedBuffer };

			mContext->IASetVertexBuffers(0, 2, vbs, stride, offset);
			mContext->IASetIndexBuffer(obj->mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		}
		else
		{
			UINT stride = sizeof(GeometryGenerator::Vertex);
			UINT offset = 0;

			mContext->IASetVertexBuffers(0, 1, obj->mVertexBuffer.GetAddressOf(), &stride, &offset);
			mContext->IASetIndexBuffer(obj->mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		}

		//  per object constant buffer
		SetPerObjectCB(obj);

		// textures
		{
			mContext->PSSetShaderResources(0, 1, &obj->mAlbedoSRV);

			// TODO : add normal mapping ?

			//mContext->DSSetShaderResources(1, 1, obj->mNormalSRV.GetAddressOf());
			//mContext->PSSetShaderResources(1, 1, obj->mNormalSRV.GetAddressOf());
		}

		// rasterizer, blend and depth-stencil states
		mContext->RSSetState(obj->mRasterizerState.Get());
		mContext->OMSetBlendState(obj->mBlendState.Get(), BlendFactor, 0xFFFFFFFF);
		mContext->OMSetDepthStencilState(obj->mDepthStencilState.Get(), obj->mStencilRef);

		// draw call
		if (obj->mIndexBuffer && obj->mInstancedBuffer)
		{
			mContext->DrawIndexedInstanced(obj->mMesh.mIndices.size(), obj->mVisibleInstanceCount, 0, 0, 0);
		}
		else if (obj->mIndexBuffer)
		{
			mContext->DrawIndexed(obj->mMesh.mIndices.size(), obj->mIndexStart, obj->mVertexStart);
		}
		else
		{
			mContext->Draw(obj->mMesh.mVertices.size(), obj->mVertexStart);
		}

		// unbind SRV
		ID3D11ShaderResourceView* const NullSRV[1] = { nullptr };
		mContext->PSSetShaderResources(0, 1, NullSRV);
	};

	if (!mEnableSSPR) DrawGameObject(&mGrid);
	DrawGameObject(&mBox);
	DrawGameObject(&mSkull);

	for (UINT i = 0; i < 5; ++i)
	{
		mCylinder.mWorld = XMMatrixTranslation(-5, 1.5f, -10 + i * 5.0f);
		DrawGameObject(&mCylinder);
		mCylinder.mWorld = XMMatrixTranslation(+5, 1.5f, -10 + i * 5.0f);
		DrawGameObject(&mCylinder);

		mSphere.mWorld = XMMatrixTranslation(-5, 3.5f, -10 + i * 5.0f);
		DrawGameObject(&mSphere);
		mSphere.mWorld = XMMatrixTranslation(+5, 3.5f, -10 + i * 5.0f);
		DrawGameObject(&mSphere);
	}

	for (GameObjectInstance* instance : mObjectInstances)
	{
		GameObject* obj = instance->obj;

		// vertex shader
		{
			mContext->VSSetShader(mSSAO.GetNormalDepthVS(obj->mIsSkinned), nullptr, 0);
		}

		// input layout
		mContext->IASetInputLayout(mSSAO.GetNormalDepthIL(obj->mIsSkinned));

		// primitive topology
		mContext->IASetPrimitiveTopology(obj->mPrimitiveTopology);

		// vertex and index buffers
		{
			UINT stride = sizeof(GeometryGenerator::Vertex);
			UINT offset = 0;

			mContext->IASetVertexBuffers(0, 1, obj->mVertexBuffer.GetAddressOf(), &stride, &offset);
			mContext->IASetIndexBuffer(obj->mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		}

		// rasterizer, blend and depth-stencil states
		{
			mContext->RSSetState(obj->mRasterizerState.Get());

			FLOAT BlendFactor[] = { 0, 0, 0, 0 };
			mContext->OMSetBlendState(obj->mBlendState.Get(), BlendFactor, 0xFFFFFFFF);

			mContext->OMSetDepthStencilState(obj->mDepthStencilState.Get(), obj->mStencilRef);
		}

		// per object constant buffer
		{
			XMMATRIX view = XMLoadFloat4x4(&mCamera.mView);
			XMMATRIX WorldView = instance->world * view;

			SSAO::NormalDepthCB buffer;
			XMStoreFloat4x4(&buffer.WorldView, WorldView);
			XMStoreFloat4x4(&buffer.WorldViewProj, WorldView * mCamera.mProj);
			XMStoreFloat4x4(&buffer.WorldInverseTransposeView, GameMath::InverseTranspose(instance->world) * view);
			XMStoreFloat4x4(&buffer.TexCoordTransform, obj->mTexCoordTransform);
			mContext->UpdateSubresource(mSSAO.GetNormalDepthCB(), 0, nullptr, &buffer, 0, 0);
			mContext->VSSetConstantBuffers(0, 1, &mSSAO.GetNormalDepthCB());
		}

		// per skinned constant buffer
		{
			PerSkinnedCB buffer;
			ZeroMemory(&buffer.mBoneTransforms, sizeof(buffer.mBoneTransforms));
			CopyMemory(&buffer.mBoneTransforms, instance->transforms.data(), instance->transforms.size() * sizeof(XMFLOAT4X4));

			mContext->UpdateSubresource(mPerSkinnedCB, 0, 0, &buffer, 0, 0);
			mContext->VSSetConstantBuffers(2, 1, &mPerSkinnedCB);
		}

		for (UINT i = 0; i < obj->mSubsets.size(); ++i)
		{
			// pixel shader
			{
				mContext->PSSetShader(mSSAO.GetNormalDepthPS(obj->mIsAlphaClipping[i]), nullptr, 0);
			}

			// bind SRVs
			{
				mContext->PSSetShaderResources(0, 1, &obj->mDiffuseMapSRVs[i]);
				//mContext->PSSetShaderResources(1, 1, &obj->mNormalMapSRVs[i]);
			}

			// draw call
			const Subset& subset = obj->mSubsets[i];
			mContext->DrawIndexed(subset.FaceCount * 3, obj->mIndexStart + subset.FaceStart * 3, obj->mVertexStart);

			// unbind SRVs
			{
				ID3D11ShaderResourceView* const NullSRVs[2] = { nullptr, nullptr };
				mContext->PSSetShaderResources(0, 2, NullSRVs);
			}
		}
	}
}

void TestApp::DrawScene()
{
	assert(mContext);
	assert(mSwapChain);

	// bind shadow map dsv and set null render target
	mShadowMap.BindDSVAndSetNullRenderTarget(mContext);
	// draw scene to shadow map
	DrawSceneToShadowMap();

	// restore rasterazer state -> no need for this, DrawGameObject sets the rasterazer state for each object

	// restore viewport
	mContext->RSSetViewports(1, &mViewport);

	// render view space normals and depths
	mSSAO.BindNormalDepthRenderTarget(mContext, mDepthStencilView);
	DrawSceneToSSAONormalDepthMap();

	//if (!IsKeyPressed(GLFW_KEY_4))
	//{
	//	// compute ambient occlusion
	//	mSSAO.ComputeAmbientMap(mContext, mCamera);
	//	// blur ambient map
	//	mSSAO.BlurAmbientMap(mContext, 4);
	//}
	//else
	{
		mContext->OMSetRenderTargets(1, &mSSAO.GetAmbientMapRTV(), nullptr);
		mContext->ClearRenderTargetView(mSSAO.GetAmbientMapRTV(), Colors::White);
	}

	// SSR
	{
		// hierarchical depth buffer
		//mSSR.ComputeHierarchicalDepthBuffer(mDevice, mContext, mSSAO.GetNormalDepthSRV());

		//mSSR.ComputeReflectionsMap(mContext, mCamera, mSSAO.GetNormalDepthSRV());
		//mSSPR.ComputeReflectionsMap(mContext, mCamera, mSSAO.GetNormalDepthSRV());

		mSSSR.draw(mContext, mCamera, mSSAO.GetNormalDepthSRV());
	}

	// restore back and depth buffers, and viewport
	//mContext->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView);
	mContext->RSSetViewports(1, &mViewport);

	//mContext->ClearRenderTargetView(mRenderTargetView, Colors::Silver);
	//mContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
	//mContext->OMSetDepthStencilState(mEqualDSS.Get(), 0);

	// bind shadow map and ambient map as SRV
	mContext->PSSetShaderResources(3, 1, &mShadowMap.GetSRV());
	mContext->PSSetShaderResources(4, 1, &mSSAO.GetAmbientMapSRV());

	auto SetPerFrameCB = [this]() -> void
	{
		PerFrameCB buffer;
		buffer.mLights[0] = mLights[0];
		buffer.mLights[1] = mLights[1];
		buffer.mLights[2] = mLights[2];
		buffer.mEyePositionW = mCamera.mPosition;
		buffer.mFogStart = 15;
		buffer.mFogRange = 175;
		XMStoreFloat4(&buffer.mFogColor, Colors::Silver);
		buffer.mHeightScale = 0.07f;
		buffer.mMaxTessDistance = 1;
		buffer.mMinTessDistance = 25;
		buffer.mMinTessFactor = 1;
		buffer.mMaxTessFactor = 5;
		XMMATRIX V = XMLoadFloat4x4(&mCamera.mView);
		XMStoreFloat4x4(&buffer.mViewProj, V * mCamera.mProj);

		mContext->UpdateSubresource(mPerFrameCB, 0, 0, &buffer, 0, 0);

		mContext->VSSetConstantBuffers(1, 1, &mPerFrameCB);
		//mContext->DSSetConstantBuffers(1, 1, &mPerFrameCB);
		mContext->PSSetConstantBuffers(1, 1, &mPerFrameCB);
	};

	auto SetPerObjectCB = [this](GameObject* obj) -> void
	{
		XMMATRIX V = XMLoadFloat4x4(&mCamera.mView);
		XMMATRIX S = XMLoadFloat4x4(&mShadowMap.mShadowTransform);

		// transform NDC space [-1,+1]^2 to texture space [0,1]^2
		XMMATRIX T
		(
			+0.5f,  0.0f, 0.0f, 0.0f,
			 0.0f, -0.5f, 0.0f, 0.0f,
			 0.0f,  0.0f, 1.0f, 0.0f,
			+0.5f, +0.5f, 0.0f, 1.0f
		);

		PerObjectCB buffer;
		XMStoreFloat4x4(&buffer.mWorld, obj->mWorld);
		XMStoreFloat4x4(&buffer.mWorldInverseTranspose, GameMath::InverseTranspose(obj->mWorld));
		XMStoreFloat4x4(&buffer.mWorldViewProj, obj->mWorld * V * mCamera.mProj);
		buffer.mMaterial = obj->mMaterial;
		XMStoreFloat4x4(&buffer.mTexCoordTransform, obj->mTexCoordTransform);
		XMStoreFloat4x4(&buffer.mShadowTransform, obj->mWorld * S);
		XMStoreFloat4x4(&buffer.mWorldViewProjTexture, obj->mWorld * V * mCamera.mProj * T);

		mContext->UpdateSubresource(mPerObjectCB, 0, 0, &buffer, 0, 0);

		mContext->VSSetConstantBuffers(0, 1, &mPerObjectCB);
		mContext->PSSetConstantBuffers(0, 1, &mPerObjectCB);
	};

	auto DrawGameObject = [this, &SetPerObjectCB](GameObject* obj, bool flag = false) -> void
	{
		FLOAT BlendFactor[] = { 0, 0, 0, 0 };

		// shaders
		mContext->VSSetShader(obj->mVertexShader.Get(), nullptr, 0);
		//mContext->HSSetShader(obj->mHullShader.Get(), nullptr, 0);
		//mContext->DSSetShader(obj->mDomainShader.Get(), nullptr, 0);
		//mContext->GSSetShader(obj->mGeometryShader.Get(), nullptr, 0);
		mContext->PSSetShader(obj->mPixelShader.Get(), nullptr, 0);

		// input layout
		mContext->IASetInputLayout(obj->mInputLayout.Get());

		// primitive topology
		mContext->IASetPrimitiveTopology(obj->mPrimitiveTopology);

		// vertex and index buffers
		if (obj->mInstancedBuffer)
		{
			UINT stride[2] = { sizeof(GeometryGenerator::Vertex), sizeof(GameObject::InstancedData) };
			UINT offset[2] = { 0, 0 };

			ID3D11Buffer* vbs[2] = { obj->mVertexBuffer.Get(), obj->mInstancedBuffer };

			mContext->IASetVertexBuffers(0, 2, vbs, stride, offset);
			mContext->IASetIndexBuffer(obj->mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		}
		else
		{
			UINT stride = sizeof(GeometryGenerator::Vertex);
			UINT offset = 0;

			mContext->IASetVertexBuffers(0, 1, obj->mVertexBuffer.GetAddressOf(), &stride, &offset);
			mContext->IASetIndexBuffer(obj->mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		}

		// constant buffer per object
		SetPerObjectCB(obj);

		// bind SRVs
		{
			mContext->PSSetShaderResources(0, 1, &obj->mAlbedoSRV);
			mContext->DSSetShaderResources(1, 1, &obj->mNormalSRV);
			mContext->PSSetShaderResources(1, 1, &obj->mNormalSRV);
		}

		// rasterizer, blend and depth-stencil states

		if (IsKeyPressed(GLFW_KEY_1))
		{
			mContext->RSSetState(mWireframeRS.Get());
		}
		else
		{
			mContext->RSSetState(obj->mRasterizerState.Get());
		}

		mContext->OMSetBlendState(obj->mBlendState.Get(), BlendFactor, 0xFFFFFFFF);

		if (obj->mDepthStencilState.Get() != nullptr)
		{
			mContext->OMSetDepthStencilState(obj->mDepthStencilState.Get(), obj->mStencilRef);
		}
		else if (IsKeyPressed(GLFW_KEY_1) || flag)
		{
			mContext->OMSetDepthStencilState(nullptr, 0);
		}
		else
		{
			mContext->OMSetDepthStencilState(mEqualDSS.Get(), 0);
		}

		// draw call
		if (obj->mIndexBuffer && obj->mInstancedBuffer)
		{
			mContext->DrawIndexedInstanced(obj->mMesh.mIndices.size(), obj->mVisibleInstanceCount, 0, 0, 0);
		}
		else if (obj->mIndexBuffer)
		{
			mContext->DrawIndexed(obj->mMesh.mIndices.size(), obj->mIndexStart, obj->mVertexStart);
		}
		else
		{
			mContext->Draw(obj->mMesh.mVertices.size(), obj->mVertexStart);
		}

		// unbind SRVs
		ID3D11ShaderResourceView* const NullSRV[2] = { nullptr, nullptr };
		mContext->PSSetShaderResources(0, 2, NullSRV);
	};

	SetPerFrameCB();

	auto DrawSceneTo = [this, &DrawGameObject](ID3D11RenderTargetView* rtv)
	{
		mContext->OMSetRenderTargets(1, &rtv, mDepthStencilView);
		mContext->RSSetViewports(1, &mViewport);

		mContext->ClearRenderTargetView(rtv, Colors::Silver);

		// draw without reflection
		{
			if (rtv == mRenderTargetView) DrawGameObject(&mGrid, mEnableSSPR);
			DrawGameObject(&mBox);

			for (UINT i = 0; i < 5; ++i)
			{
				mCylinder.mWorld = XMMatrixTranslation(-5, 1.5f, -10 + i * 5.0f);
				DrawGameObject(&mCylinder);
				mCylinder.mWorld = XMMatrixTranslation(+5, 1.5f, -10 + i * 5.0f);
				DrawGameObject(&mCylinder);
			}
		}

		// turn off tessellation

		// draw with reflection
		{
			// bind cube map SRV
			mContext->PSSetShaderResources(2, 1, &mSky.mAlbedoSRV);

			DrawGameObject(&mSkull);

			for (UINT i = 0; i < 5; ++i)
			{
				mSphere.mWorld = XMMatrixTranslation(-5, 3.5f, -10 + i * 5.0f);
				DrawGameObject(&mSphere);
				mSphere.mWorld = XMMatrixTranslation(+5, 3.5f, -10 + i * 5.0f);
				DrawGameObject(&mSphere);
			}

			// unbind SRV
			ID3D11ShaderResourceView* const NullSRV = nullptr;
			mContext->PSSetShaderResources(2, 1, &NullSRV);
		}

		// draw animated characters
		for (GameObjectInstance* instance : mObjectInstances)
		{
			GameObject* obj = instance->obj;

			// shaders
			{
				mContext->VSSetShader(obj->mVertexShader.Get(), nullptr, 0);
				mContext->PSSetShader(obj->mPixelShader.Get(), nullptr, 0);
			}

			// input layout
			mContext->IASetInputLayout(obj->mInputLayout.Get());

			// primitive topology
			mContext->IASetPrimitiveTopology(obj->mPrimitiveTopology);

			// vertex and index buffers
			{
				UINT stride = sizeof(GeometryGenerator::Vertex);
				UINT offset = 0;

				mContext->IASetVertexBuffers(0, 1, obj->mVertexBuffer.GetAddressOf(), &stride, &offset);
				mContext->IASetIndexBuffer(obj->mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
			}

			// rasterizer, blend and depth-stencil states
			{
				if (IsKeyPressed(GLFW_KEY_1))
				{
					mContext->RSSetState(mWireframeRS.Get());
				}
				else
				{
					mContext->RSSetState(obj->mRasterizerState.Get());
				}

				FLOAT BlendFactor[] = { 0, 0, 0, 0 };
				mContext->OMSetBlendState(obj->mBlendState.Get(), BlendFactor, 0xFFFFFFFF);

				if (obj->mDepthStencilState.Get() != nullptr || IsKeyPressed(GLFW_KEY_1))
				{
					mContext->OMSetDepthStencilState(obj->mDepthStencilState.Get(), obj->mStencilRef);
				}
				else
				{
					mContext->OMSetDepthStencilState(mEqualDSS.Get(), 0);
				}
			}

			// per skinned constant buffer
			{
				PerSkinnedCB buffer;
				ZeroMemory(&buffer.mBoneTransforms, sizeof(buffer.mBoneTransforms));
				CopyMemory(&buffer.mBoneTransforms, instance->transforms.data(), instance->transforms.size() * sizeof(XMFLOAT4X4));

				mContext->UpdateSubresource(mPerSkinnedCB, 0, 0, &buffer, 0, 0);
				mContext->VSSetConstantBuffers(2, 1, &mPerSkinnedCB);
			}

			XMMATRIX W = instance->world;
			XMMATRIX V = XMLoadFloat4x4(&mCamera.mView);
			XMMATRIX WorldViewProj = W * V * mCamera.mProj;
			XMMATRIX S = XMLoadFloat4x4(&mShadowMap.mShadowTransform);
			XMMATRIX WorldInverseTranspose = GameMath::InverseTranspose(W);

			// transform NDC space [-1,+1]^2 to texture space [0,1]^2
			XMMATRIX T
			(
				+0.5f,  0.0f, 0.0f, 0.0f,
				 0.0f, -0.5f, 0.0f, 0.0f,
				 0.0f,  0.0f, 1.0f, 0.0f,
				+0.5f, +0.5f, 0.0f, 1.0f
			);

			for (UINT i = 0; i < obj->mSubsets.size(); ++i)
			{
				// per object constant buffer
				{
					PerObjectCB buffer;
					XMStoreFloat4x4(&buffer.mWorld, W);
					XMStoreFloat4x4(&buffer.mWorldInverseTranspose, WorldInverseTranspose);
					XMStoreFloat4x4(&buffer.mWorldViewProj, WorldViewProj);
					buffer.mMaterial = obj->mMaterials[i];
					XMStoreFloat4x4(&buffer.mTexCoordTransform, obj->mTexCoordTransform);
					XMStoreFloat4x4(&buffer.mShadowTransform, W * S);
					XMStoreFloat4x4(&buffer.mWorldViewProjTexture, WorldViewProj * T);

					mContext->UpdateSubresource(mPerObjectCB, 0, 0, &buffer, 0, 0);

					mContext->VSSetConstantBuffers(0, 1, &mPerObjectCB);
					mContext->PSSetConstantBuffers(0, 1, &mPerObjectCB);
				}

				// bind SRVs
				{
					mContext->PSSetShaderResources(0, 1, &obj->mDiffuseMapSRVs[i]);
					mContext->PSSetShaderResources(1, 1, &obj->mNormalMapSRVs[i]);
				}

				// draw call
				const Subset& subset = obj->mSubsets[i];
				mContext->DrawIndexed(subset.FaceCount * 3, obj->mIndexStart + subset.FaceStart * 3, obj->mVertexStart);


				// unbind SRVs
				{
					ID3D11ShaderResourceView* const NullSRVs[2] = { nullptr, nullptr };
					mContext->PSSetShaderResources(0, 2, NullSRVs);
				}
			}
		}

		// draw sky
		DrawGameObject(&mSky);
	};

	//mGrid.mPixelShader = nullptr;
	DrawSceneTo(mSceneAlbedoRTV);

	mContext->OMSetRenderTargets(0, nullptr, nullptr);

	// reset depth stencil state
	mContext->OMSetDepthStencilState(nullptr, 0);
	
	//{
	//	mContext->OMSetRenderTargets(1, &mRenderTargetView, nullptr);
	//	mContext->RSSetViewports(1, &mViewport);

	//	mContext->ClearRenderTargetView(mRenderTargetView, Colors::Silver);
	//}

	{
		mGrid.mPixelShader = mEnableSSPR ? mSSPR.mPixelShader : mSSR.mPixelShader;

		// bind reflections map and scene albedo map SRVs
		ID3D11ShaderResourceView* const SRVs[2] = {
			//mEnableSSPR ? mSSPR.mReflectionsMapSRV : mSSR.mReflectionsMapSRV,
			mSSSR.mSRV,
			mSceneAlbedoSRV
		};
		mContext->PSSetShaderResources(5, 2, SRVs);
	}

	DrawSceneTo(mRenderTargetView);

	{
		mGrid.mPixelShader = mBox.mPixelShader;

		// unbind reflections map and scene albedo map SRVs
		ID3D11ShaderResourceView* const NullSRVs[2] = { nullptr, nullptr };
		mContext->PSSetShaderResources(5, 2, NullSRVs);
	}

	// unbind shadow map and ambient map as SRV
	ID3D11ShaderResourceView* const NullSRVs[2] = { nullptr, nullptr };
	mContext->PSSetShaderResources(3, 2, NullSRVs);

	// reset depth stencil state
	mContext->OMSetDepthStencilState(nullptr, 0);

	//// render scene with SSR
	//{
	//	std::vector<ID3D11ShaderResourceView*> SRVs = {
	//		mSceneAlbedoSRV,
	//		mSSR.mReflectionsMapSRV
	//	};

	//	mDebugQuad.Draw(mContext, SRVs);
	//}

	if (IsKeyPressed(GLFW_KEY_2))
	{
		mShadowMap.mDebugQuad.Draw(mContext, mShadowMap.GetSRV());
	}

	if (IsKeyPressed(GLFW_KEY_3))
	{
		mSSAO.mDebugQuad.Draw(mContext, mSSAO.GetAmbientMapSRV());
		//mSSAO.mDebugQuad.Draw(mContext, mSSAO.GetNormalDepthSRV());
	}

	if (IsKeyPressed(GLFW_KEY_5))
	{
		//if (mEnableSSPR)
		//{
		//	mSSPR.mDebugQuad.Draw(mContext, mSSPR.mReflectionsMapSRV);
		//}
		//else
		//{
		//	mSSR.mDebugQuad.Draw(mContext, mSSR.mReflectionsMapSRV);
		//}
		mSSSR.mDebugQuad.Draw(mContext, mSSSR.mSRV);
	}

	mSwapChain->Present(0, 0);
}

int main()
{
	TestApp app;

	if (!app.Init()) return 0;

	return app.Run();
}