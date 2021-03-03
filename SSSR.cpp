#include "SSSR.h"
#include "BlueNoise.h"

SSSR::SSSR() :
	mComputeShader(nullptr),
	mUAV(nullptr),
	mSRV(nullptr),
	mConstantBuffer(nullptr),

	mSobolBuffer(nullptr),
	mRankingTileBuffer(nullptr),
	mScramblingTileBuffer(nullptr),

	mSobolSRV(nullptr),
	mRankingTileSRV(nullptr),
	mScramblingTileSRV(nullptr)
{}

SSSR::~SSSR()
{
	SafeRelease(mComputeShader);
	SafeRelease(mUAV);
	SafeRelease(mSRV);
	SafeRelease(mConstantBuffer);

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

	// compute shader
	{
		std::wstring path = L"SSSR_CS.hlsl";

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "cs_5_0", 0, 0, &pCode, nullptr));
		HR(device->CreateComputeShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mComputeShader));
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

		HR(device->CreateBuffer(&desc, nullptr, &mConstantBuffer));
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

	// reflections map UAV and SRV
	{
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

		SafeRelease(mUAV);
		SafeRelease(mSRV);

		ID3D11Texture2D* texture = nullptr;
		HR(device->CreateTexture2D(&desc, nullptr, &texture));

		HR(device->CreateUnorderedAccessView(texture, nullptr, &mUAV));
		HR(device->CreateShaderResourceView(texture, nullptr, &mSRV));

		SafeRelease(texture);
	}

	mDebugQuad.OnResize(mWidth, mHeight, (float)mWidth / (float)mHeight);
}

void SSSR::draw(ID3D11DeviceContext* context, const CameraObject& camera, ID3D11ShaderResourceView* LightPass, ID3D11ShaderResourceView* DepthBufferHierarchy, ID3D11ShaderResourceView* normals, UINT FrameIndex)
{
	const FLOAT values[] = { 0, 0, 0, 0 };
	context->ClearUnorderedAccessViewFloat(mUAV, values);

	// bind compute shader
	context->CSSetShader(mComputeShader, nullptr, 0);

	// update and bind constant buffer
	{
		ConstantBuffer buffer;

		XMMATRIX view = XMLoadFloat4x4(&camera.mView);

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

		context->UpdateSubresource(mConstantBuffer, 0, 0, &buffer, 0, 0);

		context->CSSetConstantBuffers(0, 1, &mConstantBuffer);
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