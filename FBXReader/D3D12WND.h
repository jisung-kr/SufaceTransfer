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

/* ����Ž� = ���ؽ� ���ۿ� ����ִ� ���ϱ����� ����Ž������� �������
	���� �ʿ��Ҷ��� ����ϸ� ��.
	�ʿ������ ��� x(Ȥ�� �������� ó������ �������� �����ص�)
	������ �����ۿ� �ʿ��� ���� = �޽�������(���⼱ ���ۿ� �־�� ä�� �Ѱ���//������ �ּҸ� ������ ����)
												����(Material�� ó��, ���ϴ� �ؽ��Ŀ� ǥ�鿡 ���� �����͸� �̿��� ���� ǥ��)
												��ġ(����󿡼��� ��ġ�� �ʿ���(�� ��ġ�� ��ü�� ���Ұ�����ȯ����� ��)
												�⺻����(������ �ﰢ������Ʈ�� ����)
												�ؽ��ĺ�ȯ���(�ؽ��ĸ� ��ȯ�Ҷ�, �� �Ⱦ��̹Ƿ� E��ķ� ��)
												�ν��Ͻ��� .... ��ġ������ �ν��Ͻ� ����ü�� �ű�� 0~n���� ���, �ν��Ͻ� ���� ǥ��!
												*/

struct CustomRenderItem{
	CustomRenderItem() = default;
	CustomRenderItem(const CustomRenderItem& rhs) = delete;

	//�� �ν��Ͻ���Transform(��ġ, ȸ���� ��� ���Ұ�����ȯ���), �ؽ��ĺ�ȯ���, ������ ���� ����
	std::vector<InstanceData> Instances; 

	//�޽�������
	MeshGeometry* Geo = nullptr; 

	//�⺻����
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
	bool m4xMsaaState = false; //�ʱⰪ false�� ���� true�� ����
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

	//GPU - CPU�޸� ������ ������ �ڿ�
	//�а� ���� ������ �ؽ��� 
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