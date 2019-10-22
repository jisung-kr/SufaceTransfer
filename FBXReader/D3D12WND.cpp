#include "D3D12WND.h"

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

bool D3D12WND::InitDirect3D() {
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

	D3D12_FEATURE_DATA_D3D12_OPTIONS temp = { 0, };
	
	md3dDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &temp, sizeof(temp));

	OutputDebugStringA(temp.StandardSwizzle64KBSupported ? "True" : "False"); //F

	D3D12_FEATURE_DATA_ARCHITECTURE temp2 = { 0, };
	md3dDevice->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &temp2, sizeof(temp2));

	OutputDebugStringA(temp2.UMA ? "True" : "False");  //T
	OutputDebugStringA(temp2.CacheCoherentUMA ? "True" : "False");	//T
	
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

	//커맨드 오브젝트, 스왑체인, RTV/DSV서술자힙 생성
	CreateCommandObjects();
	CreateSwapChain();
	CreateRTVAndDSVDescriptorHeaps();

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	//서버측 카메라 위치 설정
	mCamera.SetPosition(0.0f, 2.0f, -30.0f);

	/* 서버 소켓 생성및 초기화 */
	//server = new Server();
	server = new IOCPServer();
	if (!server->Init())
		return false;


	//서버 클라이언트 정해진 인원 만큼 대기
	//server->WaitForClient();
	server->AcceptClient();

	CreateRTVDSV_Server();

	//텍스쳐 불러오기
	LoadTextures();

	//메테리얼 생성
	BuildMaterials();

	//지오메트리(정점데이터, 인덱스데이터)생성
	BuildShapeGeometry();
	BuildFbxMesh();

	//렌더아이템 생성
	BuildWorldRenderItem();
	BuildCharacterRenderItem();

	//애니메이션 생성
	DefineBoxAnimation();

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
	rtvHeapDesc.NumDescriptors = mSwapChainBufferCount + 1;	//For Rendering To Texture
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1 + 1;	//For Rendering To Texture
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
		server->GetClient(i)->mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	}

	BoundingFrustum::CreateFromMatrix(mCamFrustum, mCamera.GetProj());
}

float D3D12WND::AspectRatio() const {
	return static_cast<float>(mClientWidth) / mClientHeight;
}



void D3D12WND::Draw(const GameTimer& gt) {
	//클라이언트 Draw Call
	/*
	std::thread clinetRender([&]() -> void {
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


				cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(server->mRenderTargetBuffer.Get(),
					D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

				//RTV, DSV 클리어
				CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
					mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
					2,
					mRtvDescriptorSize);

				CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(
					mDsvHeap->GetCPUDescriptorHandleForHeapStart(),
					1,
					mDsvDescriptorSize);

				//디버깅을 위해 잠시빼둠
				cmdList->ClearRenderTargetView(rtvHandle, Colors::SteelBlue, 0, nullptr);

				cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

				cmdList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

				ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
				cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

				cmdList->SetGraphicsRootSignature(mRootSignature.Get());

				//셰이더 자원 서술자
				auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
				cmdList->SetGraphicsRootShaderResourceView(1, matBuffer->GetGPUVirtualAddress());

				//상수버퍼서술자 
	
				auto passCB = mCurrFrameResource->PassCB->Resource();
				D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + ((DWORD64)1 + i) * passCBByteSize;
				cmdList->SetGraphicsRootConstantBufferView(2, passCBAddress);


				//서술자 테이블
				cmdList->SetGraphicsRootDescriptorTable(4, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

				//여기서 그리기 수행
				cmdList->SetPipelineState(mPSOs["opaque"].Get());
				DrawRenderItems(cmdList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

				cmdList->SetPipelineState(mPSOs["skinnedOpaque"].Get());
				DrawRenderItems(cmdList.Get(), mRitemLayer[(int)RenderLayer::SkinnedOpaque]);

				//리소스 배리어 전환
				cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(server->mRenderTargetBuffer.Get(),
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE));


				//백버퍼에 설정값들 참조
				D3D12_RESOURCE_DESC Desc = server->mRenderTargetBuffer.Get()->GetDesc();
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
				dstLoc.PlacedFootprint.Footprint.Format = mBackBufferFormat;
				dstLoc.PlacedFootprint.Footprint.Height = mClientHeight;
				dstLoc.PlacedFootprint.Footprint.Width = mClientWidth;
				dstLoc.PlacedFootprint.Footprint.Depth = 1;
				dstLoc.PlacedFootprint.Footprint.RowPitch = mClientWidth * sizeof(FLOAT);
				dstLoc.SubresourceIndex = 0;

				//복사소스 설정
				D3D12_TEXTURE_COPY_LOCATION srcLoc;
				srcLoc.pResource = server->mRenderTargetBuffer.Get();
				srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				srcLoc.SubresourceIndex = 0;

				//복사
				cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
				//mCommandList->CopyBufferRegion(mCurrFrameResource->mSurface.Get(), 0, CurrentBackBuffer(), 0, GetSurfaceSize());


				//배리어 다시 원래대로
				cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(server->mRenderTargetBuffer.Get(),
					D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT));

				ThrowIfFailed(cmdList->Close());

				ID3D12CommandList* cmdsLists2[] = { cmdList.Get() };

				mCommandQueue->ExecuteCommandLists(_countof(cmdsLists2), cmdsLists2);

				FlushCommandQueue();

				CopyBuffer();
			}

		}
	});
	*/
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


			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(server->mRenderTargetBuffer.Get(),
				D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

			//RTV, DSV 클리어
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
				mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
				2,
				mRtvDescriptorSize);

			CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(
				mDsvHeap->GetCPUDescriptorHandleForHeapStart(),
				1,
				mDsvDescriptorSize);

			//디버깅을 위해 잠시빼둠
			cmdList->ClearRenderTargetView(rtvHandle, Colors::SteelBlue, 0, nullptr);

			cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

			cmdList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

			ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
			cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

			cmdList->SetGraphicsRootSignature(mRootSignature.Get());

			//셰이더 자원 서술자
			auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
			cmdList->SetGraphicsRootShaderResourceView(1, matBuffer->GetGPUVirtualAddress());

			//상수버퍼서술자 
			auto passCB = mCurrFrameResource->PassCB->Resource();
			D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + ((DWORD64)1 + i) * passCBByteSize;
			cmdList->SetGraphicsRootConstantBufferView(2, passCBAddress);


			//서술자 테이블
			cmdList->SetGraphicsRootDescriptorTable(4, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

			//여기서 그리기 수행
			mCommandList->SetPipelineState(mPSOs["opaque"].Get());
			DrawRenderItems(cmdList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

			cmdList->SetPipelineState(mPSOs["skinnedOpaque"].Get());
			DrawRenderItems(cmdList.Get(), mRitemLayer[(int)RenderLayer::SkinnedOpaque]);

			//리소스 배리어 전환
			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(server->mRenderTargetBuffer.Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE));


			//백버퍼에 설정값들 참조
			D3D12_RESOURCE_DESC Desc = server->mRenderTargetBuffer.Get()->GetDesc();
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
			dstLoc.PlacedFootprint.Footprint.Format = mBackBufferFormat;
			dstLoc.PlacedFootprint.Footprint.Height = mClientHeight;
			dstLoc.PlacedFootprint.Footprint.Width = mClientWidth;
			dstLoc.PlacedFootprint.Footprint.Depth = 1;
			dstLoc.PlacedFootprint.Footprint.RowPitch = mClientWidth * sizeof(FLOAT);
			dstLoc.SubresourceIndex = 0;

			//복사소스 설정
			D3D12_TEXTURE_COPY_LOCATION srcLoc;
			srcLoc.pResource = server->mRenderTargetBuffer.Get();
			srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			srcLoc.SubresourceIndex = 0;

			//복사
			cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
			//mCommandList->CopyBufferRegion(mCurrFrameResource->mSurface.Get(), 0, CurrentBackBuffer(), 0, GetSurfaceSize());


			//배리어 다시 원래대로
			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(server->mRenderTargetBuffer.Get(),
				D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT));

			ThrowIfFailed(cmdList->Close());

			ID3D12CommandList* cmdsLists2[] = { cmdList.Get() };

			mCommandQueue->ExecuteCommandLists(_countof(cmdsLists2), cmdsLists2);

			FlushCommandQueue();

			CopyBuffer();
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
	mCommandList->SetGraphicsRootDescriptorTable(4, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

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


	mAnimTimePos += gt.DeltaTime();
	if (mAnimTimePos >= mBoxAnimation.GetEndTime())
	{
		// Loop animation back to beginning.
		mAnimTimePos = 0.0f;
	}

	mBoxAnimation.Interpolate(mAnimTimePos, mBoxWorld);
	mAllRitems[0]->Instances[0].World = mBoxWorld;
	//mBoxRitem->NumFramesDirty = gNumFrameResources;



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

	if (mSkinnedModelInst.get() == nullptr)
		return;

	// We only have one skinned model being animated.
	mSkinnedModelInst->UpdateSkinnedAnimation(gt.DeltaTime());

	SkinnedConstants skinnedConstants;
	std::copy(
		std::begin(mSkinnedModelInst->FinalTransforms),
		std::end(mSkinnedModelInst->FinalTransforms),
		&skinnedConstants.BoneTransforms[0]);

	currSkinnedCB->CopyData(0, skinnedConstants);
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
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

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
		mClientPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
		mClientPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
		mClientPassCB.NearZ = 1.0f;
		mClientPassCB.FarZ = 1000.0f;
		mClientPassCB.TotalTime = gt.TotalTime();
		mClientPassCB.DeltaTime = gt.DeltaTime();
		mClientPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
		mClientPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
		mClientPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
		mClientPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
		mClientPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
		mClientPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
		mClientPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

		auto currPassCB = mCurrFrameResource->PassCB.get();
		currPassCB->CopyData(1 + i, mClientPassCB);
	}
	
}

void D3D12WND::LoadTexture(const std::string key, const std::wstring fileName) {
	pair<std::string, std::unique_ptr<Texture>> tempPair;
	auto tempTex = std::make_unique<Texture>();
	tempTex->Name = key;
	tempTex->Filename = fileName;

	char ext[50];

	sscanf(ws2s(fileName).c_str(), "%*[^.]%*[.]%s", ext);
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

void D3D12WND::LoadTextures() {

	std::vector<std::string> texNames =
	{
		"bricksTex",
		//"bricksTexMap",
		"stoneTex",
		"tileTex",
		//"tileTexMap",
		"crateTex",
	};

	std::vector<std::wstring> texFilenames =
	{
		L"Textures/bricks.dds",
		//L"Textures/bricks_nmap.dds",
		L"Textures/stone.dds",
		L"Textures/tile.dds",
		//L"Textures/tile_nmap.dds",
		L"Textures/WoodCrate01.dds"
	};
	
	for (int i = 0; i < (int)texNames.size(); ++i)
	{

		LoadTexture(texNames[i], texFilenames[i]);	//원래는 중복 체크도 해야함
		
	}

}

void D3D12WND::BuildRootSignature() {
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 16, 0, 0); //ÅØ½ºÃÄ °¹¼ö

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[5];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsShaderResourceView(0, 1);	//InstanceData Descriptor
	slotRootParameter[1].InitAsShaderResourceView(1, 1);	//MetarialData Descriptor
	slotRootParameter[2].InitAsConstantBufferView(0);		//PassCB Descriptor
	slotRootParameter[3].InitAsConstantBufferView(1);		//SkinnedData Descriptor
	slotRootParameter[4].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL); //Texture

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter,
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
	srvHeapDesc.NumDescriptors = tex2DList.size();
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

}

void D3D12WND::BuildShadersAndInputLayout() {
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO skinnedDefines[] =
	{
		"SKINNED", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = D3DUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedVS"] = D3DUtil::CompileShader(L"Shaders\\Default.hlsl", skinnedDefines, "VS", "vs_5_1");
	mShaders["opaquePS"] = D3DUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");

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

}

void D3D12WND::BuildFrameResources() {
	int num = 0;
	for (int i = 0; i < mAllRitems.size(); ++i) {
		num += mAllRitems[i]->VisibleInstanceNum;
	}

	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1 + server->GetClientNum(), num, (UINT)mMaterials.size(), server->GetClientNum()));
	}
}

void D3D12WND::BuildMaterial(std::string materialName,int matIndex, int DiffuseSrvHeapIndex, DirectX::XMFLOAT4 DiffuseAlbedo, DirectX::XMFLOAT3 FresnelR0, float Roughness) {
	auto tempMat = std::make_unique<Material>();
	tempMat->Name = materialName;
	tempMat->MatCBIndex = matIndex;
	tempMat->DiffuseSrvHeapIndex = DiffuseSrvHeapIndex;
	tempMat->DiffuseAlbedo = DiffuseAlbedo;
	tempMat->FresnelR0 = FresnelR0;
	tempMat->Roughness = Roughness;

	mMaterials[tempMat->Name] = std::move(tempMat);
}
void D3D12WND::BuildMaterials() {

	BuildMaterial("bricks0", 0, 0, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.1f, 0.1f, 0.1f), 0.3f);
	BuildMaterial("stone0", 1, 1, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.05f, 0.05f, 0.05f), 0.3f);
	BuildMaterial("tile0", 2, 2, XMFLOAT4(0.9f, 0.9f, 0.9f, 1.0f), XMFLOAT3(0.2f, 0.2f, 0.2f), 0.1f);
	BuildMaterial("crate0", 3, 3, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.05f, 0.05f, 0.05f), 0.2f);
}


void D3D12WND::BuildShapeGeometry() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(30.0f, 40.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);
	
	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);


	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	//box
	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;

		XMVECTOR P = XMLoadFloat3(&box.Vertices[i].Position);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}

	XMStoreFloat3(&boxSubmesh.Bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&boxSubmesh.Bounds.Extents, 0.5f * (vMax - vMin));


	//grid
	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;

		XMVECTOR P = XMLoadFloat3(&grid.Vertices[i].Position);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);

	}
	XMStoreFloat3(&gridSubmesh.Bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&gridSubmesh.Bounds.Extents, 0.5f * (vMax - vMin));

	//sphere
	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;

		XMVECTOR P = XMLoadFloat3(&sphere.Vertices[i].Position);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}

	XMStoreFloat3(&sphereSubmesh.Bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&sphereSubmesh.Bounds.Extents, 0.5f * (vMax - vMin));

	//cylinder
	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;

		XMVECTOR P = XMLoadFloat3(&cylinder.Vertices[i].Position);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}

	XMStoreFloat3(&cylinderSubmesh.Bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&cylinderSubmesh.Bounds.Extents, 0.5f * (vMax - vMin));

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

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

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void D3D12WND::BuildFbxMesh() {
	//이곳에서 Fbx로 부터 각 정점 받아오기 수행
	FBXReader read("Bee.fbx");

	read.LoadFBXData(read.GetRootNode(), false);

	SkinnedData* temp = new SkinnedData();
	read.GetSkinnedData(*temp);

	if (temp->BoneCount() > 0) {
		mSkinnedModelInst = make_unique<SkinnedModelInstance>();
		mSkinnedModelInst->SkinnedInfo = temp;
		mSkinnedModelInst->FinalTransforms.resize(mSkinnedModelInst->SkinnedInfo->BoneCount());
		mSkinnedModelInst->TimePos = 0.0f;
		mSkinnedModelInst->ClipName = read.GetClipName();
	}

	//텍스쳐 생성 및 메테리얼 생성
	auto& matNames = read.GetMaterialNames();
	auto& texNames = read.GetTextureNames();
	auto& fileNames = read.GetTextureFileNames();
	for (int i = 0; i < fileNames.size(); ++i) {
		LoadTexture(texNames[i], fileNames[i]);
	}
	for (int i = 0; i < matNames.size(); ++i) {
		BuildMaterial(matNames[i], mMaterials.size(), mMaterials.size());
	}

	//Vertex 설정
	GeometryGenerator::MeshData data = read.GetMeshData();
	std::vector<SkinnedVertex> skinVtx = read.GetVertices();
	std::vector<SkinnedVertex> vertices(skinVtx.size());

	for (size_t i = 0; i < data.Vertices.size(); ++i)
	{
		vertices[i].Pos = skinVtx[i].Pos;
		vertices[i].Normal = skinVtx[i].Normal;
		vertices[i].TexC = skinVtx[i].TexC;
		memcpy(&vertices[i].BoneIndices, &skinVtx[i].BoneIndices, sizeof(skinVtx[i].BoneIndices));
		vertices[i].BoneWeights = skinVtx[i].BoneWeights;
	}

	//인덱스 설정
	std::vector<std::uint32_t> indices = read.GetIndices();

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(SkinnedVertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "beeGeo";

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
		geo->DrawArgs[std::to_string(i)] = curSub;
	}

	mGeometries[geo->Name] = std::move(geo);
}

void D3D12WND::BuildWorldRenderItem() {
	//Box
	auto boxRitem = std::make_unique<RenderItem>();
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	boxRitem->Bounds = boxRitem->Geo->DrawArgs["box"].Bounds;
	boxRitem->InstanceSrvIndex = 0;
	boxRitem->InstanceNum = 1;
	boxRitem->VisibleInstanceNum = 1;

	boxRitem->Instances.resize(1);
	XMStoreFloat4x4(&boxRitem->Instances[0].World, XMMatrixScaling(4.0f, 4.0f, 4.0f) * XMMatrixTranslation(0.0f, 1.0f, 0.0f));
	XMStoreFloat4x4(&boxRitem->Instances[0].TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem->Instances[0].MaterialIndex = 3;
	
	mAllRitems.push_back(std::move(boxRitem));

	//Grid
	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	gridRitem->Bounds = gridRitem->Geo->DrawArgs["grid"].Bounds;
	gridRitem->InstanceSrvIndex = 1;
	gridRitem->InstanceNum = 1;
	gridRitem->VisibleInstanceNum = 1;

	gridRitem->Instances.resize(1);
	gridRitem->Instances[0].World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->Instances[0].TexTransform, XMMatrixScaling(16.0f, 16.0f, 1.0f));
	gridRitem->Instances[0].MaterialIndex = 2;

	mAllRitems.push_back(std::move(gridRitem));

	//Brick(Cylinder)
	auto cylinderRitem = std::make_unique<RenderItem>();
	cylinderRitem->Geo = mGeometries["shapeGeo"].get();
	cylinderRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	cylinderRitem->IndexCount = cylinderRitem->Geo->DrawArgs["cylinder"].IndexCount;
	cylinderRitem->StartIndexLocation = cylinderRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
	cylinderRitem->BaseVertexLocation = cylinderRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	cylinderRitem->Bounds = cylinderRitem->Geo->DrawArgs["cylinder"].Bounds;
	cylinderRitem->InstanceSrvIndex = 2;
	cylinderRitem->InstanceNum = 10;
	cylinderRitem->VisibleInstanceNum = 10;

	cylinderRitem->Instances.resize(10);
	XMMATRIX brickTexTransform = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	for (int i = 0; i < 5; ++i) {

		XMMATRIX leftCylWorld = XMMatrixTranslation(-9.0f, 1.5f, -15.0f + i * 8.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+9.0f, 1.5f, -15.0f + i * 8.0f);

		XMStoreFloat4x4(&cylinderRitem->Instances[i * 2].World, leftCylWorld);
		XMStoreFloat4x4(&cylinderRitem->Instances[i * 2].TexTransform, brickTexTransform);
		cylinderRitem->Instances[i * 2].MaterialIndex = 0;

		XMStoreFloat4x4(&cylinderRitem->Instances[i * 2 + 1].World, rightCylWorld);
		XMStoreFloat4x4(&cylinderRitem->Instances[i * 2 + 1].TexTransform, brickTexTransform);
		cylinderRitem->Instances[i * 2 + 1].MaterialIndex = 0;
	}

	mAllRitems.push_back(std::move(cylinderRitem));

	//Sphere
	auto sphereRitem = std::make_unique<RenderItem>();
	sphereRitem->Geo = mGeometries["shapeGeo"].get();
	sphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	sphereRitem->IndexCount = sphereRitem->Geo->DrawArgs["sphere"].IndexCount;
	sphereRitem->StartIndexLocation = sphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	sphereRitem->BaseVertexLocation = sphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	sphereRitem->Bounds = sphereRitem->Geo->DrawArgs["sphere"].Bounds;
	sphereRitem->InstanceSrvIndex = 12;
	sphereRitem->InstanceNum = 10;
	sphereRitem->VisibleInstanceNum = 10;

	sphereRitem->Instances.resize(10);
	for (int i = 0; i < 5; ++i) {

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-9.0f, 3.5f, -15.0f + i * 8.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+9.0f, 3.5f, -15.0f + i * 8.0f);

		XMStoreFloat4x4(&sphereRitem->Instances[i * 2].World, leftSphereWorld);
		sphereRitem->Instances[i * 2].TexTransform = MathHelper::Identity4x4();
		sphereRitem->Instances[i * 2].MaterialIndex = 1;

		XMStoreFloat4x4(&sphereRitem->Instances[i * 2 + 1].World, rightSphereWorld);
		sphereRitem->Instances[i * 2 + 1].TexTransform = MathHelper::Identity4x4();
		sphereRitem->Instances[i * 2 + 1].MaterialIndex = 1;
	}

	mAllRitems.push_back(std::move(sphereRitem));

	for (auto& e : mAllRitems)
		mRitemLayer[(int)RenderLayer::Opaque].push_back(e.get());

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
		curSubRItem->SkinnedModelInst = mSkinnedModelInst.get();
		curSubRItem->SkinnedCBIndex = 0;
		curSubRItem->Instances.resize(1);

		XMMATRIX pos = XMMatrixTranslation(0.0f, 0.0f, 10.0f);
		XMMATRIX scale =  XMMatrixScaling(0.008f, 0.008f, 0.008f);
		XMMATRIX rotation = XMMatrixRotationY(XMConvertToRadians(180));
		XMStoreFloat4x4(&curSubRItem->Instances[0].World, rotation * scale * pos);
		curSubRItem->Instances[0].TexTransform = MathHelper::Identity4x4();
		curSubRItem->Instances[0].MaterialIndex = 4 + i;

		mAllRitems.push_back(std::move(curSubRItem));
		mRitemLayer[(int)RenderLayer::Opaque].push_back(mAllRitems[mAllRitems.size() - 1].get());
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

	D3D12_RESOURCE_DESC readbackDesc;
	ZeroMemory(&readbackDesc, sizeof(readbackDesc));

	readbackDesc = CD3DX12_RESOURCE_DESC{ CD3DX12_RESOURCE_DESC::Buffer(GetSurfaceSize()) };
	
	for (int i = 0; i < mFrameResources.size(); ++i) {
		for (int j = 0; j < server->GetClientNum(); ++j) {
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

void D3D12WND::CopyBuffer() {
	DWORD size = GetSurfaceSize();
	D3D12_RANGE range{ 0, size };

	for (int i = 0; i < server->GetClientNum(); ++i) {
		auto curClient = server->GetClient(i);

		if (curClient->rQueue.Size() > 0) {
			HEADER* header = (HEADER*)curClient->rQueue.FrontItem()->mHeader.buf;
			if (ntohl(header->mCommand) == COMMAND::COMMAND_REQ_FRAME) {
				/*	*/
				//데이터 압축
				void** tempBuf;
				mCurrFrameResource->mSurfaces[i]->Map(0, &range, (void**)& tempBuf);
				mCurrFrameResource->mSurfaces[i]->Unmap(0, 0);

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
				compressed_size = LZ4_compress_fast((char*)tempBuf, compressed_msg, size, size, 6);
				
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
	}

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
			const float dt = gt.DeltaTime();

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

	//네트워크용 RTV생성
	D3D12_RESOURCE_DESC renderTexDesc;
	ZeroMemory(&renderTexDesc, sizeof(renderTexDesc));
	renderTexDesc.Alignment = 0;
	renderTexDesc.DepthOrArraySize = 1;
	renderTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	renderTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	renderTexDesc.Format = mBackBufferFormat;
	renderTexDesc.Width = mClientWidth;	//후에 클라이언트 해상도를 받아서 처리하게 변경 + 클라마다 렌더타겟을 동적으로 생성으로 변경
	renderTexDesc.Height = mClientHeight;
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
		IID_PPV_ARGS(server->mRenderTargetBuffer.GetAddressOf())));

	//RTV생성
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = mBackBufferFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		2, mRtvDescriptorSize);
	
	md3dDevice->CreateRenderTargetView(server->mRenderTargetBuffer.Get(), &rtvDesc, rtvDescriptor);


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
		IID_PPV_ARGS(server->mDepthStencilBuffer.GetAddressOf())));

	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescriptor(
		mDsvHeap->GetCPUDescriptorHandleForHeapStart(),
		1, mDsvDescriptorSize);


	md3dDevice->CreateDepthStencilView(server->mDepthStencilBuffer.Get(), nullptr, dsvDescriptor);
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

void D3D12WND::DefineBoxAnimation() {

	mBoxRitem = mAllRitems[0].get();

	XMVECTOR q0 = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMConvertToRadians(30.0f));
	XMVECTOR q1 = XMQuaternionRotationAxis(XMVectorSet(1.0f, 1.0f, 2.0f, 0.0f), XMConvertToRadians(45.0f));
	XMVECTOR q2 = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMConvertToRadians(-30.0f));
	XMVECTOR q3 = XMQuaternionRotationAxis(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMConvertToRadians(70.0f));

	mBoxAnimation.Keyframes.resize(5);
	mBoxAnimation.Keyframes[0].TimePos = 0.0f;
	mBoxAnimation.Keyframes[0].Translation = XMFLOAT3(-7.0f, 0.0f, 0.0f);
	mBoxAnimation.Keyframes[0].Scale = XMFLOAT3(0.25f, 0.25f, 0.25f);
	XMStoreFloat4(&mBoxAnimation.Keyframes[0].RotationQuat, q0);

	mBoxAnimation.Keyframes[1].TimePos = 2.0f;
	mBoxAnimation.Keyframes[1].Translation = XMFLOAT3(0.0f, 2.0f, 10.0f);
	mBoxAnimation.Keyframes[1].Scale = XMFLOAT3(0.5f, 0.5f, 0.5f);
	XMStoreFloat4(&mBoxAnimation.Keyframes[1].RotationQuat, q1);

	mBoxAnimation.Keyframes[2].TimePos = 4.0f;
	mBoxAnimation.Keyframes[2].Translation = XMFLOAT3(7.0f, 0.0f, 0.0f);
	mBoxAnimation.Keyframes[2].Scale = XMFLOAT3(0.25f, 0.25f, 0.25f);
	XMStoreFloat4(&mBoxAnimation.Keyframes[2].RotationQuat, q2);

	mBoxAnimation.Keyframes[3].TimePos = 6.0f;
	mBoxAnimation.Keyframes[3].Translation = XMFLOAT3(0.0f, 1.0f, -10.0f);
	mBoxAnimation.Keyframes[3].Scale = XMFLOAT3(0.5f, 0.5f, 0.5f);
	XMStoreFloat4(&mBoxAnimation.Keyframes[3].RotationQuat, q3);

	mBoxAnimation.Keyframes[4].TimePos = 8.0f;
	mBoxAnimation.Keyframes[4].Translation = XMFLOAT3(-7.0f, 0.0f, 0.0f);
	mBoxAnimation.Keyframes[4].Scale = XMFLOAT3(0.25f, 0.25f, 0.25f);
	XMStoreFloat4(&mBoxAnimation.Keyframes[4].RotationQuat, q0);
}
