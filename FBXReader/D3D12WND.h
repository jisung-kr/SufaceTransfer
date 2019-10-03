#pragma once
#include "IOCP.h"
//#include "Network.h"
#include "BitmapQueue.h"
#include "D3DUtil.h"
#include "GameTimer.h"
#include "DDSTextureLoader.h"
#include "Camera.h"
#include "GeometryGenerator.h"
#include "AnimationHelper.h"
#include "FrameResource.h"
#include "LoadM3d.h"

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")


extern const int gNumFrameResources;


struct SkinnedModelInstance {
	SkinnedData* SkinnedInfo = nullptr;

	std::vector<DirectX::XMFLOAT4X4> FinalTransforms;
	std::string ClipName;

	float TimePos = 0.0f;

	void UpdateSkinnedAnimation(float dt) {
		TimePos += dt;

		if (TimePos > SkinnedInfo->GetClipEndTime(ClipName))
			TimePos = 0.0f;

		SkinnedInfo->GetFinalTransforms(ClipName, TimePos, FinalTransforms);
	}
};

struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	MeshGeometry* Geo = nullptr;
	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;

	DirectX::BoundingBox Bounds;
	std::vector<InstanceData> Instances;

	UINT InstanceNum = 0;
	UINT VisibleInstanceNum = 0;

	UINT SkinnedCBIndex = -1;
	SkinnedModelInstance* SkinnedModelInst = nullptr;
};

/*
struct CustomRenderItem{
	CustomRenderItem() = default;
	CustomRenderItem(const CustomRenderItem& rhs) = delete;

	//각 인스턴스의Transform(위치, 회전값 등등 국소공간변환행렬), 텍스쳐변환행렬, 재질을 설정 가능
	std::vector<InstanceData> Instances; 

	//메쉬데이터
	MeshGeometry* Geo = nullptr; 

	//기본도형
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; 
};
*/
enum class RenderLayer : int{
	None = 0,
	Opaque = 1,
	SkinnedOpaque = 2,
	MAX
};

class D3D12WND {
public:
	D3D12WND(HWND wnd);
	virtual ~D3D12WND() = default;

	D3D12WND& operator=(D3D12WND& rhs) = delete;
	D3D12WND(const D3D12WND& rhs) = delete;

public:
	bool mAppPaused = false;	//for App Paused
	bool mMinimized = false;	//for App  Minimized
	bool mMaximized = false;	//for App Maximized
	bool mResizing = false;	//for App Resizing
	bool mFullscreenState = false;	//for App Full Screening
	GameTimer mTimer; //TImer of App

private:
	D3D12WND* instance = nullptr;
	HWND mhMainWnd;

	std::wstring mMainWndCaption;

	//int mClientWidth = 1280; //App Width
	//int mClientHeight = 720; //App Height

	int mClientWidth = 640; //App Width
	int mClientHeight = 480; //App Height

	/*------------------------------------------------------------------------------------------------------*/
	D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;	//D3D Device
	Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;	//DXGI Factory

	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;	//Fence : Sync CPU - GPU
	UINT64 mCurrentFence = 0;
	/*------------------------------------------------------------------------------------------------------*/
	UINT mRtvDescriptorSize = 0;	//Size of RenderTargetView Descriptor
	UINT mDsvDescriptorSize = 0;	//Size of DepthStencilView Descriptor
	UINT mCbvSrvUavDescriptorSize = 0;	//Size of ConstantBuffer-ShaderResourceView Descriptor
	/*------------------------------------------------------------------------------------------------------*/
	//DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;	//Format of BackBuffer
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;	//Format of BackBuffer - Little Endian
	
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;	//Format of DepthStencil

	Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;	//SwapChain : DoubleBuffering
	static const UINT mSwapChainBufferCount = 2;	// Value = 2, Because of Double Buffering 
	int mCurBackBuffer = 0;	//for Swap Chain

	Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[mSwapChainBufferCount];	//SwapChainBuffer
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;	//DepthStencil Buffer

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;	//RenderTargetView Descriptor Heap
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;	//DepthStencilView Descriptor Heap
	/*------------------------------------------------------------------------------------------------------*/
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;	//Command Queue
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;	//CmdList Allocator
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;	//Command List
	/*------------------------------------------------------------------------------------------------------*/
	D3D12_VIEWPORT mScreenViewport;	//ViewPort
	D3D12_RECT mScissorRect;	//ScissorRect
	/*------------------------------------------------------------------------------------------------------*/
	UINT m4xMsaaQuality;	//4xMsaa Quality
	bool m4xMsaaState = false; //초기값 false로 설정 true시 에러
	/*------------------------------------------------------------------------------------------------------*/
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;
	/*------------------------------------------------------------------------------------------------------*/
	/*	*/
	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::MAX];

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;
	
	/*------------------------------------------------------------------------------------------------------*/

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	/*------------------------------------------------------------------------------------------------------*/
	/*------------------------------------------------------------------------------------------------------*/
	//그려진 텍스쳐 데이터를 받아올 버퍼
	FLOAT* mBuffer;
	std::vector<FLOAT*> mBuffers;
	IOCPServer* server = nullptr;

	PassConstants mClientPassCB;
	std::thread* serverRenderThread = nullptr;
	std::thread* clientRenderThread = nullptr;


	/*------------------------------------------------------------------------------------------------------*/
	PassConstants mMainPassCB;

	Camera mCamera;	//서버의 카메라
	DirectX::BoundingFrustum mCamFrustum;
	bool mFrustumCullingEnabled = true;

	bool isWire_frame = false;

	POINT mLastMousePos;

	/*------------------------------------------------------------------------------------------------------*/
	RenderItem* mBoxRitem = nullptr;
	DirectX::XMFLOAT4X4 mBoxWorld = MathHelper::Identity4x4();
	float mAnimTimePos = 0.0f;
	BoneAnimation mBoxAnimation;

	void DefineBoxAnimation();

	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;
	UINT mSkinnedSrvHeapStart = 0;
	std::string mSkinnedModelFilename = "Models\\soldier.m3d";
	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;
	SkinnedData mSkinnedInfo;
	std::vector<M3DLoader::Subset> mSkinnedSubsets;
	std::vector<M3DLoader::M3dMaterial> mSkinnedMats;
	std::vector<std::string> mSkinnedTextureNames;
	void LoadSkinnedModel();

public:
	Microsoft::WRL::ComPtr<ID3D12Device> GetD3DDevice();
	D3D12WND* GetD3D12WND();
	bool InitDirect3D();
	void CalculateFrameStatus();
	/*------------------------------------------------------------------------------------------------------*/
	void CreateCommandObjects();
	void CreateSwapChain();
	void CreateRTVAndDSVDescriptorHeaps();
	/*------------------------------------------------------------------------------------------------------*/
	void FlushCommandQueue();
	/*------------------------------------------------------------------------------------------------------*/
	ID3D12Resource* CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;
	/*------------------------------------------------------------------------------------------------------*/
	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);
	/*------------------------------------------------------------------------------------------------------*/
	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();

	void BuildShapeGeometry();
	void BuildWorldRenderItem();

	void CreateReadBackTex();

	void CopyBuffer();

	void UpdateClientPassCB(const GameTimer& gt);

	FLOAT* GetReadBackBuffer();
	SIZE_T GetSurfaceSize() { return D3DUtil::CalcConstantBufferByteSize(mClientWidth * sizeof(FLOAT)) * mClientHeight; }
	//SIZE_T GetSurfaceSize() { return mClientWidth * sizeof(FLOAT) * mClientHeight; }

	void SendFrame();
	void RecvRequest();
	void InputPump(const GameTimer& gt);
	void CreateRTVDSV_Server();
	void InitClient();

	/*------------------------------------------------------------------------------------------------------*/
	void OnKeyboardInput(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateInstanceData(const GameTimer& gt);
	void UpdateMaterialBuffer(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateSkinnedCBs(const GameTimer& gt);
	/*------------------------------------------------------------------------------------------------------*/
	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);
	void OnResize();
	float AspectRatio() const;
	/*------------------------------------------------------------------------------------------------------*/
	void Draw(const GameTimer& gt);
	void Update(const GameTimer& gt);
	/*------------------------------------------------------------------------------------------------------*/

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
};