﻿#include "D3D12WND.h"

using namespace DirectX;
using namespace Microsoft::WRL;
using namespace std;
const int gNumFrameResources = 3;

D3D12WND::D3D12WND(HWND wnd) :mhMainWnd(wnd) { 
	instance = this; 

	WCHAR title[256];
	GetWindowText(wnd, title, 256);

	mMainWndCaption = title;
}

Microsoft::WRL::ComPtr<ID3D12Device> D3D12WND::GetD3DDevice() {
	return md3dDevice;
}

D3D12WND* D3D12WND::GetD3D12WND() {
	return instance;
}

bool D3D12WND::InitDirect3D(UINT clientNum, USHORT port, std::string sceneName) {
#if defined(DEBUG) || defined(_DEBUG) 
	// Enable the D3D12 debug layer.
	{
		Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));

#ifdef _DEBUG
	LogAdapters();
#endif

	Microsoft::WRL::ComPtr<IDXGIAdapter> adapter = nullptr;
	mdxgiFactory->EnumAdapters(0, &adapter);
	
	// Try to create hardware device.
	HRESULT hardwareResult = D3D12CreateDevice(
		adapter.Get(),             // default adapter
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&md3dDevice));
	
	// Fallback to WARP device.
	if (FAILED(hardwareResult))
	{
		Microsoft::WRL::ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&md3dDevice)));
	}

	ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&mFence)));

	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Check 4X MSAA quality support for our back buffer format.
	// All Direct3D 11 capable devices support 4X MSAA for all render 
	// target formats, so we only need to check quality support.

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = mBackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));

	m4xMsaaQuality = msQualityLevels.NumQualityLevels;
	assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");


	RECT rt = { 0, };
	GetClientRect(mhMainWnd, &rt);

	mClientWidth = rt.right - rt.left;
	mClientHeight = rt.bottom - rt.top; 

	//서버측 카메라 위치 설정
	mCamera.SetPosition(0.0f, 2.0f, -30.0f);

	/* 서버 소켓 생성및 초기화 */
	server = new IOCPServer(clientNum);
	if (!server->Init(port))
		return false;


	//서버 클라이언트 정해진 인원 만큼 대기
	server->AcceptClient();


	//커맨드 오브젝트, 스왑체인, RTV/DSV서술자힙 생성
	CreateCommandObjects();
	CreateSwapChain();
	CreateRTVAndDSVDescriptorHeaps();

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	//서버측 RTV,DSV생성
	CreateRTVDSV_Server();

	//LoadScene
	LoadScene(sceneName);


	//텍스쳐 불러오기
	//LoadTextures();

	//메테리얼 생성
	//BuildMaterials();

	//지오메트리(정점데이터, 인덱스데이터)생성
	//BuildShapeGeometry();
	//BuildFbxMesh();

	//렌더아이템 생성
	//BuildWorldRenderItem();
	//BuildCharacterRenderItem();

	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();

	BuildFrameResources();	
	BuildPSOs();

	//클라이언트 명령 할당자, 목록 생성
	InitClient();
	//리드백 자원 생성
	CreateReadBackTex();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void D3D12WND::CalculateFrameStatus() {
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

		std::wstring fpsStr = std::to_wstring(fps);
		std::wstring mspfStr = std::to_wstring(mspf);

		std::wstring windowText = mMainWndCaption +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr;

		SetWindowText(mhMainWnd, windowText.c_str());

		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

void D3D12WND::CreateCommandObjects() {
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));

	ThrowIfFailed(md3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mDirectCmdListAlloc.Get(), // 연관된 cmdAlloc
		nullptr,                   // 초기화 PSO
		IID_PPV_ARGS(mCommandList.GetAddressOf())));

	mCommandList->Close();
}

void D3D12WND::CreateSwapChain() {
	mSwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = mClientWidth;
	sd.BufferDesc.Height = mClientHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = mBackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = mSwapChainBufferCount;
	sd.OutputWindow = mhMainWnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(mdxgiFactory->CreateSwapChain(
		mCommandQueue.Get(),
		&sd,
		mSwapChain.GetAddressOf()));
}




void D3D12WND::CreateRTVAndDSVDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = mSwapChainBufferCount + server->GetClientNum();	//For Rendering To Texture
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1 + server->GetClientNum();	//For Rendering To Texture
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

void D3D12WND::FlushCommandQueue() {
	mCurrentFence++;

	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

		WaitForSingleObject(eventHandle, INFINITE);

		CloseHandle(eventHandle);
	}
}

ID3D12Resource* D3D12WND::CurrentBackBuffer() const {
	return mSwapChainBuffer[mCurBackBuffer].Get();
}
D3D12_CPU_DESCRIPTOR_HANDLE D3D12WND::CurrentBackBufferView() const {
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		mCurBackBuffer,
		mRtvDescriptorSize);
}
D3D12_CPU_DESCRIPTOR_HANDLE D3D12WND::DepthStencilView() const {
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3D12WND::LogAdapters() {
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (mdxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugString(text.c_str());

		adapterList.push_back(adapter);

		++i;
	}

	for (size_t i = 0; i < adapterList.size(); ++i)
	{
		LogAdapterOutputs(adapterList[i]);
		ReleaseCom(adapterList[i]);
	}
}
void D3D12WND::LogAdapterOutputs(IDXGIAdapter* adapter) {
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

		LogOutputDisplayModes(output, mBackBufferFormat);

		ReleaseCom(output);

		++i;
	}
}
void D3D12WND::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format) {
	UINT count = 0;
	UINT flags = 0;

	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (auto& x : modeList)
	{
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"Width = " + std::to_wstring(x.Width) + L" " +
			L"Height = " + std::to_wstring(x.Height) + L" " +
			L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
			L"\n";

		::OutputDebugString(text.c_str());
	}
}

void D3D12WND::OnMouseDown(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0)
	{
		mLastMousePos.x = x;
		mLastMousePos.y = y;

		SetCapture(mhMainWnd);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		//Pick(x, y);
	}
 }
void D3D12WND::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
};
void D3D12WND::OnMouseMove(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));
	
		mCamera.Pitch(dy);
		mCamera.RotateY(dx);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
};

void D3D12WND::OnResize() {
	assert(md3dDevice);
	assert(mSwapChain);
	assert(mDirectCmdListAlloc);

	// Flush before changing any resources.
	FlushCommandQueue();

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
	ThrowIfFailed(mCommandList->Close());

	// Release the previous resources we will be recreating.
	for (int i = 0; i < mSwapChainBufferCount; ++i)
		mSwapChainBuffer[i].Reset();
	mDepthStencilBuffer.Reset();

	// Resize the swap chain.
	ThrowIfFailed(mSwapChain->ResizeBuffers(
		mSwapChainBufferCount,
		mClientWidth, mClientHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	mCurBackBuffer = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < mSwapChainBufferCount; i++)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
		md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}

	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mClientWidth;
	depthStencilDesc.Height = mClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;

	// Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from 
	// the depth buffer.  Therefore, because we need to create two views to the same resource:
	//   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
	//   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
	// we need to create the depth buffer resource with a typeless format.  
	depthStencilDesc.Format = mDepthStencilFormat;

	depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

	// Create descriptor to mip level 0 of entire resource using the format of the resource.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = mDepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

	// Transition the resource from its initial state to be used as a depth buffer.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Execute the resize commands.

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();

	// Update the viewport transform to cover the client area.
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, mClientWidth, mClientHeight };

	mCamera.SetLens(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);

	for (int i = 0; i < server->GetClientNum(); ++i) {
		auto curClinet = server->GetClient(i);
		curClinet->mCamera.SetLens(0.25f * MathHelper::Pi, static_cast<float>(curClinet->mDeviceInfo.mClientWidth) / curClinet->mDeviceInfo.mClientHeight, 1.0f, 1000.0f);
	}

	BoundingFrustum::CreateFromMatrix(mCamFrustum, mCamera.GetProj());
}

float D3D12WND::AspectRatio() const {
	return static_cast<float>(mClientWidth) / mClientHeight;
}



void D3D12WND::Draw(const GameTimer& gt) {
	//클라이언트 Draw Call
	/*	*/
	UINT passCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	for (UINT i = 0; i < server->GetClientNum(); ++i) {
		auto curClient = server->GetClient(i);
		if (curClient->rQueue.Size() > 0) {
			auto cmdAlloc = curClient->mDirectCmdListAlloc;
			auto cmdList = curClient->mCommandList;

			//명령할당자, 명령리스트 리셋
			ThrowIfFailed(cmdAlloc->Reset());
			ThrowIfFailed(cmdList->Reset(cmdAlloc.Get(), mPSOs["opaque"].Get()));

			//뷰포트 가위사각설정
			cmdList->RSSetViewports(1, &mScreenViewport);
			cmdList->RSSetScissorRects(1, &mScissorRect);


			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(curClient->mRenderTargetBuffer.Get(),
				D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

			//RTV, DSV 클리어
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
				mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
				2 + i,
				mRtvDescriptorSize);

			CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(
				mDsvHeap->GetCPUDescriptorHandleForHeapStart(),
				1 + i,
				mDsvDescriptorSize);

			//디버깅을 위해 잠시빼둠
			cmdList->ClearRenderTargetView(rtvHandle, Colors::SteelBlue, 0, nullptr);

			cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

			cmdList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

			//루트 시그니쳐 설정
			cmdList->SetGraphicsRootSignature(mRootSignature.Get());

			//서술자 테이블
			ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
			cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

			//skybox texture
			CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
			skyTexDescriptor.Offset(skyCubeSrvIdx, mCbvSrvUavDescriptorSize);
			cmdList->SetGraphicsRootDescriptorTable(4, skyTexDescriptor);

			//textures
			cmdList->SetGraphicsRootDescriptorTable(5, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

			//셰이더 자원 서술자(메테리얼)
			auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
			cmdList->SetGraphicsRootShaderResourceView(1, matBuffer->GetGPUVirtualAddress());

			//상수버퍼서술자(패스자료)
			auto passCB = mCurrFrameResource->PassCB->Resource();
			D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + ((1 + i) * passCBByteSize);
			cmdList->SetGraphicsRootConstantBufferView(2, passCBAddress);

			//여기서 그리기 수행
			cmdList->SetPipelineState(mPSOs["opaque"].Get());
			DrawRenderItems(cmdList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

			cmdList->SetPipelineState(mPSOs["skinnedOpaque"].Get());
			DrawRenderItems(cmdList.Get(), mRitemLayer[(int)RenderLayer::SkinnedOpaque]);

			cmdList->SetPipelineState(mPSOs["sky"].Get());
			DrawRenderItems(cmdList.Get(), mRitemLayer[(int)RenderLayer::Sky]);


			//리소스 배리어 전환
			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(curClient->mRenderTargetBuffer.Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE));

			//백버퍼에 설정값들 참조
			D3D12_RESOURCE_DESC Desc = curClient->mRenderTargetBuffer.Get()->GetDesc();
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT descFootPrint = {};
			UINT Rows = 0;
			UINT64 RowSize = 0;
			UINT64 TotalBytes = 0;
			md3dDevice->GetCopyableFootprints(&Desc, 0, 1, 0, &descFootPrint, &Rows, &RowSize, &TotalBytes);

			//복사대상 설정
			D3D12_TEXTURE_COPY_LOCATION dstLoc;
			dstLoc.pResource = mCurrFrameResource->mSurfaces[i].Get();
			dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			dstLoc.PlacedFootprint.Offset = 0;
			dstLoc.PlacedFootprint.Footprint.Format = curClient->mDeviceInfo.mClientPixelOreder == DeviceInfo::PixelOrder::RGBA ? mBackBufferRGBA : mBackBufferBGRA;
			dstLoc.PlacedFootprint.Footprint.Height = curClient->mDeviceInfo.mClientHeight;
			dstLoc.PlacedFootprint.Footprint.Width = curClient->mDeviceInfo.mClientWidth;
			dstLoc.PlacedFootprint.Footprint.Depth = 1;
			dstLoc.PlacedFootprint.Footprint.RowPitch = curClient->mDeviceInfo.mClientWidth * sizeof(FLOAT);
			dstLoc.SubresourceIndex = 0;

			//복사소스 설정
			D3D12_TEXTURE_COPY_LOCATION srcLoc;
			srcLoc.pResource = curClient->mRenderTargetBuffer.Get();
			srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			srcLoc.SubresourceIndex = 0;

			//복사
			cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
			//mCommandList->CopyBufferRegion(mCurrFrameResource->mSurface.Get(), 0, CurrentBackBuffer(), 0, GetSurfaceSize());


			//배리어 다시 원래대로
			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(curClient->mRenderTargetBuffer.Get(),
				D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT));

			ThrowIfFailed(cmdList->Close());

			ID3D12CommandList* cmdsLists2[] = { cmdList.Get() };

			mCommandQueue->ExecuteCommandLists(_countof(cmdsLists2), cmdsLists2);

			FlushCommandQueue();

			CopyBuffer(i);
		}
	
	}

	/*	*/
	//서버 DrawCall 

	//현재 프레임 자원의 할당자를 가져옴
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	//명령할당자, 명령리스트 리셋
	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	//뷰포트 가위사각설정
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	//백버퍼 배리어 전환 제시 -> 렌더타겟
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	//RTV, DSV 클리어
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::SteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	//서술자 테이블 (텍스쳐)
	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	//skybox texture
	CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	skyTexDescriptor.Offset(skyCubeSrvIdx, mCbvSrvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(4, skyTexDescriptor);

	//textures
	mCommandList->SetGraphicsRootDescriptorTable(5, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());


	//셰이더 자원 서술자 (메테리얼)
	auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(1, matBuffer->GetGPUVirtualAddress());

	//상수버퍼서술자 (Pass자료)
	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());


	//여기서 그리기 수행
	mCommandList->SetPipelineState(mPSOs["opaque"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["skinnedOpaque"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::SkinnedOpaque]);

	mCommandList->SetPipelineState(mPSOs["sky"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Sky]);


	//Change Barrier
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurBackBuffer = (mCurBackBuffer + 1) % mSwapChainBufferCount;

	FlushCommandQueue();
	
}


void D3D12WND::Update(const GameTimer& gt) {
	OnKeyboardInput(gt);

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	AnimateMaterials(gt);
	UpdateInstanceData(gt);
	UpdateSkinnedCBs(gt);
	UpdateMaterialBuffer(gt);
	UpdateMainPassCB(gt);

	//클라이언트 카메라를 업데이트
	UpdateClientPassCB(gt);
}

void D3D12WND::OnKeyboardInput(const GameTimer& gt) {
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState(VK_F2) & 0x8000)
		isWire_frame = !isWire_frame;

	if (GetAsyncKeyState('W') & 0x8000)
		mCamera.Walk(20.0f*dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-20.0f *dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-20.0f *dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(20.0f *dt);

	if (GetAsyncKeyState(VK_UP) & 0x8000) {
		float dy = XMConvertToRadians(0.25f*static_cast<float>(0.5f));
		mCamera.Pitch(-dy);
	}
	if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
		float dy = XMConvertToRadians(0.25f*static_cast<float>(0.5f));
		mCamera.Pitch(dy);
	}
	if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
		float dx = XMConvertToRadians(0.25f*static_cast<float>(0.5f));
		mCamera.RotateY(dx);
	}
	if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
		float dx = XMConvertToRadians(0.25f*static_cast<float>(0.5f));
		mCamera.RotateY(-dx);
	}


	if (GetAsyncKeyState('1') & 0x8000)
		mFrustumCullingEnabled = true;

	if (GetAsyncKeyState('2') & 0x8000)
		mFrustumCullingEnabled = false;
	mCamera.UpdateViewMatrix();

	for (int i = 0; i < server->GetClientNum(); ++i) {
		server->GetClient(i)->mCamera.UpdateViewMatrix();
	}
}

void D3D12WND::AnimateMaterials(const GameTimer& gt) {

}

void D3D12WND::UpdateInstanceData(const GameTimer& gt) {
	//서버쪽 절두체 선별
	XMMATRIX view = mCamera.GetView();
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);

	auto currInstanceBuffer = mCurrFrameResource->InstanceBuffer.get();
	int instanceCount = 0;
	for (auto& e : mAllRitems)
	{
		const auto& instanceData = e->Instances;
		e->InstanceSrvIndex = instanceCount;

		int visibleInstanceCount = 0;
		for (UINT i = 0; i < (UINT)instanceData.size(); ++i)
		{
			XMMATRIX world = XMLoadFloat4x4(&instanceData[i].World);
			XMMATRIX texTransform = XMLoadFloat4x4(&instanceData[i].TexTransform);

			XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(world), world);

			// View space to the object's local space.
			XMMATRIX viewToLocal = XMMatrixMultiply(invView, invWorld);

			// Transform the camera frustum from view space to the object's local space.
			BoundingFrustum localSpaceFrustum;
			mCamFrustum.Transform(localSpaceFrustum, viewToLocal);

			// Perform the box/frustum intersection test in local space.
			if ((localSpaceFrustum.Contains(e->Bounds) != DirectX::DISJOINT) || (mFrustumCullingEnabled == false))
			{
				InstanceData data;
				XMStoreFloat4x4(&data.World, XMMatrixTranspose(world));
				XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(texTransform));
				data.MaterialIndex = instanceData[i].MaterialIndex;

				// Write the instance data to structured buffer for the visible objects.
				currInstanceBuffer->CopyData(instanceCount++, data);
				++visibleInstanceCount;
			}
		}
		e->VisibleInstanceNum = visibleInstanceCount;
	}
}

void D3D12WND::UpdateSkinnedCBs(const GameTimer& gt)
{
	auto currSkinnedCB = mCurrFrameResource->SkinnedCB.get();

	for (int i = 0; i < mSkinnedModelInst.size(); ++i) {
		if (mSkinnedModelInst[i].get() == nullptr)
			return;

		// We only have one skinned model being animated.
		mSkinnedModelInst[i]->UpdateSkinnedAnimation(gt.DeltaTime());

		SkinnedConstants skinnedConstants;
		std::copy(
			std::begin(mSkinnedModelInst[i]->FinalTransforms),
			std::end(mSkinnedModelInst[i]->FinalTransforms),
			&skinnedConstants.BoneTransforms[0]);

		currSkinnedCB->CopyData(i, skinnedConstants);
	}
	
}


void D3D12WND::UpdateMaterialBuffer(const GameTimer& gt) {
	auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
	for (auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialData matData;
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;
			matData.FresnelR0 = mat->FresnelR0;
			matData.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
			matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
			matData.NormalMapIndex = mat->NormalSrvHeapIndex;
			matData.SpecularMapIndex = mat->SpecularSrvHeapIndex;

			currMaterialBuffer->CopyData(mat->MatCBIndex, matData);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void D3D12WND::UpdateMainPassCB(const GameTimer& gt) {
	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mCamera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void D3D12WND::UpdateClientPassCB(const GameTimer& gt) {
	for (UINT i = 0; i < server->GetClientNum(); ++i) {
		auto curClient = server->GetClient(i);

		XMMATRIX view = curClient->mCamera.GetView();
		XMMATRIX proj = curClient->mCamera.GetProj();

		XMMATRIX viewProj = XMMatrixMultiply(view, proj);
		XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
		XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
		XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

		XMStoreFloat4x4(&mClientPassCB.View, XMMatrixTranspose(view));
		XMStoreFloat4x4(&mClientPassCB.InvView, XMMatrixTranspose(invView));
		XMStoreFloat4x4(&mClientPassCB.Proj, XMMatrixTranspose(proj));
		XMStoreFloat4x4(&mClientPassCB.InvProj, XMMatrixTranspose(invProj));
		XMStoreFloat4x4(&mClientPassCB.ViewProj, XMMatrixTranspose(viewProj));
		XMStoreFloat4x4(&mClientPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
		mClientPassCB.EyePosW = curClient->mCamera.GetPosition3f();
		mClientPassCB.RenderTargetSize = XMFLOAT2((float)curClient->mDeviceInfo.mClientWidth, (float)curClient->mDeviceInfo.mClientHeight);
		mClientPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / curClient->mDeviceInfo.mClientWidth, 1.0f / curClient->mDeviceInfo.mClientHeight);
		mClientPassCB.NearZ = 1.0f;
		mClientPassCB.FarZ = 1000.0f;
		mClientPassCB.TotalTime = gt.TotalTime();
		mClientPassCB.DeltaTime = gt.DeltaTime();
		mClientPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
		mClientPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
		mClientPassCB.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
		mClientPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
		mClientPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
		mClientPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
		mClientPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

		auto currPassCB = mCurrFrameResource->PassCB.get();
		currPassCB->CopyData(1 + i, mClientPassCB);
	}
	
}

void D3D12WND::LoadTexture(const std::string key, const std::wstring fileName) {
	auto tempTex = std::make_unique<Texture>();
	tempTex->Name = key;
	tempTex->Filename = fileName;

	char ext[10];

	//문자열 뒤 부터 "."의 위치 검색 후 확장자명만 받아오기
	auto dotIdx = fileName.find_last_of(L".");
	sscanf(ws2s(fileName).c_str() + dotIdx, "%*[.]%s", ext);

	if (strcmp(ext, "dds") == 0) {
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
			mCommandList.Get(), tempTex->Filename.c_str(),
			tempTex->Resource, tempTex->UploadHeap));
	}
	else {
		std::unique_ptr<uint8_t[]> decodedData;
		D3D12_SUBRESOURCE_DATA subresource;

		LoadWICTextureFromFile(md3dDevice.Get(), tempTex->Filename.c_str(), tempTex->Resource.GetAddressOf(), decodedData, subresource);

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(tempTex->Resource.Get(), 0, 1);

		ThrowIfFailed(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(tempTex->UploadHeap.GetAddressOf())));

		UpdateSubresources(mCommandList.Get(), tempTex->Resource.Get(), tempTex->UploadHeap.Get(),
			0, 0, 1, &subresource);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(tempTex->Resource.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	}

	mTextures.push_back(make_pair(tempTex->Name, std::move(tempTex)));
}

void D3D12WND::LoadScene(std::string fileName) {
	mScene.Init(fileName);

	//텍스쳐/메테리얼 불러오기
	mScene.ReadMaterialFile();
	auto matAttrs = mScene.GetMaterialAttrs();
	for (auto& e : matAttrs) {
		int idx = mTextures.size();

		int norIdx = -1;
		bool hasNormal = false;

		int specIdx = -1;
		bool hasSpecular = false;

		//Load Diffuse Texture
		if (e.DiffuseTextureName.compare("NULL") != 0) {
			LoadTexture(e.DiffuseTextureName, s2ws(e.DiffuseTextureName));
		}

		//Load Normal Texture 
		if (e.NormalTextureName.compare("NULL") != 0) {
			LoadTexture(e.NormalTextureName, s2ws(e.NormalTextureName));
			hasNormal = true;
		}

		//Load Specular Texture
		if (e.SpecularTextureName.compare("NULL") != 0) {
			LoadTexture(e.SpecularTextureName, s2ws(e.SpecularTextureName));
		}

		int diffuseSrvIdx = idx;
		int normalSrvIdx = diffuseSrvIdx + (hasNormal ? 1 : 0);
		int specularSrvIdx = normalSrvIdx + (hasSpecular ? 1 : 0);

		//BuildMaterial
		BuildMaterial(e.MaterialName, diffuseSrvIdx, hasNormal ? normalSrvIdx : -1, hasSpecular ? specularSrvIdx : -1,
			e.DiffuseAlbedo, e.FresnelR0, e.Roughness);
	}

	//Geometry 불러오기
	mScene.ReadGeometryFile();
	auto geoAttrs = mScene.GetGeometryAttrs();

	//읽어온 Geometry 생성
	for (auto& e : geoAttrs) {
		std::string geoName = e.first;
		if (geoName.compare("DefaultShape") == 0) {
			//기본 도형 생성
			BuildDefaultShape(e.second);
		}
		else{
			//FBX파일인지 확인
			size_t idx = geoName.find(".fbx");
			if (idx != -1) {
				//FBX file이면 FBX파일 읽어오기
				LoadFBX(geoName, e.second[0].DrawArgsName);
			}
			else
				continue;
		}
	}

	//렌더아이템 생성
	mScene.ReadRenderItemFile();
	auto renderItemAttrs = mScene.GetRenderItemAttrs();

	for (int i = 0; i < renderItemAttrs.size(); ++i) {
		BuildRenderItem(renderItemAttrs[i]);
	}


	//타임테이블 불러오기
	mScene.ReadTimetableFile();

}


void D3D12WND::BuildRootSignature() {
	CD3DX12_DESCRIPTOR_RANGE skyboxTexTable;
	skyboxTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0); //skyboxTex

	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 16, 1, 0); //tex

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[6];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsShaderResourceView(0, 1);	//InstanceData Descriptor
	slotRootParameter[1].InitAsShaderResourceView(1, 1);	//MetarialData Descriptor
	slotRootParameter[2].InitAsConstantBufferView(0);		//PassCB Descriptor
	slotRootParameter[3].InitAsConstantBufferView(1);		//SkinnedData Descriptor
	slotRootParameter[4].InitAsDescriptorTable(1, &skyboxTexTable, D3D12_SHADER_VISIBILITY_PIXEL); //skyboxTexture
	slotRootParameter[5].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL); //Texture

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(6, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void D3D12WND::BuildDescriptorHeaps() {

	std::vector<ComPtr<ID3D12Resource>> tex2DList;

	for (auto& e : mTextures) {
		assert(e.second->Resource != nullptr);
		tex2DList.push_back(e.second->Resource);
	}
	
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = tex2DList.size() + 6;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));


	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (UINT i = 0; i < (UINT)tex2DList.size(); ++i)
	{
		srvDesc.Format = tex2DList[i]->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = tex2DList[i]->GetDesc().MipLevels;
		md3dDevice->CreateShaderResourceView(tex2DList[i].Get(), &srvDesc, hDescriptor);

		// next descriptor
		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	}

	/*		*/
	skyCubeSrvIdx = tex2DList.size();
	LoadTexture("skybox", L"Textures/desertcube1024.dds");

	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = mTextures[skyCubeSrvIdx].second->Resource->GetDesc().MipLevels;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = mTextures[mTextures.size() - 1].second->Resource->GetDesc().Format;

	md3dDevice->CreateShaderResourceView(mTextures[skyCubeSrvIdx].second->Resource.Get(), &srvDesc, hDescriptor);

	BuildMaterial("sky", skyCubeSrvIdx, -1, -1, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.1f, 0.1f, 0.1f), 1.0f);
	
}

void D3D12WND::BuildShadersAndInputLayout() {

	const D3D_SHADER_MACRO skinnedDefines[] =
	{
		"SKINNED", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = D3DUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedVS"] = D3DUtil::CompileShader(L"Shaders\\Default.hlsl", skinnedDefines, "VS", "vs_5_1");
	mShaders["opaquePS"] = D3DUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["skyVS"] = D3DUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skyPS"] = D3DUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	mSkinnedInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void D3D12WND::BuildPSOs() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	auto temp = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	opaquePsoDesc.RasterizerState = temp;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	//
	// PSO for skinned pass.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedOpaquePsoDesc = opaquePsoDesc;
	skinnedOpaquePsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	skinnedOpaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedVS"]->GetBufferPointer()),
		mShaders["skinnedVS"]->GetBufferSize()
	};
	skinnedOpaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedOpaquePsoDesc, IID_PPV_ARGS(&mPSOs["skinnedOpaque"])));

	//
	// PSO for skybox
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyboxPsoDesc = opaquePsoDesc;
	skyboxPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	//하늘을 그릴 때 깊이판정에 성공하기 위해
	skyboxPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyboxPsoDesc.pRootSignature = mRootSignature.Get();
	skyboxPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
		mShaders["skyVS"]->GetBufferSize()
	};
	skyboxPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
		mShaders["skyPS"]->GetBufferSize()
	};

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyboxPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));
}

void D3D12WND::BuildFrameResources() {
	int num = 0;
	for (int i = 0; i < mAllRitems.size(); ++i) {
		num += mAllRitems[i]->VisibleInstanceNum;
	}

	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1 + server->GetClientNum(), num, (UINT)mMaterials.size(), (UINT)mSkinnedModelInst.size(), server->GetClientNum()));
	}
}

void D3D12WND::BuildMaterial(std::string materialName, int DiffuseSrvHeapIndex, int normalSrvHeapIndex, int specularSrvHeapIndex, DirectX::XMFLOAT4 DiffuseAlbedo, DirectX::XMFLOAT3 FresnelR0, float Roughness) {
	auto tempMat = std::make_unique<Material>();
	tempMat->Name = materialName;
	tempMat->MatCBIndex = mMaterials.size();
	tempMat->DiffuseSrvHeapIndex = DiffuseSrvHeapIndex;
	tempMat->NormalSrvHeapIndex = normalSrvHeapIndex;
	tempMat->SpecularSrvHeapIndex = specularSrvHeapIndex;
	tempMat->DiffuseAlbedo = DiffuseAlbedo;
	tempMat->FresnelR0 = FresnelR0;
	tempMat->Roughness = Roughness;

	mMaterials.push_back(std::pair<std::string, unique_ptr<Material>>(tempMat->Name, std::move(tempMat)));
}

void D3D12WND::BuildDefaultShape(std::vector<GeometryAttr> geoAttr) {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box;
	GeometryGenerator::MeshData sphere;
	GeometryGenerator::MeshData geosphere;
	GeometryGenerator::MeshData cylinder;
	GeometryGenerator::MeshData grid;
	GeometryGenerator::MeshData quad;

	std::vector<GeometryGenerator::MeshData> defaultShapes;
	std::vector< SubmeshGeometry> submeshs;

	for (auto& e : geoAttr) {
		if (e.DrawArgsName.compare("Box") == 0) {
			box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
			defaultShapes.push_back(box);
		}
		else if (e.DrawArgsName.compare("Sphere") == 0) {
			sphere = geoGen.CreateSphere(1.0f, 20, 20);
			defaultShapes.push_back(sphere);
		}
		else if (e.DrawArgsName.compare("Geosphere") == 0) {
			geosphere = geoGen.CreateGeosphere(1.0f, 3);
			defaultShapes.push_back(geosphere);
		}
		else if (e.DrawArgsName.compare("Cylinder") == 0) {
			cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 5.0f, 20, 20);
			defaultShapes.push_back(cylinder);
		}
		else if (e.DrawArgsName.compare("Grid") == 0) {
			grid = geoGen.CreateGrid(30.0f, 40.0f, 60, 40);
			defaultShapes.push_back(grid);
		}
		else if (e.DrawArgsName.compare("Quad") == 0) {
			quad = geoGen.CreateQuad(0, 0, 10, 10, 1);
			defaultShapes.push_back(quad);
		}
	}

	//서브메쉬 생성

	UINT VertexOffset = 0;
	UINT IndexOffset = 0;

	for (auto& e : defaultShapes) {
		SubmeshGeometry temp;
		temp.IndexCount = e.Indices32.size();
		temp.BaseVertexLocation = VertexOffset;
		temp.StartIndexLocation = IndexOffset;

		VertexOffset += e.Vertices.size();
		IndexOffset += e.Indices32.size();

		submeshs.push_back(temp);
	}

	auto totalVertexCount = VertexOffset;

	std::vector<Vertex> vertices(totalVertexCount);

	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

	UINT j = 0;
	UINT k = 0;

	//바운딩 박스 생성
	for (auto& e : defaultShapes) {
		auto& curSubmesh = submeshs[j++];
		for (size_t i = 0; i < e.Vertices.size(); ++i, ++k)
		{
			vertices[k].Pos = e.Vertices[i].Position;
			vertices[k].Normal = e.Vertices[i].Normal;
			vertices[k].TexC = e.Vertices[i].TexC;
			vertices[k].TangentU = e.Vertices[i].TangentU;

			XMVECTOR P = XMLoadFloat3(&e.Vertices[i].Position);

			vMin = XMVectorMin(vMin, P);
			vMax = XMVectorMax(vMax, P);
		}

		XMStoreFloat3(&curSubmesh.Bounds.Center, 0.5f * (vMin + vMax));
		XMStoreFloat3(&curSubmesh.Bounds.Extents, 0.5f * (vMax - vMin));
	}


	//인덱스 생성

	std::vector<std::uint16_t> indices;
	for (auto& e : defaultShapes) {
		indices.insert(indices.end(), std::begin(e.GetIndices16()), std::end(e.GetIndices16()));
	}


	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "DefaultShape";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	UINT subIdx = 0;
	for (auto& e : geoAttr) {
		
		geo->DrawArgs[e.DrawArgsName] = submeshs[subIdx++];

	}

	mGeometries[geo->Name] = std::move(geo);
}


void D3D12WND::LoadFBX(std::string fileName, std::string argsName) {
	FBXReader read(fileName.c_str());
	read.LoadFBXData(read.GetRootNode(), false);

	SkinnedData* temp = new SkinnedData();
	read.GetSkinnedData(*temp);
	
	//애니메이션 데이터
	if (temp->BoneCount() > 0) {
		auto skinned = make_unique<SkinnedModelInstance>();
		skinned->SkinnedInfo = temp;
		skinned->FinalTransforms.resize(skinned->SkinnedInfo->BoneCount());
		skinned->TimePos = 0.0f;
		skinned->ClipName = "mixamo.com";

		mSkinnedModelInst.push_back(std::move(skinned));
	}

	skinnedTexSrvIdx = mTextures.size();

	//텍스쳐 생성 및 메테리얼 생성
	auto& mat = read.GetMaterials();
	int baseIdx = skinnedTexSrvIdx;

	for (int i = 0; i < mat.size(); ++i) {
		auto& curMat = mat[i];

		bool hasDiffuse = false;
		bool hasNormal = false;
		bool hasSpecular = false;

		if (curMat.second[FBXReader::TextureType::DIFFUSE].size() != 0) {
			hasDiffuse = true;
			auto& curDiffuse = curMat.second[FBXReader::TextureType::DIFFUSE][0];
			LoadTexture(curDiffuse.first, curDiffuse.second);
		}

		if (curMat.second[FBXReader::TextureType::NORMAL].size() != 0) {
			hasNormal = true;
			auto& curNormal = curMat.second[FBXReader::TextureType::NORMAL][0];
			LoadTexture(curNormal.first, curNormal.second);
		}

		if (curMat.second[FBXReader::TextureType::SPECULAR].size() != 0) {
			hasSpecular = true;
			auto& curSpec = curMat.second[FBXReader::TextureType::SPECULAR][0];
			LoadTexture(curSpec.first, curSpec.second);
		}
		int diffuseSrvIdx = baseIdx;
		int normalSrvIdx = diffuseSrvIdx + (hasNormal ? 1 : 0);
		int specularSrvIdx = normalSrvIdx + (hasSpecular ? 1 : 0);

		BuildMaterial(curMat.first, diffuseSrvIdx, hasNormal ? normalSrvIdx : -1, hasSpecular ? specularSrvIdx : -1);

		baseIdx = specularSrvIdx + 1;
	}

	//Vertex 설정
	std::vector<SkinnedVertex> skinVtx = read.GetVertices();
	std::vector<SkinnedVertex> vertices(skinVtx.size());

	for (size_t i = 0; i < skinVtx.size(); ++i)
	{
		vertices[i].Pos = skinVtx[i].Pos;
		vertices[i].Normal = skinVtx[i].Normal;
		vertices[i].TexC = skinVtx[i].TexC;
		vertices[i].TangentU = skinVtx[i].TangentU;
		if (vertices[i].TangentU.x == 0.0f && vertices[i].TangentU.y == 0.0f && vertices[i].TangentU.z == 0.0f) {
			vertices[i].TangentU = XMFLOAT3(vertices[i].TexC.x, 0.0f, 0.0f);
		}

		memcpy(&vertices[i].BoneIndices, &skinVtx[i].BoneIndices, sizeof(skinVtx[i].BoneIndices));
		vertices[i].BoneWeights = skinVtx[i].BoneWeights;
	}

	//인덱스 설정
	std::vector<std::uint32_t> indices = read.GetIndices();

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(SkinnedVertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = fileName;

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(SkinnedVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	for (int i = 0; i < read.GetSubmesh().size(); ++i) {
		auto curSub = read.GetSubmesh()[i];
		geo->DrawArgs[curSub.name] = curSub;
	}

	mGeometries[geo->Name] = std::move(geo);
}


void D3D12WND::BuildRenderItem(RenderItemAttr renderItemAttr) {

	UINT animCBIdx = skinnedAnimIdx;

	if (renderItemAttr.GeometryInfo->DrawArgsName.compare("Default") == 0) {
		for (auto& e : mGeometries[renderItemAttr.GeometryName]->DrawArgs) {
			auto renderItem = std::make_unique<RenderItem>();
			renderItem->Geo = mGeometries[renderItemAttr.GeometryName].get();
			renderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

			renderItem->IndexCount = e.second.IndexCount;
			renderItem->StartIndexLocation = e.second.StartIndexLocation;
			renderItem->BaseVertexLocation = e.second.BaseVertexLocation;
			renderItem->Bounds = e.second.Bounds;

			UINT instanceSrvIdx = 0;

			for (int i = 0; i < mAllRitems.size(); ++i) {
				instanceSrvIdx += mAllRitems[i]->InstanceNum;
			}

			renderItem->InstanceSrvIndex = instanceSrvIdx;
			renderItem->InstanceNum = renderItemAttr.GeometryInfo->InstanceNum;
			renderItem->VisibleInstanceNum = renderItemAttr.GeometryInfo->VisibleInstanceNum;
			
			if (renderItemAttr.GeometryInfo->IsAnimation.compare("True") == 0) {
				renderItem->SkinnedModelInst = mSkinnedModelInst[animCBIdx].get();
				renderItem->SkinnedModelInst->TimePos = renderItemAttr.GeometryInfo->TimePos;
				renderItem->SkinnedModelInst->ClipName = renderItemAttr.GeometryInfo->ClipName;
				renderItem->SkinnedCBIndex = animCBIdx;
			}

			instanceSrvIdx += renderItemAttr.GeometryInfo->InstanceNum;

			//인스턴스 설정
			renderItem->Instances.resize(renderItemAttr.GeometryInfo->InstanceNum);
			for (UINT i = 0; i < renderItemAttr.GeometryInfo->InstanceNum; ++i) {

				XMMATRIX pos = XMMatrixTranslation(renderItemAttr.Positions[i].x, renderItemAttr.Positions[i].y, renderItemAttr.Positions[i].z);
				XMMATRIX scale = XMMatrixScaling(renderItemAttr.Scales[i].x, renderItemAttr.Scales[i].y, renderItemAttr.Scales[i].z);
				XMMATRIX rotation = XMMatrixRotationRollPitchYaw(XMConvertToRadians(renderItemAttr.Rotations[i].x), XMConvertToRadians(renderItemAttr.Rotations[i].y), XMConvertToRadians(renderItemAttr.Rotations[i].z));

				XMStoreFloat4x4(&renderItem->Instances[i].World, rotation * scale * pos);

				XMMATRIX tesPos = XMMatrixTranslation(renderItemAttr.TexturePositions[i].x, renderItemAttr.TexturePositions[i].y, renderItemAttr.TexturePositions[i].z);
				XMMATRIX texScale = XMMatrixScaling(renderItemAttr.TextureScales[i].x, renderItemAttr.TextureScales[i].y, renderItemAttr.TextureScales[i].z);
				XMMATRIX texRotation = XMMatrixRotationRollPitchYaw(XMConvertToRadians(renderItemAttr.TextureRotations[i].x), XMConvertToRadians(renderItemAttr.TextureRotations[i].y), XMConvertToRadians(renderItemAttr.TextureRotations[i].z));

				XMStoreFloat4x4(&renderItem->Instances[i].TexTransform, texRotation * texScale * tesPos);

				//Material 검색

				UINT matIdx = mAllRitems.size() + 1;
				for (int j = 0; j < mMaterials.size(); ++j) {
					auto& curName = mMaterials[j].first;

					if (curName.compare(e.second.matName) == 0) {
						matIdx = j;
						break;
					}
				}

				renderItem->Instances[i].MaterialIndex = matIdx;
			}

			mAllRitems.push_back(std::move(renderItem));

			//애니메이션 존재할 경우 SkinnedOpaque로 입력
			if (renderItemAttr.GeometryInfo->IsAnimation.compare("False") == 0)
				mRitemLayer[(int)RenderLayer::Opaque].push_back(mAllRitems[mAllRitems.size() - 1].get());
			else
				mRitemLayer[(int)RenderLayer::SkinnedOpaque].push_back(mAllRitems[mAllRitems.size() - 1].get());


		}

		if (renderItemAttr.GeometryInfo->IsAnimation.compare("True") == 0) 
			skinnedAnimIdx++;
		

	}
	else {
		auto renderItem = std::make_unique<RenderItem>();
		renderItem->Geo = mGeometries[renderItemAttr.GeometryName].get();
		renderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		renderItem->IndexCount = renderItem->Geo->DrawArgs[renderItemAttr.GeometryInfo->DrawArgsName].IndexCount;
		renderItem->StartIndexLocation = renderItem->Geo->DrawArgs[renderItemAttr.GeometryInfo->DrawArgsName].StartIndexLocation;
		renderItem->BaseVertexLocation = renderItem->Geo->DrawArgs[renderItemAttr.GeometryInfo->DrawArgsName].BaseVertexLocation;
		renderItem->Bounds = renderItem->Geo->DrawArgs[renderItemAttr.GeometryInfo->DrawArgsName].Bounds;

		UINT instanceSrvIdx = 0;

		for (int i = 0; i < mAllRitems.size(); ++i) {
			instanceSrvIdx += mAllRitems[i]->InstanceNum;
		}

		renderItem->InstanceSrvIndex = instanceSrvIdx;
		renderItem->InstanceNum = renderItemAttr.GeometryInfo->InstanceNum;
		renderItem->VisibleInstanceNum = renderItemAttr.GeometryInfo->VisibleInstanceNum;

		if (renderItemAttr.GeometryInfo->IsAnimation.compare("True") == 0) {
			renderItem->SkinnedModelInst = mSkinnedModelInst[animCBIdx].get();
			renderItem->SkinnedModelInst->TimePos = renderItemAttr.GeometryInfo->TimePos;
			renderItem->SkinnedModelInst->ClipName = renderItemAttr.GeometryInfo->ClipName;
			renderItem->SkinnedCBIndex = animCBIdx;
		}

		instanceSrvIdx += renderItemAttr.GeometryInfo->InstanceNum;

		//인스턴스 설정
		renderItem->Instances.resize(renderItemAttr.GeometryInfo->InstanceNum);
		for (UINT i = 0; i < renderItemAttr.GeometryInfo->InstanceNum; ++i) {

			XMMATRIX pos = XMMatrixTranslation(renderItemAttr.Positions[i].x, renderItemAttr.Positions[i].y, renderItemAttr.Positions[i].z);
			XMMATRIX scale = XMMatrixScaling(renderItemAttr.Scales[i].x, renderItemAttr.Scales[i].y, renderItemAttr.Scales[i].z);
			XMMATRIX rotation = XMMatrixRotationRollPitchYaw(XMConvertToRadians(renderItemAttr.Rotations[i].x), XMConvertToRadians(renderItemAttr.Rotations[i].y), XMConvertToRadians(renderItemAttr.Rotations[i].z));

			XMStoreFloat4x4(&renderItem->Instances[i].World, rotation * scale * pos);

			XMMATRIX tesPos = XMMatrixTranslation(renderItemAttr.TexturePositions[i].x, renderItemAttr.TexturePositions[i].y, renderItemAttr.TexturePositions[i].z);
			XMMATRIX texScale = XMMatrixScaling(renderItemAttr.TextureScales[i].x, renderItemAttr.TextureScales[i].y, renderItemAttr.TextureScales[i].z);
			XMMATRIX texRotation = XMMatrixRotationRollPitchYaw(XMConvertToRadians(renderItemAttr.TextureRotations[i].x), XMConvertToRadians(renderItemAttr.TextureRotations[i].y), XMConvertToRadians(renderItemAttr.TextureRotations[i].z));

			XMStoreFloat4x4(&renderItem->Instances[i].TexTransform, texRotation * texScale * tesPos);

			//Material 검색

			UINT matIdx = mAllRitems.size() + 1;
			for (int j = 0; j < mMaterials.size(); ++j) {
				auto& curName = mMaterials[j].first;

				if (curName.compare(renderItemAttr.MaterialName[i]) == 0) {
					matIdx = j;
					break;
				}
			}

			renderItem->Instances[i].MaterialIndex = matIdx;
		}

		mAllRitems.push_back(std::move(renderItem));

		//애니메이션 존재할 경우 SkinnedOpaque로 입력
		if (renderItemAttr.GeometryInfo->IsAnimation.compare("False") == 0)
			mRitemLayer[(int)RenderLayer::Opaque].push_back(mAllRitems[mAllRitems.size() - 1].get());
		else
			mRitemLayer[(int)RenderLayer::SkinnedOpaque].push_back(mAllRitems[mAllRitems.size() - 1].get());

	}

}

void D3D12WND::BuildCharacterRenderItem() {
	//서브 메쉬 별로 렌더 아이템 생성
	for (int i = 0; i < mGeometries["beeGeo"]->DrawArgs.size(); ++i) {
		auto curSubRItem = std::make_unique<RenderItem>();
		curSubRItem->Geo = mGeometries["beeGeo"].get();
		curSubRItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		curSubRItem->IndexCount = curSubRItem->Geo->DrawArgs[std::to_string(i)].IndexCount;
		curSubRItem->StartIndexLocation = curSubRItem->Geo->DrawArgs[std::to_string(i)].StartIndexLocation;
		curSubRItem->BaseVertexLocation = curSubRItem->Geo->DrawArgs[std::to_string(i)].BaseVertexLocation;
		curSubRItem->InstanceSrvIndex = 22 + i;
		curSubRItem->InstanceNum = 1;
		curSubRItem->VisibleInstanceNum = 1;
		curSubRItem->SkinnedModelInst = mSkinnedModelInst[mRitemLayer[(int)RenderLayer::SkinnedOpaque].size()].get();	//!!FBX를 읽고 바로 하는게 아니라 마지막에 불러온 값이 들어감
		curSubRItem->SkinnedCBIndex = mRitemLayer[(int)RenderLayer::SkinnedOpaque].size();
		curSubRItem->Instances.resize(1);

		XMMATRIX pos = XMMatrixTranslation(0.0f, 0.0f, 10.0f);
		XMMATRIX scale = XMMatrixScaling(0.02f, 0.02f, 0.02f);
		XMMATRIX rotation = XMMatrixRotationRollPitchYaw(XMConvertToRadians(0), XMConvertToRadians(180), XMConvertToRadians(0));
		
		XMStoreFloat4x4(&curSubRItem->Instances[0].World, rotation * scale * pos);
		//XMStoreFloat4x4(&curSubRItem->Instances[0].TexTransform, rotation);
		//curSubRItem->Instances[0].World = MathHelper::Identity4x4();;
		curSubRItem->Instances[0].TexTransform = MathHelper::Identity4x4();
		curSubRItem->Instances[0].MaterialIndex = 3 + i;

		mAllRitems.push_back(std::move(curSubRItem));

		if (mSkinnedModelInst[mRitemLayer[(int)RenderLayer::SkinnedOpaque].size()].get() == nullptr)
			mRitemLayer[(int)RenderLayer::Opaque].push_back(mAllRitems[mAllRitems.size() - 1].get());
		else
			mRitemLayer[(int)RenderLayer::SkinnedOpaque].push_back(mAllRitems[mAllRitems.size() - 1].get());

	}
	
}

void D3D12WND::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems) {
	int size = sizeof(InstanceData);

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];
		
		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView()); 
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		auto instanceBuffer = mCurrFrameResource->InstanceBuffer->Resource();
		cmdList->SetGraphicsRootShaderResourceView(0, instanceBuffer->GetGPUVirtualAddress() + (ri->InstanceSrvIndex * size));

		if (ri->SkinnedModelInst != nullptr)
		{
			auto skinnedCB = mCurrFrameResource->SkinnedCB->Resource();
			UINT skinnedCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(SkinnedConstants));

			D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = skinnedCB->GetGPUVirtualAddress() + ri->SkinnedCBIndex * skinnedCBByteSize;
			cmdList->SetGraphicsRootConstantBufferView(3, skinnedCBAddress);
		}
		else
		{
			cmdList->SetGraphicsRootConstantBufferView(3, 0);
		}

		cmdList->DrawIndexedInstanced(ri->IndexCount, ri->VisibleInstanceNum, ri->StartIndexLocation, ri->BaseVertexLocation, 0);

	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> D3D12WND::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}


void D3D12WND::CreateReadBackTex() {
	for (int i = 0; i < mFrameResources.size(); ++i) {
		for (int j = 0; j < server->GetClientNum(); ++j) {
			auto curClient = server->GetClient(j);

			D3D12_RESOURCE_DESC readbackDesc;
			ZeroMemory(&readbackDesc, sizeof(readbackDesc));

			readbackDesc = CD3DX12_RESOURCE_DESC{ CD3DX12_RESOURCE_DESC::Buffer(curClient->mDeviceInfo.mClientWidth * curClient->mDeviceInfo.mClientHeight * sizeof(FLOAT)) };

			ThrowIfFailed(md3dDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
				D3D12_HEAP_FLAG_NONE,
				&readbackDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(mFrameResources[i]->mSurfaces[j].GetAddressOf())
			));
		}
	}

	mBuffers.reserve(server->GetClientNum());

	for (int i = 0; i < server->GetClientNum(); ++i) {
		mBuffers.push_back(new FLOAT());
	}

}

void D3D12WND::CopyBuffer(int cliIdx) {
	auto curClient = server->GetClient(cliIdx);

	DWORD size = curClient->mDeviceInfo.mClientWidth * curClient->mDeviceInfo.mClientHeight * sizeof(FLOAT);
	D3D12_RANGE range{ 0, size };

	if (curClient->rQueue.Size() > 0) {
		HEADER* header = (HEADER*)curClient->rQueue.FrontItem()->mHeader.buf;
		if (ntohl(header->mCommand) == COMMAND::COMMAND_REQ_FRAME) {
			/*	*/
			//데이터 압축
			void** tempBuf;
			mCurrFrameResource->mSurfaces[cliIdx]->Map(0, &range, (void**)& tempBuf);
			mCurrFrameResource->mSurfaces[cliIdx]->Unmap(0, 0);

			char* compressed_msg = new char[size];
			size_t compressed_size = 0;

			//멀티스레드 고려
			/*
			const int count = 4;
			char** compressed_msg1 = new char* [count];
			size_t compressed_size1[count];

			for (int j = 0; j < count; ++j) {
				compressed_msg1[j] = new char[size / count];
			}

			for (int j = 0; j < count; ++j) {
				compressed_size1[j] = LZ4_compress_default((char*)tempBuf + j * (size / count), compressed_msg1[j], size / count, size / count);
				//compressed_size1[j] = LZ4_compress_fast((char*)tempBuf + j*(size/ count), compressed_msg1[j], size / count, size / count, 6);
			}
			//compressed_size = LZ4_compress_default((char*)tempBuf, compressed_msg, size, size);

		*/
		//compressed_size = LZ4_compress_fast((char*)tempBuf, compressed_msg, size, size, 2);
			compressed_size = LZ4_compress_default((char*)tempBuf, compressed_msg, size, size);

			std::unique_ptr<Packet> packet = std::make_unique<Packet>(new CHEADER(COMMAND::COMMAND_RES_FRAME, compressed_size));
			packet->mData.len = compressed_size;
			packet->mData.buf = compressed_msg;

			/*
			//패킷 생성
			std::unique_ptr<Packet> packet = std::make_unique<Packet>(new CHEADER(COMMAND::COMMAND_RES_FRAME, size));
			packet->mData.len = size;

			mCurrFrameResource->mSurfaces[i]->Map(0, &range, (void**)& packet->mData.buf);
			mCurrFrameResource->mSurfaces[i]->Unmap(0, 0);
			*/

			curClient->wQueue.PushItem(std::move(packet));

			curClient->rQueue.FrontItem().release();
			curClient->rQueue.PopItem();
		}
	}

	/*
	for (int i = 0; i < server->GetClientNum(); ++i) {
		auto curClient = server->GetClient(i);

		DWORD size = curClient->mDeviceInfo.mClientWidth * curClient->mDeviceInfo.mClientHeight * sizeof(FLOAT);
		D3D12_RANGE range{ 0, size };

		if (curClient->rQueue.Size() > 0) {
			HEADER* header = (HEADER*)curClient->rQueue.FrontItem()->mHeader.buf;
			if (ntohl(header->mCommand) == COMMAND::COMMAND_REQ_FRAME) {

				//데이터 압축
				void** tempBuf;
				mCurrFrameResource->mSurfaces[i]->Map(0, &range, (void**)& tempBuf);
				mCurrFrameResource->mSurfaces[i]->Unmap(0, 0);

				char* compressed_msg = new char[size];
				size_t compressed_size = 0;

				//compressed_size = LZ4_compress_fast((char*)tempBuf, compressed_msg, size, size, 2);
				compressed_size = LZ4_compress_default((char*)tempBuf, compressed_msg, size, size);

				std::unique_ptr<Packet> packet = std::make_unique<Packet>(new CHEADER(COMMAND::COMMAND_RES_FRAME, compressed_size));
				packet->mData.len = compressed_size;
				packet->mData.buf = compressed_msg;
					
				curClient->wQueue.PushItem(std::move(packet));

				curClient->rQueue.FrontItem().release();
				curClient->rQueue.PopItem();
			}
		}
	}
	*/

}

FLOAT* D3D12WND::GetReadBackBuffer() {
	return mBuffer;
}

void D3D12WND::InputPump(const GameTimer& gt) {
	/*	*/
	for (int i = 0; i < server->GetClientNum(); ++i) {
		auto curClient = server->GetClient(i);


		while (curClient->inputRQueue.Size() > 0) {
			HEADER* header = (HEADER*)curClient->inputRQueue.FrontItem()->mHeader.buf;

			if (ntohl(header->mCommand) != COMMAND::COMMAND_INPUT)
				break;

			std::unique_ptr<Packet> packet = std::move(curClient->inputRQueue.FrontItem());
			INPUT_DATA* inputData = (INPUT_DATA*)packet->mData.buf;
			const float dtC = inputData->deltaTime;
			const float dt = gt.DeltaTime() + 2* dtC;

			if (inputData->mInputType == INPUT_TYPE::INPUT_KEY_W) {
				curClient->mCamera.Walk(20.0f * dt);
				OutputDebugStringA("Input W\n");
			}

			if (inputData->mInputType == INPUT_TYPE::INPUT_KEY_S) {
				curClient->mCamera.Walk(-20.0f * dt);
				OutputDebugStringA("Input S\n");
			}

			if (inputData->mInputType == INPUT_TYPE::INPUT_KEY_A) {
				curClient->mCamera.Strafe(-20.0f * dt);
				OutputDebugStringA("Input A\n");
			}

			if (inputData->mInputType == INPUT_TYPE::INPUT_KEY_D) {
				curClient->mCamera.Strafe(20.0f * dt);
				OutputDebugStringA("Input D\n");
			}


			if (inputData->mInputType == INPUT_TYPE::INPUT_AXIS_CAMERA_MOVE) {
				float speed = 20.0f;
				float dx = inputData->x * speed * dt;
				float dy = inputData->y * speed * dt;

				curClient->mCamera.Walk(dy);
				curClient->mCamera.Strafe(dx);
			}

			if (inputData->mInputType == INPUT_TYPE::INPUT_AXIS_CAMERA_ROT) {
				float dx = inputData->z;
				float dy = inputData->w;

				curClient->mCamera.Pitch(dy);
				curClient->mCamera.RotateY(dx);
			}

			curClient->inputRQueue.PopItem();
		}	

	}
}

void D3D12WND::RecvRequest() {
	for (int i = 0; i < server->GetClientNum(); ++i) {
		server->RequestRecv(i);
	}
}

void D3D12WND::SendFrame() {
	for (int i = 0; i < server->GetClientNum(); ++i) {
		server->RequestSend(i);
	}

}

void D3D12WND::CreateRTVDSV_Server() {

	for (int i = 0; i < server->GetClientNum(); ++i) {
		auto curClient = server->GetClient(i);
		auto clientFormat = curClient->mDeviceInfo.mClientPixelOreder == DeviceInfo::PixelOrder::RGBA ? mBackBufferRGBA : mBackBufferBGRA;
		//네트워크용 RTV생성
		D3D12_RESOURCE_DESC renderTexDesc;
		ZeroMemory(&renderTexDesc, sizeof(renderTexDesc));
		renderTexDesc.Alignment = 0;
		renderTexDesc.DepthOrArraySize = 1;
		renderTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		renderTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		renderTexDesc.Format = clientFormat;
		renderTexDesc.Width = curClient->mDeviceInfo.mClientWidth;	//후에 클라이언트 해상도를 받아서 처리하게 변경 + 클라마다 렌더타겟을 동적으로 생성으로 변경
		renderTexDesc.Height = curClient->mDeviceInfo.mClientHeight;
		renderTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		renderTexDesc.MipLevels = 1;
		renderTexDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
		renderTexDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;

		ThrowIfFailed(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&renderTexDesc,
			D3D12_RESOURCE_STATE_PRESENT,
			nullptr,
			IID_PPV_ARGS(curClient->mRenderTargetBuffer.GetAddressOf())));

		//RTV생성
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Format = clientFormat;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(
			mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
			2 + i, mRtvDescriptorSize);

		md3dDevice->CreateRenderTargetView(curClient->mRenderTargetBuffer.Get(), &rtvDesc, rtvDescriptor);


		//네트워크용 DSV생성
		D3D12_RESOURCE_DESC depthStencilDesc;
		depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthStencilDesc.Alignment = 0;
		depthStencilDesc.Width = mClientWidth;
		depthStencilDesc.Height = mClientHeight;
		depthStencilDesc.DepthOrArraySize = 1;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.Format = mDepthStencilFormat;
		depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
		depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
		depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE optClear;
		optClear.Format = mDepthStencilFormat;
		optClear.DepthStencil.Depth = 1.0f;
		optClear.DepthStencil.Stencil = 0;

		ThrowIfFailed(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&optClear,
			IID_PPV_ARGS(curClient->mDepthStencilBuffer.GetAddressOf())));

		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescriptor(
			mDsvHeap->GetCPUDescriptorHandleForHeapStart(),
			1 + i, mDsvDescriptorSize);


		md3dDevice->CreateDepthStencilView(curClient->mDepthStencilBuffer.Get(), nullptr, dsvDescriptor);
	}
}

void D3D12WND::InitClient() {
	for (int i = 0; i < server->GetClientNum(); ++i) {
		auto curClient = server->GetClient(i);
		/**/
		ThrowIfFailed(md3dDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(curClient->mDirectCmdListAlloc.GetAddressOf())));

		ThrowIfFailed(md3dDevice->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			curClient->mDirectCmdListAlloc.Get(), // 연관된 cmdAlloc
			nullptr,                   // 초기화 PSO
			IID_PPV_ARGS(curClient->mCommandList.GetAddressOf())));
		curClient->mCommandList->Close();
	}

}

