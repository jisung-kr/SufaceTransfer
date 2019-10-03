#include "FrameResource.h"


FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT maxInstanceCount, UINT materialCount, UINT clientNum)
{
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

	PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);

	MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(device, materialCount, false);
	InstanceBuffer = std::make_unique<UploadBuffer<InstanceData>>(device, maxInstanceCount, false);
	SkinnedCB = std::make_unique<UploadBuffer<SkinnedConstants>>(device, clientNum + 1, true);

	mSurfaces.reserve(clientNum);
	for (int i = 0; i < clientNum; ++i) {
		mSurfaces.push_back(Microsoft::WRL::ComPtr<ID3D12Resource>());
	}
}

FrameResource::~FrameResource()
{

}