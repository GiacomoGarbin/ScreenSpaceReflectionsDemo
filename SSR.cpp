#include "SSR.h"

SSR::SSR() :
	mReflectionsMapRTV(nullptr),
	mReflectionsMapSRV(nullptr),
	//mVertexShader(nullptr),
	//mInputLayout(nullptr),
	mPixelShader(nullptr),
	mSamplerState(nullptr),
	mConstantBuffer(nullptr)
{}

SSR::~SSR()
{
	SafeRelease(mReflectionsMapRTV);
	SafeRelease(mReflectionsMapSRV);
	//SafeRelease(mVertexShader);
	//SafeRelease(mInputLayout);
	SafeRelease(mPixelShader);
	SafeRelease(mSamplerState);
	SafeRelease(mConstantBuffer);
}

void SSR::Init(ID3D11Device* device, UINT width, UINT height, float FieldOfViewY, float NearZ, float FarZ)
{
	OnResize(device, width, height, FieldOfViewY, NearZ, FarZ);

	GeometryGenerator::CreateScreenQuad(mReflectionsMapQuad.mMesh);

	// store far plane frustum corner indices in normal.x
	mReflectionsMapQuad.mMesh.mVertices[0].mNormal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mReflectionsMapQuad.mMesh.mVertices[1].mNormal = XMFLOAT3(1.0f, 0.0f, 0.0f);
	mReflectionsMapQuad.mMesh.mVertices[2].mNormal = XMFLOAT3(2.0f, 0.0f, 0.0f);
	mReflectionsMapQuad.mMesh.mVertices[3].mNormal = XMFLOAT3(3.0f, 0.0f, 0.0f);

	// vertex buffer
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(GeometryGenerator::Vertex) * mReflectionsMapQuad.mMesh.mVertices.size();
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA InitData;
		InitData.pSysMem = mReflectionsMapQuad.mMesh.mVertices.data();
		InitData.SysMemPitch = 0;
		InitData.SysMemSlicePitch = 0;

		HR(device->CreateBuffer(&desc, &InitData, &mReflectionsMapQuad.mVertexBuffer));
	}

	// index buffer
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(UINT) * mReflectionsMapQuad.mMesh.mIndices.size();
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA InitData;
		InitData.pSysMem = mReflectionsMapQuad.mMesh.mIndices.data();
		InitData.SysMemPitch = 0;
		InitData.SysMemSlicePitch = 0;

		HR(device->CreateBuffer(&desc, &InitData, &mReflectionsMapQuad.mIndexBuffer));
	}

	// VS
	{
		std::wstring path = L"SSRReflectionsMapComputeVS.hlsl";

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
		HR(device->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mReflectionsMapQuad.mVertexShader));

		// input layout
		{
			std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
			};

			HR(device->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &mReflectionsMapQuad.mInputLayout));
		}
	}

	// PS
	{
		std::wstring path = L"SSRReflectionsMapComputePS.hlsl";

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
		HR(device->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mReflectionsMapQuad.mPixelShader));
	}

	// PS
	{
		std::wstring path = L"PS.hlsl";

		std::vector<D3D_SHADER_MACRO> defines;
		defines.push_back({ "ENABLE_TEXTURE",         "1" });
		defines.push_back({ "ENABLE_SPHERE_TEXCOORD", "0" });
		defines.push_back({ "ENABLE_NORMAL_MAPPING",  "1" });
		defines.push_back({ "ENABLE_ALPHA_CLIPPING",  "0" });
		//defines.push_back({ "ENABLE_LIGHTING",       "1" });
		defines.push_back({ "ENABLE_REFLECTION",      "0" });
		defines.push_back({ "ENABLE_SSR",             "1" });
		defines.push_back({ "ENABLE_SSPR",            "0" });
		defines.push_back({ "ENABLE_FOG",             "0" });
		defines.push_back({ nullptr, nullptr });

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
		HR(device->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mPixelShader));
	}

	// sampler state
	{
		D3D11_SAMPLER_DESC desc;
		desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.MipLODBias = 0;
		desc.MaxAnisotropy = 1;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		FLOAT BorderColor[4] = { 0.0f, 0.0f, 0.0f, 1e5f };
		CopyMemory(desc.BorderColor, BorderColor, sizeof(desc.BorderColor));
		desc.MinLOD = 0;
		desc.MaxLOD = 0;

		HR(device->CreateSamplerState(&desc, &mSamplerState));
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

void SSR::OnResize(ID3D11Device* device, UINT width, UINT height, float FieldOfViewY, float NearZ, float FarZ)
{
	mWidth = width;
	mHeight = height;

	// render to reflections map at half the resolution
	mReflectionsMapViewport.TopLeftX = 0.0f;
	mReflectionsMapViewport.TopLeftY = 0.0f;
	mReflectionsMapViewport.Width = mWidth; // / 2.0f;
	mReflectionsMapViewport.Height = mHeight; // / 2.0f;
	mReflectionsMapViewport.MinDepth = 0.0f;
	mReflectionsMapViewport.MaxDepth = 1.0f;

	// frustum far corners
	{
		float aspect = (float)mWidth / (float)mHeight;

		float HalfHeight = FarZ * tanf(0.5f * FieldOfViewY);
		float HalfWidth = aspect * HalfHeight;

		mFrustumFarCorner[0] = XMFLOAT4(-HalfWidth, -HalfHeight, NearZ, FarZ);
		mFrustumFarCorner[1] = XMFLOAT4(-HalfWidth, +HalfHeight, NearZ, FarZ);
		mFrustumFarCorner[2] = XMFLOAT4(+HalfWidth, +HalfHeight, NearZ, FarZ);
		mFrustumFarCorner[3] = XMFLOAT4(+HalfWidth, -HalfHeight, NearZ, FarZ);
	}

	// reflections map RTV and SRV
	{
		// render to ambient map at half the resolution
		D3D11_TEXTURE2D_DESC desc;
		desc.Width = mWidth; // / 2;
		desc.Height = mHeight; // / 2;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		SafeRelease(mReflectionsMapRTV);
		SafeRelease(mReflectionsMapSRV);

		ID3D11Texture2D* texture = nullptr;
		HR(device->CreateTexture2D(&desc, nullptr, &texture));

		HR(device->CreateRenderTargetView(texture, nullptr, &mReflectionsMapRTV));
		HR(device->CreateShaderResourceView(texture, nullptr, &mReflectionsMapSRV));

		SafeRelease(texture);
	}

	mDebugQuad.OnResize(mWidth, mHeight, (float)mWidth / (float)mHeight);
}

void SSR::ComputeReflectionsMap(ID3D11DeviceContext* context, const CameraObject& camera, ID3D11ShaderResourceView* NormalDepthSRV)
{
	// bind the reflections map as the render target
	// do not bind a depth/stencil buffer -> no depth test is performed
	context->OMSetRenderTargets(1, &mReflectionsMapRTV, nullptr);
	context->ClearRenderTargetView(mReflectionsMapRTV, Colors::Black);
	context->RSSetViewports(1, &mReflectionsMapViewport);

	// shaders
	context->VSSetShader(mReflectionsMapQuad.mVertexShader.Get(), nullptr, 0);
	context->PSSetShader(mReflectionsMapQuad.mPixelShader.Get(), nullptr, 0);
	// input layout
	context->IASetInputLayout(mReflectionsMapQuad.mInputLayout.Get());

	// primitive topology
	context->IASetPrimitiveTopology(mReflectionsMapQuad.mPrimitiveTopology);

	// vertex and index buffers
	{
		UINT stride = sizeof(GeometryGenerator::Vertex);
		UINT offset = 0;

		context->IASetVertexBuffers(0, 1, mReflectionsMapQuad.mVertexBuffer.GetAddressOf(), &stride, &offset);
		context->IASetIndexBuffer(mReflectionsMapQuad.mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	}

	// update and bind CBs
	{
		ConstantBuffer buffer;

		for (UINT i = 0; i < 4; ++i) buffer.FrustumFarCorner[i] = mFrustumFarCorner[i];
		XMStoreFloat4x4(&buffer.proj, camera.mProj);

		buffer.view = camera.mView;

		XMMATRIX view = XMLoadFloat4x4(&camera.mView);
		XMVECTOR det = XMMatrixDeterminant(view);
		XMStoreFloat4x4(&buffer.ViewInverse, XMMatrixInverse(&det, view));

		XMFLOAT4 plane = XMFLOAT4(0, 1, 0, 0);
		XMStoreFloat4x4(&buffer.reflect, XMMatrixReflect(XMLoadFloat4(&plane)));

		context->UpdateSubresource(mConstantBuffer, 0, 0, &buffer, 0, 0);

		context->VSSetConstantBuffers(0, 1, &mConstantBuffer);
		context->PSSetConstantBuffers(0, 1, &mConstantBuffer);
	}

	// bind SRVs
	{
		context->PSSetShaderResources(0, 1, &NormalDepthSRV);
	}

	// bind SSs
	{
		context->PSSetSamplers(2, 1, &mSamplerState);
	}

	// rasterizer, blend and depth-stencil states
	{
		FLOAT BlendFactor[] = { 0, 0, 0, 0 };

		context->RSSetState(mReflectionsMapQuad.mRasterizerState.Get());
		context->OMSetBlendState(mReflectionsMapQuad.mBlendState.Get(), BlendFactor, 0xFFFFFFFF);
		context->OMSetDepthStencilState(mReflectionsMapQuad.mDepthStencilState.Get(), mReflectionsMapQuad.mStencilRef);
	}

	// draw call
	context->DrawIndexed(mReflectionsMapQuad.mMesh.mIndices.size(), mReflectionsMapQuad.mIndexStart, mReflectionsMapQuad.mVertexStart);

	// unbind SRVs
	{
		ID3D11ShaderResourceView* const NullSRVs[1] = { nullptr };
		context->PSSetShaderResources(0, 1, NullSRVs);
	}

	// unbind SSs
	{
		ID3D11SamplerState* const NullSSs[1] = { nullptr };
		context->PSSetSamplers(2, 1, NullSSs);
	}

	// unbind render target
	context->OMSetRenderTargets(0, nullptr, nullptr);
}