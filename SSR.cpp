#include "SSR.h"

SSR::SSR() :
	mReflectionsMapRTV(nullptr),
	mReflectionsMapSRV(nullptr),
	//mVertexShader(nullptr),
	//mInputLayout(nullptr),
	mPixelShader(nullptr),
	mSamplerState(nullptr),
	mConstantBuffer(nullptr),

	mCopyDepthBufferCS(nullptr),
	//mCopyDepthBufferUAV(nullptr),
	//mCopyDepthBufferSRV(nullptr),

	//mHierarchicalDepthBufferCS(nullptr),

	//mHierarchicalDepthBufferVS(nullptr),
	//mHierarchicalDepthBufferIL(nullptr),
	//mHierarchicalDepthBufferPS(nullptr),

	//mPingPongTextureRTV{ nullptr, nullptr },
	//mPingPongTextureSRV{ nullptr, nullptr },
	//mPingPongTextureUAV{ nullptr, nullptr },
	
	mHierarchicalDepthBufferSS(nullptr),

	mHierarchicalDepthBufferSRV(nullptr)
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

	SafeRelease(mCopyDepthBufferCS);
	//SafeRelease(mCopyDepthBufferUAV);
	//SafeRelease(mCopyDepthBufferSRV);

	//SafeRelease(mHierarchicalDepthBufferCS);

	//SafeRelease(mHierarchicalDepthBufferVS);
	//SafeRelease(mHierarchicalDepthBufferIL);
	//SafeRelease(mHierarchicalDepthBufferPS);

	//SafeRelease(mPingPongTextureRTV[0]);
	//SafeRelease(mPingPongTextureRTV[1]);
	//SafeRelease(mPingPongTextureSRV[0]);
	//SafeRelease(mPingPongTextureSRV[1]);
	//SafeRelease(mPingPongTextureUAV[0]);
	//SafeRelease(mPingPongTextureUAV[1]);

	SafeRelease(mHierarchicalDepthBufferSS);

	//for (ID3D11RenderTargetView* rtv : mHierarchicalDepthBufferRTV)
	//{
	//	SafeRelease(rtv);
	//}

	SafeRelease(mHierarchicalDepthBufferSRV);
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

	// hierarchical depth buffer
	{
		// compute shader
		{
			std::wstring path = L"SSRCopyDepthBufferCS.hlsl";

			ID3DBlob* blob;
			HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "cs_5_0", 0, 0, &blob, nullptr));
			HR(device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &mCopyDepthBufferCS));
		}

		//// compute shader
		//{
		//	std::wstring path = L"SSRComputeHierarchicalDepthBufferCS.hlsl";

		//	ID3DBlob* blob;
		//	HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "cs_5_0", 0, 0, &blob, nullptr));
		//	HR(device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &mHierarchicalDepthBufferCS));
		//}

		float AspectRatio = (float)mWidth / (float)mHeight;
		mHierarchicalDepthBufferQuad.Init(device, mWidth, mHeight, AspectRatio, DebugQuad::WindowCorner::FullWindow, AspectRatio);

		// vertex shader
		{
			std::wstring path = L"SSRComputeHierarchicalDepthBufferVS.hlsl";

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &pCode, nullptr));
			//HR(device->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mHierarchicalDepthBufferVS));
			HR(device->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mHierarchicalDepthBufferQuad.mVertexShader));

			// input layout
			{
				std::vector<D3D11_INPUT_ELEMENT_DESC> desc =
				{
					{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
					{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
				};

				//HR(device->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &mHierarchicalDepthBufferIL));
				HR(device->CreateInputLayout(desc.data(), desc.size(), pCode->GetBufferPointer(), pCode->GetBufferSize(), &mHierarchicalDepthBufferQuad.mInputLayout));
			}
		}

		// pixel shader
		{
			std::wstring path = L"SSRComputeHierarchicalDepthBufferPS.hlsl";

			ID3DBlob* pCode;
			HR(D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "ps_5_0", 0, 0, &pCode, nullptr));
			//HR(device->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mHierarchicalDepthBufferPS));
			HR(device->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, &mHierarchicalDepthBufferQuad.mPixelShader));
		}

		// sampler state
		{
			D3D11_SAMPLER_DESC desc;
			desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.MipLODBias = 0;
			desc.MaxAnisotropy = 1;
			desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			ZeroMemory(desc.BorderColor, sizeof(desc.BorderColor));
			desc.MinLOD = 0;
			desc.MaxLOD = 0;

			HR(device->CreateSamplerState(&desc, &mHierarchicalDepthBufferSS));

			//mContext->DSSetSamplers(0, 1, &mSamplerState);
			//mContext->PSSetSamplers(0, 1, &mSamplerState);
		}

		// SRVs and UAVs
		{
			mMipLevels = std::ceil(std::log2(std::min(mWidth, mHeight)));

			D3D11_TEXTURE2D_DESC desc;
			desc.Width = mWidth;
			desc.Height = mHeight;
			desc.MipLevels = mMipLevels; // <- to be computed
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R32_FLOAT;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;

			ID3D11Texture2D* texture = nullptr;
			HR(device->CreateTexture2D(&desc, 0, &texture));

			//mHierarchicalDepthBufferRTV.resize(mMipLevels);
			//for (UINT i = 0; i < mHierarchicalDepthBufferRTV.size(); ++i)
			//{
			//	D3D11_RENDER_TARGET_VIEW_DESC RenderTargetDesc;
			//	RenderTargetDesc.Format = desc.Format;
			//	RenderTargetDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			//	RenderTargetDesc.Texture2D.MipSlice = i;

			//	HR(device->CreateRenderTargetView(texture, &RenderTargetDesc, &mHierarchicalDepthBufferRTV[i]));
			//}

			HR(device->CreateShaderResourceView(texture, nullptr, &mHierarchicalDepthBufferSRV));

			SafeRelease(texture);
		}
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
		desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // DXGI_FORMAT_R8G8B8A8_UNORM;
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

		{
			XMMATRIX ViewProj = view * camera.mProj;
			XMStoreFloat4x4(&buffer.ViewProj, ViewProj);

			XMVECTOR det = XMMatrixDeterminant(ViewProj);
			XMStoreFloat4x4(&buffer.ViewProjInverse, XMMatrixInverse(&det, ViewProj));
		}

		buffer.CameraPosition = camera.mPosition;

		context->UpdateSubresource(mConstantBuffer, 0, 0, &buffer, 0, 0);

		context->VSSetConstantBuffers(0, 1, &mConstantBuffer);
		context->PSSetConstantBuffers(0, 1, &mConstantBuffer);
	}

	// bind SRVs
	{
		ID3D11ShaderResourceView* const SRVs[2] =
		{
			NormalDepthSRV,
			mHierarchicalDepthBufferSRV
		};
		context->PSSetShaderResources(0, 2, SRVs);
	}

	// bind SSs
	{
		ID3D11SamplerState* const SSs[2] =
		{
			mSamplerState,
			mHierarchicalDepthBufferSS
		};
		context->PSSetSamplers(2, 2, SSs);
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
		ID3D11ShaderResourceView* const NullSRVs[2] = { nullptr, nullptr };
		context->PSSetShaderResources(0, 2, NullSRVs);
	}

	// unbind SSs
	{
		ID3D11SamplerState* const NullSSs[2] = { nullptr, nullptr };
		context->PSSetSamplers(2, 2, NullSSs);
	}

	// unbind render target
	context->OMSetRenderTargets(0, nullptr, nullptr);
}

//void SSR::ComputeHierarchicalDepthBuffer(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11ShaderResourceView* NormalDepthSRV)
//{
//	UINT HalfWidth;
//	UINT HalfHeight;
//
//	ID3D11ShaderResourceView* const NullSRV = nullptr;
//	ID3D11UnorderedAccessView* const NullUAV = nullptr;
//
//	// get texture dimensions
//	{
//		ID3D11Resource* resource = nullptr;
//		NormalDepthSRV->GetResource(&resource);
//
//		ID3D11Texture2D* DepthTexture = nullptr;
//		HR(resource->QueryInterface(IID_ID3D11Texture2D, (void**)&DepthTexture));
//
//		D3D11_TEXTURE2D_DESC desc;
//		DepthTexture->GetDesc(&desc);
//
//		HalfWidth = desc.Width / 2;
//		HalfHeight = desc.Height / 2;
//
//		// copy depth buffer
//		{
//			{
//				//SafeRelease(mCopyDepthBufferSRV);
//				//SafeRelease(mCopyDepthBufferUAV);
//				SafeRelease(mPingPongTextureSRV[0]);
//				SafeRelease(mPingPongTextureUAV[0]);
//
//				D3D11_TEXTURE2D_DESC CopyDesc;
//
//				CopyDesc = desc;
//				CopyDesc.Format = DXGI_FORMAT_R32_FLOAT;
//				CopyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
//
//				ID3D11Texture2D* texture = nullptr;
//				HR(device->CreateTexture2D(&CopyDesc, 0, &texture));
//
//				//HR(device->CreateShaderResourceView(texture, nullptr, &mCopyDepthBufferSRV));
//				//HR(device->CreateUnorderedAccessView(texture, nullptr, &mCopyDepthBufferUAV));
//				HR(device->CreateShaderResourceView(texture, nullptr, &mPingPongTextureSRV[0]));
//				HR(device->CreateUnorderedAccessView(texture, nullptr, &mPingPongTextureUAV[0]));
//
//				SafeRelease(texture);
//			}
//
//			context->CSSetShader(mCopyDepthBufferCS, nullptr, 0);
//
//			// bind SRV and UAV
//			context->CSSetShaderResources(0, 1, &NormalDepthSRV);
//			//context->CSSetUnorderedAccessViews(0, 1, &mCopyDepthBufferUAV, nullptr);
//			context->CSSetUnorderedAccessViews(0, 1, &mPingPongTextureUAV[0], nullptr);
//
//			UINT GroupsX = std::ceil(desc.Width / 256.0f);
//			context->Dispatch(GroupsX, desc.Height, 1);
//
//			// unbind SRV and UAV
//			context->CSSetShaderResources(0, 1, &NullSRV);
//			context->CSSetUnorderedAccessViews(0, 1, &NullUAV, nullptr);
//		}
//
//		//// 
//		//{
//		//	SafeRelease(mPingPongTextureSRV[0]);
//
//		//	D3D11_TEXTURE2D_DESC CopyDesc;
//		//	//CopyDesc.Width = width;
//		//	//CopyDesc.Height = height;
//		//	//CopyDesc.MipLevels = 1;
//		//	//CopyDesc.ArraySize = 1;
//		//	//CopyDesc.Format = DXGI_FORMAT_R32_FLOAT;
//		//	//CopyDesc.SampleDesc.Count = 1;
//		//	//CopyDesc.SampleDesc.Quality = 0;
//		//	//CopyDesc.Usage = D3D11_USAGE_DEFAULT;
//		//	//CopyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
//		//	//CopyDesc.CPUAccessFlags = 0;
//		//	//CopyDesc.MiscFlags = 0;
//
//		//	CopyDesc = desc;
//		//	CopyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
//
//		//	ID3D11Texture2D* texture = nullptr;
//		//	HR(device->CreateTexture2D(&CopyDesc, 0, &texture));
//
//		//	HR(device->CreateShaderResourceView(texture, nullptr, &mPingPongTextureSRV[0]));
//		//	//HR(device->CreateUnorderedAccessView(texture, nullptr, &mHierarchicalDepthBufferUAV));
//
//		//	context->CopyResource(texture, DepthTexture);
//
//		//	SafeRelease(texture);
//		//}
//
//		// 
//		{
//			SafeRelease(mPingPongTextureSRV[1]);
//			SafeRelease(mPingPongTextureUAV[1]);
//
//			D3D11_TEXTURE2D_DESC CopyDesc;
//			//CopyDesc.Width = width; // / 2;
//			//CopyDesc.Height = height; // / 2;
//			//CopyDesc.MipLevels = 1;
//			//CopyDesc.ArraySize = 1;
//			//CopyDesc.Format = DXGI_FORMAT_R32_FLOAT;
//			//CopyDesc.SampleDesc.Count = 1;
//			//CopyDesc.SampleDesc.Quality = 0;
//			//CopyDesc.Usage = D3D11_USAGE_DEFAULT;
//			//CopyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
//			//CopyDesc.CPUAccessFlags = 0;
//			//CopyDesc.MiscFlags = 0;
//
//			CopyDesc = desc;
//			CopyDesc.Width = HalfWidth;
//			CopyDesc.Height = HalfHeight;
//			CopyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
//
//			ID3D11Texture2D* texture = nullptr;
//			HR(device->CreateTexture2D(&CopyDesc, 0, &texture));
//
//			HR(device->CreateShaderResourceView(texture, nullptr, &mPingPongTextureSRV[1]));
//			HR(device->CreateUnorderedAccessView(texture, nullptr, &mPingPongTextureUAV[1]));
//
//			SafeRelease(texture);
//		}
//
//		SafeRelease(DepthTexture);
//		SafeRelease(resource);
//	}
//
//	{
//		context->CSSetShader(mHierarchicalDepthBufferCS, nullptr, 0);
//
//		// bind SRV and UAV
//		context->CSSetShaderResources(0, 1, &mPingPongTextureSRV[0]);
//		context->CSSetUnorderedAccessViews(0, 1, &mPingPongTextureUAV[1], nullptr);
//
//		UINT GroupsX = std::ceil(HalfWidth / 256.0f);
//		context->Dispatch(GroupsX, HalfHeight, 1);
//
//		ID3D11ShaderResourceView* const NullSRV = nullptr;
//		ID3D11UnorderedAccessView* const NullUAV = nullptr;
//
//		// unbind SRV and UAV
//		context->CSSetShaderResources(0, 1, &NullSRV);
//		context->CSSetUnorderedAccessViews(0, 1, &NullUAV, nullptr);
//	}
//
//	// copy
//	//CopyTexture(mCurrTextureSRV, mNextTextureUAV);
//}

void SSR::ComputeHierarchicalDepthBuffer(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11ShaderResourceView* NormalDepthSRV)
{
	ID3D11ShaderResourceView* const NullSRVs[1] = { nullptr };
	ID3D11UnorderedAccessView* const NullUAVs[1] = { nullptr };

	ID3D11RenderTargetView*    rtv = nullptr;
	ID3D11ShaderResourceView*  srv = nullptr;
	ID3D11UnorderedAccessView* uav = nullptr;

	ID3D11Resource* resource = nullptr;
	mHierarchicalDepthBufferSRV->GetResource(&resource);

	ID3D11Texture2D* HierarchicalDepthBufferTexture = nullptr;
	HR(resource->QueryInterface(IID_ID3D11Texture2D, (void**)&HierarchicalDepthBufferTexture));

	SafeRelease(resource);

	D3D11_TEXTURE2D_DESC HierarchicalDepthBufferTextureDesc;
	HierarchicalDepthBufferTexture->GetDesc(&HierarchicalDepthBufferTextureDesc);

	D3D11_VIEWPORT viewport;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = HierarchicalDepthBufferTextureDesc.Width;
	viewport.Height = HierarchicalDepthBufferTextureDesc.Height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	//XMVECTORF32 colors[] =
	//{
	//	Colors::Red,
	//	Colors::Green,
	//	Colors::Blue,
	//	Colors::Yellow,
	//	Colors::Magenta,
	//	Colors::Cyan,
	//};

	std::vector<XMVECTORF32> colors(mMipLevels, Colors::Transparent);

	// common to all draw calls
	{
		// shaders
		context->VSSetShader(mHierarchicalDepthBufferQuad.mVertexShader.Get(), nullptr, 0);
		context->PSSetShader(mHierarchicalDepthBufferQuad.mPixelShader.Get(), nullptr, 0);

		// input layout
		context->IASetInputLayout(mHierarchicalDepthBufferQuad.mInputLayout.Get());

		// primitive topology
		context->IASetPrimitiveTopology(mHierarchicalDepthBufferQuad.mPrimitiveTopology);

		// vertex and index buffers
		{
			UINT stride = sizeof(GeometryGenerator::Vertex);
			UINT offset = 0;

			context->IASetVertexBuffers(0, 1, mHierarchicalDepthBufferQuad.mVertexBuffer.GetAddressOf(), &stride, &offset);
			context->IASetIndexBuffer(mHierarchicalDepthBufferQuad.mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		}

		//// bind SSs
		//{
		//	context->PSSetSamplers(1, 0, &mHierarchicalDepthBufferSS);
		//}
	}

	// mip level 0
	{
		// UAV
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
			desc.Format = HierarchicalDepthBufferTextureDesc.Format;
			desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipSlice = 0;

			SafeRelease(uav);
			HR(device->CreateUnorderedAccessView(HierarchicalDepthBufferTexture, &desc, &uav));
		}

		context->CSSetShader(mCopyDepthBufferCS, nullptr, 0);

		// bind SRV and UAV
		context->CSSetShaderResources(0, 1, &NormalDepthSRV);
		context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

		const FLOAT values[] = { 0, 0, 0, 0 };
		context->ClearUnorderedAccessViewFloat(uav, values);

		UINT GroupsX = std::ceil(HierarchicalDepthBufferTextureDesc.Width / 256.0f);
		context->Dispatch(GroupsX, HierarchicalDepthBufferTextureDesc.Height, 1);

		// unbind SRV and UAV
		context->CSSetShaderResources(0, 1, NullSRVs);
		context->CSSetUnorderedAccessViews(0, 1, NullUAVs, nullptr);
	}

	//// mip level 0
	//{
	//	// RTV
	//	{
	//		D3D11_RENDER_TARGET_VIEW_DESC desc;
	//		desc.Format = HierarchicalDepthBufferTextureDesc.Format;
	//		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	//		desc.Texture2D.MipSlice = 0;

	//		SafeRelease(mPingPongTextureRTV[0]);
	//		HR(device->CreateRenderTargetView(HierarchicalDepthBufferTexture, &desc, &mPingPongTextureRTV[0]));
	//	}

	//	std::vector<ID3D11RenderTargetView*> RTVs =
	//	{
	//		//mHierarchicalDepthBufferRTV[i],
	//		mPingPongTextureRTV[0]
	//	};

	//	context->OMSetRenderTargets(RTVs.size(), RTVs.data(), nullptr);

	//	//context->ClearRenderTargetView(mHierarchicalDepthBufferRTV[i], colors[i]);
	//	context->ClearRenderTargetView(mPingPongTextureRTV[0], colors[0]);

	//	context->RSSetViewports(1, &viewport);

	//	// bind SRVs
	//	{
	//		context->PSSetShaderResources(0, 1, &NormalDepthSRV);
	//	}

	//	//// draw call
	//	context->DrawIndexed(mHierarchicalDepthBufferQuad.mMesh.mIndices.size(), mHierarchicalDepthBufferQuad.mIndexStart, mHierarchicalDepthBufferQuad.mVertexStart);

	//	// unbind SRVs
	//	{
	//		context->PSSetShaderResources(0, 1, NullSRVs);
	//	}
	//}

	for (UINT i = 1; i < mMipLevels; ++i)
	{
		UINT width  = HierarchicalDepthBufferTextureDesc.Width  >> i;
		UINT height = HierarchicalDepthBufferTextureDesc.Height >> i;

		// SRV
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC desc;
			desc.Format = HierarchicalDepthBufferTextureDesc.Format;
			desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MostDetailedMip = i - 1;
			desc.Texture2D.MipLevels = 1;

			SafeRelease(srv);
			HR(device->CreateShaderResourceView(HierarchicalDepthBufferTexture, &desc, &srv));
		}

		// RTV
		{
			D3D11_RENDER_TARGET_VIEW_DESC desc;
			desc.Format = HierarchicalDepthBufferTextureDesc.Format;
			desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipSlice = i;

			SafeRelease(rtv);
			HR(device->CreateRenderTargetView(HierarchicalDepthBufferTexture, &desc, &rtv));
		}

		context->OMSetRenderTargets(1, &rtv, nullptr);

		context->ClearRenderTargetView(rtv, colors[i]);

		viewport.Width = width;
		viewport.Height = height;
		context->RSSetViewports(1, &viewport);

		// bind SRVs
		{
			context->PSSetShaderResources(0, 1, &srv);
		}

		// draw call
		context->DrawIndexed(mHierarchicalDepthBufferQuad.mMesh.mIndices.size(), mHierarchicalDepthBufferQuad.mIndexStart, mHierarchicalDepthBufferQuad.mVertexStart);

		// unbind SRVs
		{
			context->PSSetShaderResources(0, 1, NullSRVs);
		}
	}

	SafeRelease(HierarchicalDepthBufferTexture);

	// unbind render target
	context->OMSetRenderTargets(0, nullptr, nullptr);

	SafeRelease(srv);
	SafeRelease(rtv);
	SafeRelease(uav);
}