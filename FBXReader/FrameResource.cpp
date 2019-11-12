#include "FrameResource.h"


FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT maxInstanceCount, UINT materialCount, UINT maxSkinnedCount, UINT clientNum)
{
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

	if (passCount > 0)
		PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);

	if (materialCount > 0)
		MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(device, materialCount, false);

	if (maxInstanceCount > 0)
		InstanceBuffer = std::make_unique<UploadBuffer<InstanceData>>(device, maxInstanceCount, false);

	if(maxSkinnedCount > 0)
		SkinnedCB = std::make_unique<UploadBuffer<SkinnedConstants>>(device, maxSkinnedCount, true);

	mSurfaces.reserve(clientNum);
	for (int i = 0; i < clientNum; ++i) {
		mSurfaces.push_back(Microsoft::WRL::ComPtr<ID3D12Resource>());
	}
}

FrameResource::~FrameResource()
{

}