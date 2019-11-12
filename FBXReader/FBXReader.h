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

public:
	enum TextureType {
		DIFFUSE = 0,
		NORMAL,
		SPECULAR
	};

	// Pair[TextureName,TextureFileName]
	using TextureInfo = std::pair<std::string, std::wstring>;
	using TextureBundle = std::unordered_map< TextureType, std::vector<TextureInfo>>;

private:
	FbxManager* mManager = nullptr;
	FbxIOSettings* mIos = nullptr;
	FbxImporter* mImporter = nullptr;
	FbxScene* mScene = nullptr;

	char mFileName[256];

	std::vector<std::pair<std::string, TextureBundle>> mMaterials;

	Skeleton mSkeleton;

	std::vector<SkinnedVertex> mVertex;
	std::vector<uint32_t> mIndex;

	std::vector<SubmeshGeometry> mSubMesh;

	bool mIsSkinned = false;

	SkinnedData mSkinnedData;

	using AnimationInfo = std::pair< std::string, AnimationClip>;
	std::vector<AnimationInfo> mAnimation;

	std::vector<DirectX::XMFLOAT4X4> mBoneOffset;
	std::vector<BoneAnimation> mBoneAniamtions;


public:
	inline FbxNode* GetRootNode() {
		return mScene->GetRootNode();
	}

	std::vector<std::pair<std::string, TextureBundle>>& GetMaterials() { return mMaterials; }

	void LoadFBXData(FbxNode* node, bool isDirectX = true, int inDepth = 0, int myIndex = 0, int inParentIndex = -1);

	void LoadMesh(FbxNode* node, bool isDirectX = true);
	void LoadMaterial(FbxNode* node);

	void LoadSkeletonHierarchy(FbxNode* inRootNode);

	std::vector<SkinnedVertex> GetVertices() { return mVertex; }
	std::vector<uint32_t> GetIndices() { return mIndex; }
	std::vector<SubmeshGeometry> GetSubmesh() { return mSubMesh; }

	void GetSkinnedData(SkinnedData& data);

private:
	void LoadMeshData(FbxNode* node, bool isDirectX = true);

	void LayeredTexture(const FbxProperty& prop, std::vector<TextureInfo>& texBundle);

	DirectX::XMFLOAT3& ReadNormal(FbxMesh* mesh, int controllPointIndex, int vertexCounter);
	DirectX::XMFLOAT3& ReadBinormal(FbxMesh* mesh, int controllPointIndex, int vertexCounter);
	DirectX::XMFLOAT3& ReadTangent(FbxMesh* mesh, int controllPointIndex, int vertexCounter);
	DirectX::XMFLOAT2& ReadUV(FbxMesh* mesh, int controllPointIndex, int vertexCounter);

	void LoadSkeletonHierarchyRecursively(FbxNode* inNode, int inDepth, int myIndex, int inParentIndex);

	unsigned int FindJointIndexUsingName(std::string);
};
