#include "SSSR.h"

SSSR::SSSR() :
	mComputeShader(nullptr),
	mUAV(nullptr),
	mSRV(nullptr),
	mConstantBuffer(nullptr)
{}

SSSR::~SSSR()
{
	SafeRelease(mComputeShader);
	SafeRelease(mUAV);
	SafeRelease(mSRV);
	SafeRelease(mConstantBuffer);
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

void SSSR::draw(ID3D11DeviceContext* context, const CameraObject& camera, ID3D11ShaderResourceView* NormalDepthSRV)
{
	// bind compute shader
	context->CSSetShader(mComputeShader, nullptr, 0);

	// update and bind constant buffer
	{
		ConstantBuffer buffer;

		buffer.view = camera.mView;
		XMStoreFloat4x4(&buffer.proj, camera.mProj);


		//XMMATRIX view = XMLoadFloat4x4(&camera.mView);
		//XMVECTOR det = XMMatrixDeterminant(view);
		//XMStoreFloat4x4(&buffer.ViewInverse, XMMatrixInverse(&det, view));

		context->UpdateSubresource(mConstantBuffer, 0, 0, &buffer, 0, 0);

		context->CSSetConstantBuffers(0, 1, &mConstantBuffer);
	}

	// bind normal-depth SRV
	{
		context->CSSetShaderResources(0, 1, &NormalDepthSRV);
	}

	// bind UAV
	{

		context->CSSetUnorderedAccessViews(0, 1, &mUAV, nullptr);

		const UINT values[] = { 0, 0, 0, 0 };
		context->ClearUnorderedAccessViewUint(mUAV, values);
	}

	UINT GroupsX = std::ceil(mWidth / 256.0f);
	context->Dispatch(GroupsX, mHeight, 1);

	// unbind SRVs
	{
		ID3D11ShaderResourceView* const NullSRVs[1] = { nullptr };
		context->CSSetShaderResources(0, 1, NullSRVs);
	}

	// unbind UAV
	{
		ID3D11UnorderedAccessView* const NullUAV[1] = { nullptr };
		context->CSSetUnorderedAccessViews(0, 1, NullUAV, nullptr);
	}
}