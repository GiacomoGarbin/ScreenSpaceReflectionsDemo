#include "SSPR.h"

SSPR::SSPR() :
	mReflectionsMapUAV(nullptr),
	mReflectionsMapSRV(nullptr),
	//mComputeShader(nullptr),
	mPixelShader(nullptr),
	mConstantBuffer(nullptr),
	mSamplerState(nullptr)
{}

SSPR::~SSPR()
{
	SafeRelease(mReflectionsMapUAV);
	SafeRelease(mReflectionsMapSRV);
	//SafeRelease(mComputeShader);
	SafeRelease(mPixelShader);
	SafeRelease(mConstantBuffer);
	SafeRelease(mSamplerState);
}

void SSPR::Init(ID3D11Device* device, UINT width, UINT height, float FieldOfViewY, float NearZ, float FarZ)
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

	//// compute shader
	//{
	//	std::wstring path = L"SSPRReflectionsMapComputeCS.hlsl";

	//	ID3DBlob* pCode;
	//	HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "cs_5_0", 0, 0, &pCode, nullptr));
	//	HR(device->CreateComputeShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mComputeShader));
	//}

	// reflections map vertex shader
	{
		std::wstring path = L"SSPRReflectionsMapComputeVS.hlsl";

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

	// reflections map pixel shader
	{
		std::wstring path = L"SSPRReflectionsMapComputePS.hlsl";

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
		HR(device->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mReflectionsMapQuad.mPixelShader));
	}

	// reflective surface pixel shader
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
		defines.push_back({ "ENABLE_SSPR",            "1" });
		defines.push_back({ "ENABLE_FOG",             "0" });
		defines.push_back({ nullptr, nullptr });

		ID3DBlob* pCode;
		HR(D3DCompileFromFile(path.c_str(), defines.data(), nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
		HR(device->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mPixelShader));
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
}

void SSPR::OnResize(ID3D11Device* device, UINT width, UINT height, float FieldOfViewY, float NearZ, float FarZ)
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

	// reflections map UAV and SRV
	{
		D3D11_TEXTURE2D_DESC desc;
		desc.Width = mWidth; // / 2;
		desc.Height = mHeight; // / 2;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R32_UINT;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS| D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		SafeRelease(mReflectionsMapUAV);
		SafeRelease(mReflectionsMapSRV);

		ID3D11Texture2D* texture = nullptr;
		HR(device->CreateTexture2D(&desc, nullptr, &texture));

		HR(device->CreateUnorderedAccessView(texture, nullptr, &mReflectionsMapUAV));
		HR(device->CreateShaderResourceView(texture, nullptr, &mReflectionsMapSRV));

		SafeRelease(texture);
	}

	mDebugQuad.OnResize(mWidth, mHeight, (float)mWidth / (float)mHeight);
}

//void SSPR::ComputeReflectionsMap(ID3D11DeviceContext* context, const CameraObject& camera, ID3D11ShaderResourceView* NormalDepthSRV)
//{
//	// bind compute shader
//	context->CSSetShader(mComputeShader, nullptr, 0);
//
//	// update and bind constant buffer
//	{
//		ConstantBuffer buffer;
//
//		for (UINT i = 0; i < 4; ++i) buffer.FrustumFarCorner[i] = mFrustumFarCorner[i];
//		XMStoreFloat4x4(&buffer.proj, camera.mProj);
//
//		buffer.view = camera.mView;
//
//		XMMATRIX view = XMLoadFloat4x4(&camera.mView);
//		XMVECTOR det = XMMatrixDeterminant(view);
//		XMStoreFloat4x4(&buffer.ViewInverse, XMMatrixInverse(&det, view));
//
//		XMFLOAT4 plane = XMFLOAT4(0, 1, 0, 0);
//		XMStoreFloat4x4(&buffer.reflect, XMMatrixReflect(XMLoadFloat4(&plane)));
//
//		context->UpdateSubresource(mConstantBuffer, 0, 0, &buffer, 0, 0);
//
//		context->CSSetConstantBuffers(0, 1, &mConstantBuffer);
//	}
//
//	// bind reflections map UAV
//	context->CSSetUnorderedAccessViews(0, 1, &mReflectionsMapUAV, nullptr);
//
//	UINT GroupsX = std::ceil(mWidth / 256.0f);
//	context->Dispatch(GroupsX, mHeight, 1);
//
//	// unbind UAV
//	{
//		ID3D11UnorderedAccessView* const NullUAV[1] = { nullptr };
//		context->CSSetUnorderedAccessViews(0, 1, NullUAV, nullptr);
//	}
//}

void SSPR::ComputeReflectionsMap(ID3D11DeviceContext* context, const CameraObject& camera, ID3D11ShaderResourceView* NormalDepthSRV)
{
	// bind reflections map UAV
	context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 1, 1, &mReflectionsMapUAV, nullptr);

	const UINT values[] = { 0, 0, 0, 0 };
	context->ClearUnorderedAccessViewUint(mReflectionsMapUAV, values);
	
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

	// update and bind constant buffer
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

	// bind normal-depth SRV
	{
		context->PSSetShaderResources(0, 1, &NormalDepthSRV);
	}

//
//	UINT GroupsX = std::ceil(mWidth / 256.0f);
//	context->Dispatch(GroupsX, mHeight, 1);
//
//	// unbind UAV
//	{
//		ID3D11UnorderedAccessView* const NullUAV[1] = { nullptr };
//		context->CSSetUnorderedAccessViews(0, 1, NullUAV, nullptr);
//	}

	// bind sampler state
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

	// unbind reflections map UAV
	context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 0, nullptr, nullptr);
}