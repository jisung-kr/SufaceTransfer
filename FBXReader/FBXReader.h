#pragma once
#include <Windows.h>
#include <iostream>
#include <fbxsdk.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <DirectXPackedVector.h>
#include "GeometryGenerator.h"
#include "D3DUtil.h"
#include "AnimationHelper.h"
#include "FrameResource.h"

struct ControlPoint {
	SkinnedVertex skinnedData;
	int pairNum = 0;
};

struct Joint {
	int mParentIndex;
	const char* mName;
	DirectX::XMMATRIX mGlobalBindposeInverse;
};

struct Skeleton {
	std::vector<Joint> mJoints;
};

struct BlendingIndexWeightPair {
	unsigned int mBlendingIndex;
	double* mBlendingWeight;
};


class FBXReader {
public:
	FBXReader(const char* fileName = "Bee.fbx");

	~FBXReader();

private:
	FbxManager* mManager = nullptr;
	FbxIOSettings* mIos = nullptr;
	FbxImporter* mImporter = nullptr;
	FbxScene* mScene = nullptr;

	char mFileName[256];

	std::vector<std::string> mMaterialNames;
	std::vector<std::string> mTextureNames;
	std::vector<std::wstring> mTextureFileNames;

	Skeleton mSkeleton;

	std::vector<SkinnedVertex> mVertex;
	std::vector<uint32_t> mIndex;

	GeometryGenerator::MeshData mMeshData;
	std::vector<SubmeshGeometry> mSubMesh;

	SkinnedData mSkinnedData;
	std::string mAnimationName;
	AnimationClip* mAnimClip;

	std::vector<DirectX::XMFLOAT4X4> mBoneOffset;
	std::vector<BoneAnimation> mBoneAniamtions;


public:
	inline FbxNode* GetRootNode() {
		return mScene->GetRootNode();
	}

	std::vector<std::string>& GetMaterialNames() { return mMaterialNames; }
	std::vector<std::string>& GetTextureNames() { return mTextureNames; }
	std::vector<std::wstring>& GetTextureFileNames() { return mTextureFileNames; }

	void LoadFBXData(FbxNode* node, bool isDirectX = true, int inDepth = 0, int myIndex = 0, int inParentIndex = -1);

	void LoadMesh(FbxNode* node, bool isDirectX = true);
	void LoadMaterial(FbxNode* node);

	void LoadSkeletonHierarchy(FbxNode* inRootNode);

	GeometryGenerator::MeshData GetMeshData() { return mMeshData; }
	std::vector<SkinnedVertex> GetVertices() { return mVertex; }
	std::vector<uint32_t> GetIndices() { return mIndex; }
	std::vector<SubmeshGeometry> GetSubmesh() { return mSubMesh; }

	void GetSkinnedData(SkinnedData& data);
	std::string GetClipName() { return mAnimationName; }

private:
	void LoadMeshData(FbxNode* node, bool isDirectX = true);

	void LayeredTexture(const FbxProperty& prop);
	void LoadAnimationData();
	void DisplayAnimation(FbxAnimStack* pAnimStack, FbxNode* pNode, bool isSwitcher = false);
	void DisplayAnimation(FbxAnimLayer* pAnimLayer, FbxNode* pNode, bool isSwitcher = false);
	void DisplayChannels(FbxNode* pNode, FbxAnimLayer* pAnimLayer, int jointIdx, bool isSwitcher = false);

	DirectX::XMFLOAT3& ReadNormal(FbxMesh* mesh, int controllPointIndex, int vertexCounter);
	DirectX::XMFLOAT3& ReadBinormal(FbxMesh* mesh, int controllPointIndex, int vertexCounter);
	DirectX::XMFLOAT3& ReadTangent(FbxMesh* mesh, int controllPointIndex, int vertexCounter);
	DirectX::XMFLOAT2& ReadUV(FbxMesh* mesh, int controllPointIndex, int vertexCounter);

	void LoadSkeletonHierarchyRecursively(FbxNode* inNode, int inDepth, int myIndex, int inParentIndex);

	unsigned int FindJointIndexUsingName(std::string);
};
