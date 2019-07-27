#include "D3D12WND.h"

using namespace DirectX;
using namespace Microsoft::WRL;

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
	mdxgiFactory->EnumAdapters(1, &adapter);
	
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
	mClientHeight = rt.bottom - rt.top; //App Height

	CreateCommandObjects();
	CreateSwapChain();
	CreateRTVAndDSVDescriptorHeaps();


	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	mCamera.SetPosition(0.0f, 2.0f, -25.0f);

	LoadTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildMaterials();

	BuildCubeMesh();
	BuildRenderItems();

	//리드백 자원 생성
	CreateReadBackTex();

	BuildFrameResources();	
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();


	return true;
}

void D3D12WND::CalculateFrameStatus() {
	// Code computes the average frames per second, and also the 
	// average time it takes to render one frame.  These stats 
	// are appended to the window caption bar.

	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period.
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

		// Reset for next average.
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
		mDirectCmdListAlloc.Get(), // Associated command allocator
		nullptr,                   // Initial PipelineStateObject
		IID_PPV_ARGS(mCommandList.GetAddressOf())));

	// Start off in a closed state.  This is because the first time we refer 
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
	mCommandList->Close();
}
void D3D12WND::CreateSwapChain() {
	// Release the previous swapchain we will be recreating.
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



	// Note: Swap chain uses queue to perform flush.
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
	// Advance the fence value to mark commands up to this fence point.
	mCurrentFence++;

	// Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	// Wait until the GPU has completed commands up to this fence point.
	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.  
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

		// Wait until the GPU hits current fence event is fired.
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
		//ReleaseCom(adapterList[i]);
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

		//ReleaseCom(output);

		++i;
	}
}
void D3D12WND::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format) {
	UINT count = 0;
	UINT flags = 0;

	// Call with nullptr to get list count.
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
		// Make each pixel correspond to a quarter of a degree.
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
}

float D3D12WND::AspectRatio() const {
	return static_cast<float>(mClientWidth) / mClientHeight;
}



void D3D12WND::Draw(const GameTimer& gt) {
	/*	*/
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::SteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	/*--------------------------------------------------------------------------------------*/
	/*	그리기  */
	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	//셰이더 자원 서술자
	auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(1, matBuffer->GetGPUVirtualAddress());

	//상수버퍼서술자 
	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	//서술자 테이블
	mCommandList->SetGraphicsRootDescriptorTable(3, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	//와이어 프레임 설정 켜져 있을 시 파이프상태 변경
	if (isWire_frame)
		mCommandList->SetPipelineState(mPSOs["opaque_wireFrame"].Get());

	//여기서 그리기 수행
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);


	/*--------------------------------------------------------------------------------------*/
	//리소스 배리어 전환
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE));
	
	//복사대상 설정
	D3D12_TEXTURE_COPY_LOCATION dstLoc;
	dstLoc.pResource = mSurface.Get();
	dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dstLoc.PlacedFootprint.Offset = 0;
	dstLoc.PlacedFootprint.Footprint.Format = mBackBufferFormat;
	dstLoc.PlacedFootprint.Footprint.Height = mClientHeight;
	dstLoc.PlacedFootprint.Footprint.Width = mClientWidth;
	dstLoc.PlacedFootprint.Footprint.Depth = 1;
	dstLoc.PlacedFootprint.Footprint.RowPitch = (((mClientWidth * sizeof(float)) / D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) + 1) * D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
	dstLoc.SubresourceIndex = 0;

	//복사소스 설정
	D3D12_TEXTURE_COPY_LOCATION srcLoc;
	srcLoc.pResource = CurrentBackBuffer();
	srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	srcLoc.SubresourceIndex = 0;

	//복사
	mCommandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
	

	//배리어 다시 원래대로
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

	/*--------------------------------------------------------------------------------------*/
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurBackBuffer = (mCurBackBuffer + 1) % mSwapChainBufferCount;

	FlushCommandQueue();   

	CheckBuffer();

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
	UpdateMaterialBuffer(gt);
	UpdateMainPassCB(gt);
}

void D3D12WND::OnKeyboardInput(const GameTimer& gt) {
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState(VK_F2) & 0x8000)
		isWire_frame = !isWire_frame;

	if (GetAsyncKeyState('W') & 0x8000)
		mCamera.Walk(100.0f*dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-100.0f*dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-100.0f*dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(100.0f*dt);

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
	mCamera.UpdateViewMatrix();
}
void D3D12WND::AnimateMaterials(const GameTimer& gt) {

}
void D3D12WND::UpdateInstanceData(const GameTimer& gt) {
	XMMATRIX view = mCamera.GetView();
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);

	auto currInstanceBuffer = mCurrFrameResource->InstanceBuffer.get();
	int instanceCount = 0;
	for (auto& e : mAllRitems)
	{
		auto& instanceData = e->Instances;
		UINT visibleInstNum = e->VisibleInstanceNum;
		for (UINT i = 0; i < visibleInstNum; ++i)
		{
			XMMATRIX world, texTransform;
			InstanceData data;

			world = XMLoadFloat4x4(&instanceData[i].World);
			texTransform = XMLoadFloat4x4(&instanceData[i].TexTransform);

			XMStoreFloat4x4(&data.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(texTransform));

			data.MaterialIndex = instanceData[i].MaterialIndex;

			currInstanceBuffer->CopyData(instanceCount++, data);
		}
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


void D3D12WND::LoadTextures() {
	auto defaultTex = std::make_unique<Texture>();
	defaultTex->Name = "defaultTex";
	defaultTex->Filename = L"Textures/white1x1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), defaultTex->Filename.c_str(),
		defaultTex->Resource, defaultTex->UploadHeap));

	mTextures[defaultTex->Name] = std::move(defaultTex);
}
void D3D12WND::BuildRootSignature() {
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0); //ÅØ½ºÃÄ °¹¼ö

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsShaderResourceView(0, 1);	//InstanceData Descriptor
	slotRootParameter[1].InitAsShaderResourceView(1, 1);	//MetarialData Descriptor
	slotRootParameter[2].InitAsConstantBufferView(0);	//PassCB Descriptor
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL); //Texture

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
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
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = (UINT)mTextures.size();
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto defaultTex = mTextures["defaultTex"]->Resource;


	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = defaultTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = defaultTex->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	md3dDevice->CreateShaderResourceView(defaultTex.Get(), &srvDesc, hDescriptor);

}
void D3D12WND::BuildShadersAndInputLayout() {
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = D3DUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = D3DUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
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

	D3D12_RASTERIZER_DESC wire_frame = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	wire_frame.FillMode = D3D12_FILL_MODE_WIREFRAME;
	opaquePsoDesc.RasterizerState = wire_frame;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireFrame"])));
}
void D3D12WND::BuildFrameResources() {
	int num = 1;
	for (int i = 0; i < mAllRitems.size(); ++i) {
		num += mAllRitems[i]->InstanceCount;
	}
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, num, (UINT)mMaterials.size()));
	}
}
void D3D12WND::BuildMaterials() {
	auto defaultMat = std::make_unique<Material>();
	defaultMat->Name = "default";
	defaultMat->MatCBIndex = 0;
	defaultMat->DiffuseSrvHeapIndex = 0;
	defaultMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	defaultMat->FresnelR0 = XMFLOAT3(0.09f, 0.09f, 0.09f);
	defaultMat->Roughness = 0.8f;

	mMaterials[defaultMat->Name] = std::move(defaultMat);
}

void D3D12WND::BuildCubeMesh() {

	//이곳에서 Cube
	GeometryGenerator a;
	auto data = a.CreateBox(10, 10, 10, 3);

	std::vector<Vertex> vertices(data.Vertices.size());

	/*이곳은 정점을 vertices에 입력및 경계상자 생성	*/
	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

	for (size_t i = 0; i < data.Vertices.size(); ++i)
	{
		vertices[i].Pos = data.Vertices[i].Position;
		vertices[i].Normal = data.Vertices[i].Normal;
		vertices[i].TexC = data.Vertices[i].TexC;

		XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}

	BoundingBox bounds;
	XMStoreFloat3(&bounds.Center, 0.5f*(vMin + vMax));
	XMStoreFloat3(&bounds.Extents, 0.5f*(vMax - vMin));


	std::vector<std::uint16_t> indices = data.GetIndices16();

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "cubeGeo";

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

	SubmeshGeometry beeSubmesh;
	beeSubmesh.IndexCount = (UINT)indices.size();
	beeSubmesh.StartIndexLocation = 0;
	beeSubmesh.BaseVertexLocation = 0;
	beeSubmesh.Bounds = bounds;

	geo->DrawArgs["cube"] = beeSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void D3D12WND::BuildRenderItems() {
	auto beeRitem = std::make_unique<RenderItem>();
	beeRitem->Geo = mGeometries["cubeGeo"].get();
	beeRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	beeRitem->IndexCount = beeRitem->Geo->DrawArgs["cube"].IndexCount;
	beeRitem->StartIndexLocation = beeRitem->Geo->DrawArgs["cube"].StartIndexLocation;
	beeRitem->BaseVertexLocation = beeRitem->Geo->DrawArgs["cube"].BaseVertexLocation;
	beeRitem->InstancingIndex = 0;

	beeRitem->Instances.resize(1);
	beeRitem->Instances[0].World = MathHelper::Identity4x4();
	beeRitem->Instances[0].TexTransform = MathHelper::Identity4x4();
	beeRitem->Instances[0].MaterialIndex = 0;
	beeRitem->Instances[0].Visible = true;
	beeRitem->VisibleInstanceNum = 1;
	beeRitem->InstanceCount = 1;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(beeRitem.get());

	mAllRitems.push_back(std::move(beeRitem));
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

		// Set the instance buffer to use for this render-item.  For structured buffers, we can bypass 
		// the heap and set as a root descriptor.
		auto instanceBuffer = mCurrFrameResource->InstanceBuffer->Resource();
		mCommandList->SetGraphicsRootShaderResourceView(0, instanceBuffer->GetGPUVirtualAddress() + (ri->InstancingIndex * size));

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
	/*
	readbackDesc.Alignment = 1;
	readbackDesc.DepthOrArraySize = 1;
	readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	readbackDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	readbackDesc.Format = mBackBufferFormat;
	readbackDesc.Height = mClientHeight;
	readbackDesc.Width = mClientWidth;
	readbackDesc.MipLevels = 1;
	readbackDesc.SampleDesc.Count = 1;
	readbackDesc.SampleDesc.Quality = 0;
	*/
	readbackDesc = CD3DX12_RESOURCE_DESC{ CD3DX12_RESOURCE_DESC::Buffer(mSurfaceSize) };
	
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&readbackDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(mSurface.GetAddressOf())
	));

}

void D3D12WND::CheckBuffer() {
	
	FLOAT* buffer;
	D3D12_RANGE renge{ 0, mSurfaceSize };
	mSurface->Map(0, &renge, (void**)&buffer);

	
	//HHH

	mSurface->Unmap(0, 0);
	
}






