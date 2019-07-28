#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif


#include "GameTimer.h"
#include "D3DUtil.h"
#include "DDSTextureLoader.h"
#include "FrameResource.h"
#include "Camera.h"
#include "GeometryGenerator.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

//std::wstring AnsiToWString(const std::string& str);

extern const int gNumFrameResources;

struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	DirectX::BoundingBox Bounds;
	std::vector<InstanceData> Instances;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT InstancingIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT InstanceCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;

	UINT VisibleInstanceNum = 0;
};

/* 서브매쉬 = 버텍스 버퍼에 들어있는 기하구조를 서브매쉬정보로 나누어둠
	따라서 필요할때만 사용하면 됨.
	필요없으면 사용 x(혹은 설정값을 처음부터 끝까지로 설정해둠)
	렌더링 아이템에 필요한 정보 = 메쉬데이터(여기선 버퍼에 넣어둔 채로 넘겨줌//버퍼의 주소를 가지고 있음)
												재질(Material로 처리, 원하는 텍스쳐와 표면에 대한 데이터를 이용해 재질 표현)
												위치(월드상에서의 위치가 필요함(이 위치는 물체의 국소공간변환행렬이 됨)
												기본도형(보통은 삼각형리스트로 설정)
												텍스쳐변환행렬(텍스쳐를 변환할때, 잘 안쓰이므로 E행렬로 둠)
												인스턴싱은 .... 위치정보를 인스턴싱 구조체로 옮기고 0~n개를 출력, 인스턴싱 갯수 표시!
												*/

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

enum class RenderLayer : int{
	None = 0,
	Opaque = 1
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

	int mClientWidth = 800; //App Width
	int mClientHeight = 600; //App Height

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
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;	//Format of BackBuffer
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
	/**/
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;
	
	/*------------------------------------------------------------------------------------------------------*/
	/*	*/
	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Opaque + 1];

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

	//GPU - CPU메모리 영역에 생성할 자원
	//읽고 쓰고 가능한 텍스쳐 
	Microsoft::WRL::ComPtr<ID3D12Resource> mSurface;
	SIZE_T mSurfaceSize = mClientHeight * mClientWidth * sizeof(float);
	FLOAT* mBuffer;

	/*------------------------------------------------------------------------------------------------------*/
	PassConstants mMainPassCB;

	Camera mCamera;

	bool isWire_frame = false;

	POINT mLastMousePos;


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
	void BuildRenderItems();

	void BuildCubeMesh();

	void CreateReadBackTex();

	void MappingBuffer();
	void UnMapBuffer();

	FLOAT* GetReadBackBuffer();
	int GetReadBackBufferSize();
	

	/*------------------------------------------------------------------------------------------------------*/
	void OnKeyboardInput(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateInstanceData(const GameTimer& gt);
	void UpdateMaterialBuffer(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
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