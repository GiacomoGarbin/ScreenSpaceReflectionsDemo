#include "SSSR.h"
#include "BlueNoise.h"

SSSR::SSSR() :
	mComputeHierarchicalDepthBufferCS(nullptr),
	mHierarchicalRayMarchingCS(nullptr),

	mHierarchicalDepthBufferUAV(nullptr),
	mHierarchicalDepthBufferSRV(nullptr),
	mUAV(nullptr),
	mSRV(nullptr),

	mHierarchicalRayMarchingCB(nullptr),

	mSobolBuffer(nullptr),
	mRankingTileBuffer(nullptr),
	mScramblingTileBuffer(nullptr),

	mSobolSRV(nullptr),
	mRankingTileSRV(nullptr),
	mScramblingTileSRV(nullptr)
{}

SSSR::~SSSR()
{
	SafeRelease(mComputeHierarchicalDepthBufferCS);
	SafeRelease(mHierarchicalRayMarchingCS);

	SafeRelease(mHierarchicalDepthBufferUAV);
	SafeRelease(mHierarchicalDepthBufferSRV);
	SafeRelease(mUAV);
	SafeRelease(mSRV);
	
	SafeRelease(mHierarchicalRayMarchingCB);

	SafeRelease(mSobolBuffer);
	SafeRelease(mRankingTileBuffer);
	SafeRelease(mScramblingTileBuffer);

	SafeRelease(mSobolSRV);
	SafeRelease(mRankingTileSRV);
	SafeRelease(mScramblingTileSRV);
}

void SSSR::init(ID3D11Device* device, UINT width, UINT height)
{
	OnResize(device, width, height);

	// compute hierarchical depth buffer CS
	{
		std::wstring path = L"ComputeHierarchicalDepthBufferCS.hlsl";

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "cs_5_0", 0, 0, &pCode, nullptr));
		HR(device->CreateComputeShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mComputeHierarchicalDepthBufferCS));
	}

	// hierarchical ray marching CS
	{
		std::wstring path = L"HierarchicalRayMarchingCS.hlsl";

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "cs_5_0", 0, 0, &pCode, nullptr));
		HR(device->CreateComputeShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mHierarchicalRayMarchingCS));
	}

	// constant buffer
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(ConstantBuffer);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		HR(device->CreateBuffer(&desc, nullptr, &mHierarchicalRayMarchingCB));
	}

	// blue noise
	{
		auto CreateStructuredBufferSRV = [device](const std::vector<UINT>& elements, ID3D11Buffer*& buffer, ID3D11ShaderResourceView*& srv)
		{
			// buffer
			{
				D3D11_BUFFER_DESC desc;
				desc.Usage = D3D11_USAGE_DEFAULT;
				desc.ByteWidth = sizeof(UINT) * elements.size();
				desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				desc.CPUAccessFlags = 0;
				desc.StructureByteStride = sizeof(UINT);
				desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

				D3D11_SUBRESOURCE_DATA data;
				data.pSysMem = elements.data();
				data.SysMemPitch = 0;
				data.SysMemSlicePitch = 0;

				HR(device->CreateBuffer(&desc, &data, &buffer));
			}

			// SRV
			{
				D3D11_SHADER_RESOURCE_VIEW_DESC desc;
				desc.Format = DXGI_FORMAT_UNKNOWN;
				desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
				desc.Buffer.FirstElement = 0;
				desc.Buffer.NumElements = elements.size();

				HR(device->CreateShaderResourceView(buffer, &desc, &srv));
			}
		};

		// sobol buffer
		CreateStructuredBufferSRV(sobol_256spp_256d, mSobolBuffer, mSobolSRV);
		// scrambling tile buffer
		CreateStructuredBufferSRV(scramblingTile, mScramblingTileBuffer, mScramblingTileSRV);
		// ranking tile buffer
		CreateStructuredBufferSRV(rankingTile, mRankingTileBuffer, mRankingTileSRV);
	}
}

void SSSR::OnResize(ID3D11Device* device, UINT width, UINT height)
{
	mWidth = width;
	mHeight = height;

	// hierarchical depth buffer UAV and SRV
	{
		SafeRelease(mHierarchicalDepthBufferSRV);

		for (ID3D11UnorderedAccessView* uav : mHierarchicalDepthBufferUAV)
		{
			SafeRelease(uav);
		}

		D3D11_TEXTURE2D_DESC TextureDesc;
		TextureDesc.Width = mWidth;
		TextureDesc.Height = mHeight;
		TextureDesc.MipLevels = std::ceil(std::log2(std::min(mWidth, mHeight)));;
		TextureDesc.ArraySize = 1;
		TextureDesc.Format = DXGI_FORMAT_R32_FLOAT;
		TextureDesc.SampleDesc.Count = 1;
		TextureDesc.SampleDesc.Quality = 0;
		TextureDesc.Usage = D3D11_USAGE_DEFAULT;
		TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		TextureDesc.CPUAccessFlags = 0;
		TextureDesc.MiscFlags = 0;

		ID3D11Texture2D* texture = nullptr;
		HR(device->CreateTexture2D(&TextureDesc, 0, &texture));

		HR(device->CreateShaderResourceView(texture, nullptr, &mHierarchicalDepthBufferSRV));

		mHierarchicalDepthBufferUAV.resize(TextureDesc.MipLevels);

		for (UINT i = 0; i < mHierarchicalDepthBufferUAV.size(); ++i)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
			desc.Format = TextureDesc.Format;
			desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipSlice = i;

			HR(device->CreateUnorderedAccessView(texture, &desc, &mHierarchicalDepthBufferUAV[i]));
		}

		SafeRelease(texture);
	}

	// reflections map UAV and SRV
	{
		SafeRelease(mUAV);
		SafeRelease(mSRV);

		D3D11_TEXTURE2D_DESC desc;
		desc.Width = mWidth;
		desc.Height = mHeight;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		ID3D11Texture2D* texture = nullptr;
		HR(device->CreateTexture2D(&desc, nullptr, &texture));

		HR(device->CreateUnorderedAccessView(texture, nullptr, &mUAV));
		HR(device->CreateShaderResourceView(texture, nullptr, &mSRV));

		SafeRelease(texture);
	}

	mDebugQuad.OnResize(mWidth, mHeight, (float)mWidth / (float)mHeight);
}

void SSSR::ComputeHierarchicalDepthBuffer(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11ShaderResourceView* depth)
{
	context->CSSetShader(mComputeHierarchicalDepthBufferCS, nullptr, 0);

	// bind SRV and UAV
	{
		context->CSSetShaderResources(0, 1, &depth);
		context->CSSetUnorderedAccessViews(0, 1, &mHierarchicalDepthBufferUAV, nullptr);
	}

	// clear UAV
	{
		const FLOAT values[] = { 0, 0, 0, 0 };
		context->ClearUnorderedAccessViewFloat(mHierarchicalDepthBufferUAV, values);
	}

	// dispatch
	{
		auto RoundedDivide = [](float value, float divisor) -> UINT
		{
			return (value + divisor - 1) / divisor;
		};

		UINT GroupsX = RoundedDivide(mWidth, 8u);
		UINT GroupsY = RoundedDivide(mHeight, 8u);

		context->Dispatch(GroupsX, GroupsY, 1);
	}

	// unbind SRV and UAV
	{
		ID3D11ShaderResourceView* const NullSRVs[1] = { nullptr };
		context->CSSetShaderResources(0, 1, NullSRVs);

		ID3D11UnorderedAccessView* const NullUAVs[1] = { nullptr };
		context->CSSetUnorderedAccessViews(0, 1, NullUAVs, nullptr);
	}
}

void SSSR::draw(ID3D11DeviceContext* context, const CameraObject& camera, ID3D11ShaderResourceView* LightPass, ID3D11ShaderResourceView* DepthBufferHierarchy, ID3D11ShaderResourceView* normals, UINT FrameIndex)
{
	const FLOAT values[] = { 0, 0, 0, 0 };
	context->ClearUnorderedAccessViewFloat(mUAV, values);

	// bind compute shader
	context->CSSetShader(mHierarchicalRayMarchingCS, nullptr, 0);

	// update and bind constant buffer
	{
		ConstantBuffer buffer;

		XMMATRIX view = XMLoadFloat4x4(&camera.mView);
		XMStoreFloat4x4(&buffer.view, view);

		{
			XMVECTOR det = XMMatrixDeterminant(view);
			XMStoreFloat4x4(&buffer.ViewInverse, XMMatrixInverse(&det, view));
		}

		XMMATRIX proj = camera.mProj;
		XMStoreFloat4x4(&buffer.proj, proj);

		{
			XMVECTOR det = XMMatrixDeterminant(proj);
			XMStoreFloat4x4(&buffer.ProjInverse, XMMatrixInverse(&det, proj));
		}

		{
			XMMATRIX ViewProj = view * proj;

			XMVECTOR det = XMMatrixDeterminant(ViewProj);
			XMStoreFloat4x4(&buffer.ViewProjInverse, XMMatrixInverse(&det, ViewProj));
		}

		buffer.FrameIndex = FrameIndex;

		context->UpdateSubresource(mHierarchicalRayMarchingCB, 0, 0, &buffer, 0, 0);

		context->CSSetConstantBuffers(0, 1, &mHierarchicalRayMarchingCB);
	}

	// bind SRVs
	{
		context->CSSetShaderResources(0, 1, &LightPass);
		context->CSSetShaderResources(1, 1, &DepthBufferHierarchy);
		context->CSSetShaderResources(2, 1, &normals);

		//context->UpdateSubresource(mSobolBuffer, 0, nullptr, sobol_256spp_256d.data(), 0, 0);

		context->CSSetShaderResources(5, 1, &mSobolSRV);
		context->CSSetShaderResources(6, 1, &mScramblingTileSRV);
		context->CSSetShaderResources(7, 1, &mRankingTileSRV);
	}

	// bind UAVs
	{
		context->CSSetUnorderedAccessViews(0, 1, &mUAV, nullptr);

		const UINT values[] = { 0, 0, 0, 0 };
		context->ClearUnorderedAccessViewUint(mUAV, values);
	}

	auto RoundedDivide = [](float value, float divisor) -> UINT
	{
		return (value + divisor - 1) / divisor;
	};

	UINT GroupsX = RoundedDivide(mWidth,  8u);
	UINT GroupsY = RoundedDivide(mHeight, 8u);

	context->Dispatch(GroupsX, GroupsY, 1);

	// unbind SRVs
	{
		ID3D11ShaderResourceView* const NullSRVs[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
		context->CSSetShaderResources(0, 6, NullSRVs);
	}

	// unbind UAVs
	{
		ID3D11UnorderedAccessView* const NullUAV[1] = { nullptr };
		context->CSSetUnorderedAccessViews(0, 1, NullUAV, nullptr);
	}
}