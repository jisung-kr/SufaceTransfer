#include "FBXReader.h"
#include <map>

using namespace std;
using namespace DirectX;

FBXReader::FBXReader(const char* fileName) {
	char ext[20];

	sscanf(fileName, "%[^.].%s", mFileName, ext);

	if (strcmp(ext, "fbx") != 0) {
		return;
	}

	mManager = FbxManager::Create();

	mIos = FbxIOSettings::Create(mManager, IOSROOT);

	mManager->SetIOSettings(mIos);

	mImporter = FbxImporter::Create(mManager, "");
	bool status = mImporter->Initialize(fileName, -1, mManager->GetIOSettings());

	if (!status) {
		cout << "Importer Initialize() Error!!" << endl;
		cout << mImporter->GetStatus().GetErrorString() << endl;
		exit(-1);
	}

	mScene = FbxScene::Create(mManager, "scene");
	mImporter->Import(mScene);
}

FBXReader::~FBXReader() {
	mScene->Destroy();
	if (mManager) mManager->Destroy();
}

void FBXReader::LoadFBXData(FbxNode* node, bool isDirectX, int inDepth, int myIndex, int inParentIndex) {
	/*	*/

	//SkeletonHierarchy 로드
	LoadSkeletonHierarchy(node);

	//노드 순회하면서 eMesh데이터 받아오기(정점, 메테리얼 등)
	LoadMeshData(node,isDirectX);

	OutputDebugStringA("안녕하세요\n");
}

void FBXReader::LoadMeshData(FbxNode* node, bool isDirectX) {
	//노드 순회하면서 각자 속성에 맞게 처리
	FbxNodeAttribute* attr = node->GetNodeAttribute();

	if (attr != nullptr) {
		if (attr->GetAttributeType() == FbxNodeAttribute::eMesh) {
			//서브메쉬를 생성해서 각 부분에 맞게 메테리얼과 정점 데이터 생성
			LoadMaterial(node);
			LoadMesh(node, isDirectX);
		}
	}

	//트리 순회
	int childCount = node->GetChildCount();
	for (int i = 0; i < childCount; ++i) {
		LoadMeshData(node->GetChild(i));
	}
}



void FBXReader::LoadMaterial(FbxNode* node) {
	int materialCount = node->GetMaterialCount();

	for (int index = 0; index < materialCount; index++)
	{
		FbxSurfaceMaterial* material = (FbxSurfaceMaterial*)node->GetSrcObject<FbxSurfaceMaterial>(index);

		if (material != NULL)
		{
			cout << "\nmaterial: " << material->GetName() << std::endl;
			OutputDebugStringA(material->GetName());
			OutputDebugStringA(" :\t");
			mMaterialNames.push_back(material->GetName());

			// This only gets the material of type sDiffuse, you probably need to traverse all Standard Material Property by its name to get all possible textures.
			FbxProperty prop = material->FindProperty(FbxSurfaceMaterial::sDiffuse);
			//FbxProperty prop2 = material->FindProperty(FbxSurfaceMaterial::sNormalMap);
			//FbxProperty prop3 = material->FindProperty(FbxSurfaceMaterial::sSpecular);
			//FbxProperty prop4 = material->FindProperty(FbxSurfaceMaterial::sEmissive);
	
			// Check if it's layeredtextures
			LayeredTexture(prop);
			//LayeredTexture(prop2);
			//LayeredTexture(prop3);
			//LayeredTexture(prop4);
		}
	}
}

void FBXReader::LayeredTexture(const FbxProperty& prop) {
	int layeredTextureCount = prop.GetSrcObjectCount<FbxLayeredTexture>();

	if (layeredTextureCount > 0)
	{
		for (int j = 0; j < layeredTextureCount; j++)
		{
			FbxLayeredTexture* layered_texture = FbxCast<FbxLayeredTexture>(prop.GetSrcObject<FbxLayeredTexture>(j));
			int lcount = layered_texture->GetSrcObjectCount<FbxTexture>();

			for (int k = 0; k < lcount; k++)
			{
				FbxTexture* texture = FbxCast<FbxTexture>(layered_texture->GetSrcObject<FbxTexture>(k));
				// Then, you can get all the properties of the texture, include its name
				const char* textureName = texture->GetName();
				cout << textureName;

				char tName[1024] = { 0, };
				sprintf(tName, "%s_%s", mFileName, prop.GetName());
				mTextureNames.push_back(tName);
				mTextureFileNames.push_back(AnsiToWString(textureName));

				OutputDebugStringA(textureName);
				OutputDebugStringA("\n");
			}
		}
	}
	else
	{
		// Directly get textures
		int textureCount = prop.GetSrcObjectCount<FbxTexture>();
		for (int j = 0; j < textureCount; j++)
		{
			FbxFileTexture* texture = FbxCast<FbxFileTexture>(prop.GetSrcObject<FbxTexture>(j));
			// Then, you can get all the properties of the texture, include its name
			const char* textureName = texture->GetFileName();//texture->GetName();
			cout << textureName;
			char tName[1024] = { 0, };
			sprintf(tName, "%s_%s", mFileName, prop.GetName());
			mTextureNames.push_back(tName);
			mTextureFileNames.push_back(AnsiToWString(textureName));

			OutputDebugStringA(textureName);
			OutputDebugStringA(prop.GetName());
			OutputDebugStringA("\n");
				
		}
	}
}
FbxAMatrix GetGeometryTransformation(FbxNode* inNode)
{
	if (!inNode)
	{
		throw std::exception("Null for mesh geometry");
	}

	const FbxVector4 lT = inNode->GetGeometricTranslation(FbxNode::eSourcePivot);
	const FbxVector4 lR = inNode->GetGeometricRotation(FbxNode::eSourcePivot);
	const FbxVector4 lS = inNode->GetGeometricScaling(FbxNode::eSourcePivot);

	return FbxAMatrix(lT, lR, lS);
}

unsigned int FBXReader::FindJointIndexUsingName(std::string name) {

	for (int i = 0; i < mSkeleton.mJoints.size(); ++i) {
		if (strcmp(mSkeleton.mJoints[i].mName, name.c_str()) == 0) {
			return i;
		}
	}

	return 0;
}

DirectX::XMMATRIX LoadFBXMatrix(const FbxMatrix& matrix) {
	
	DirectX::XMFLOAT4X4 gBindposeInv(matrix.Get(0, 0), matrix.Get(0, 1), matrix.Get(0, 2), matrix.Get(0, 3),
									matrix.Get(1, 0), matrix.Get(1, 1), matrix.Get(1, 2), matrix.Get(1, 3),
									matrix.Get(2, 0), matrix.Get(2, 1), matrix.Get(2, 2), matrix.Get(2, 3),
									matrix.Get(3, 0), matrix.Get(3, 1), matrix.Get(3, 2), matrix.Get(3, 3));

	return std::move(XMLoadFloat4x4(&gBindposeInv));
}


void FBXReader::LoadMesh(FbxNode* node, bool isDirectX) {

	//메쉬 데이터를 받아옴
	GeometryGenerator::MeshData data;
	SubmeshGeometry subData;

	FbxMesh* mesh = node->GetMesh();
	int count = mesh->GetControlPointsCount();
	
	ControlPoint* positions = new ControlPoint[count];
	/*제어점 정보 받아오기 (제어점의 위치)*/
	for (int i = 0; i < count; ++i) {
		positions[i].skinnedData.Pos.x = static_cast<float>(mesh->GetControlPointAt(i).mData[0]);
		positions[i].skinnedData.Pos.y = static_cast<float>(mesh->GetControlPointAt(i).mData[1]);
		positions[i].skinnedData.Pos.z = static_cast<float>(mesh->GetControlPointAt(i).mData[2]);
		memset(&positions[i].skinnedData.BoneIndices, 0x00, sizeof(positions[i].skinnedData.BoneIndices));
		positions[i].skinnedData.BoneWeights = XMFLOAT3(0.0f, 0.0f, 0.0f);
	}

	//가중치 데이터 넣기
	FbxAMatrix geometryTransform = GetGeometryTransformation(node);
	unsigned int numOfDeformers = mesh->GetDeformerCount();
	for (unsigned int deformerIndex = 0; deformerIndex < numOfDeformers; ++deformerIndex)
	{
		// There are many types of deformers in Maya,
		// We are using only skins, so we see if this is a skin
		FbxSkin* currSkin = reinterpret_cast<FbxSkin*>(mesh->GetDeformer(deformerIndex, FbxDeformer::eSkin));
		if (!currSkin)
		{
			continue;
		}

		unsigned int numOfClusters = currSkin->GetClusterCount();
		for (unsigned int clusterIndex = 0; clusterIndex < numOfClusters; ++clusterIndex)
		{
			FbxCluster* currCluster = currSkin->GetCluster(clusterIndex);
			std::string currJointName = currCluster->GetLink()->GetName();
			unsigned int currJointIndex = FindJointIndexUsingName(currJointName);

			FbxAMatrix transformMatrix;
			FbxAMatrix transformLinkMatrix;
			FbxAMatrix globalBindposeInverseMatrix;

			currCluster->GetTransformMatrix(transformMatrix);	// The transformation of the mesh at binding time
			currCluster->GetTransformLinkMatrix(transformLinkMatrix);	// The transformation of the cluster(joint) at binding time from joint space to world space
			globalBindposeInverseMatrix = transformLinkMatrix.Inverse() * transformMatrix * geometryTransform;
			
			FbxAMatrix temp = currCluster->GetLink()->EvaluateGlobalTransform();

			FbxVector4 scale = temp.GetS();
			FbxVector4 rotation = temp.GetR();
			FbxVector4 translation = temp.GetT();

			//mSkeleton.mJoints[currJointIndex].mGlobalBindposeInverse = LoadFBXMatrix(temp);


			//가중치와 인덱스 받아오기
			unsigned int numOfIndices = currCluster->GetControlPointIndicesCount();
			if (numOfIndices > 0) {
				for (unsigned int i = 0; i < numOfIndices; ++i)
				{
					auto& curCntPt = positions[currCluster->GetControlPointIndices()[i]];

					double* weight = currCluster->GetControlPointWeights();

					if (curCntPt.pairNum < 4) {
						curCntPt.skinnedData.BoneIndices[curCntPt.pairNum++] = currJointIndex;
						curCntPt.skinnedData.BoneWeights.x = weight[0];
						curCntPt.skinnedData.BoneWeights.y = weight[1];
						curCntPt.skinnedData.BoneWeights.z = weight[2];
					}

				}

				//뼈대 갯수만큼 BoneAnimation을 만들고 Clip안에 인덱스에 맞게 넣기
				//현재 currJointIndex가 뼈대의 인덱스

				//키프레임 생성 후 


				//애니메이션 키프레임 값 받아오기
				// Get animation information
				// Now only supports one take

				int numStacks = mScene->GetSrcObjectCount<FbxAnimStack>();

				if (numStacks > 0) {

					FbxAnimStack* currAnimStack = (FbxAnimStack*)mScene->GetSrcObject<FbxAnimStack>();

					FbxString animStackName = currAnimStack->GetName();
					mAnimationName = animStackName.Buffer();

					FbxTakeInfo* takeInfo = mScene->GetTakeInfo(animStackName);

					fbxsdk::FbxTime Time = takeInfo->mLocalTimeSpan.GetDuration().GetFrameCount();
					fbxsdk::FbxTime start = takeInfo->mLocalTimeSpan.GetStart();
					fbxsdk::FbxTime end = takeInfo->mLocalTimeSpan.GetStop();

					int mAnimationLength = end.GetFrameCount(fbxsdk::FbxTime::eFrames30) - start.GetFrameCount(fbxsdk::FbxTime::eFrames30) + 1;
					FbxAMatrix matNowMatrix;
					FbxAMatrix matBeforeMatrix;
					BoneAnimation boneAnimation;
					for (FbxLongLong i = start.GetFrameCount(fbxsdk::FbxTime::eFrames30); i <= end.GetFrameCount(fbxsdk::FbxTime::eFrames30); ++i)
					{
						Keyframe keyFrame;
						//KeyFrame에서 변화값과 시간 값을 받아오기
						fbxsdk::FbxTime currTime;
						currTime.SetFrame(i, fbxsdk::FbxTime::eFrames30);

						FbxAMatrix currentTransformOffset = node->EvaluateGlobalTransform(currTime) * geometryTransform;
						FbxAMatrix currentGlobalTransform = currentTransformOffset.Inverse() * currCluster->GetLink()->EvaluateGlobalTransform(currTime);
						
						matNowMatrix = currCluster->GetLink()->EvaluateLocalTransform(currTime);
						FbxAMatrix a = currCluster->GetLink()->EvaluateGlobalTransform(currTime);
						if (matNowMatrix == matBeforeMatrix) {
							continue;
						}

						FbxVector4 scale = currentTransformOffset.GetS();
						FbxVector4 rotation = currentTransformOffset.GetR();
						FbxVector4 translation = currentTransformOffset.GetT();

						//translation.Set(translation.mData[0], translation.mData[1], -translation.mData[2]); // This negate Z of Translation Component of the matrix
						//rotation.Set(-rotation.mData[0], -rotation.mData[1], rotation.mData[2]);

						keyFrame.TimePos = currTime.GetFramedTime().GetMilliSeconds();
						keyFrame.Scale.x = scale.mData[0];	keyFrame.Scale.y = scale.mData[1];	keyFrame.Scale.z = scale.mData[2];
						keyFrame.RotationQuat.x = rotation.mData[0];	keyFrame.RotationQuat.y = rotation.mData[1];	keyFrame.RotationQuat.z = rotation.mData[2];	keyFrame.RotationQuat.z = rotation.mData[3];
						keyFrame.Translation.x = translation.mData[0];	keyFrame.Translation.y = translation.mData[1];	keyFrame.Translation.z = translation.mData[2];

						boneAnimation.Keyframes.push_back(keyFrame);
						matBeforeMatrix = matNowMatrix;
					}

					mAnimClip.BoneAnimations.push_back(boneAnimation);
				}
			}
			
			
		}
	}

	//Pos, Normal, Uv, TexC 값 계산
	int triCount = mesh->GetPolygonCount();
	int vertexCount = 0;

	unordered_map<GeometryGenerator::Vertex, uint32_t> findVertex;

	for (int i = 0; i < triCount; ++i) {
		for (int j = 0; j < 3; ++j) {
			int controllPointIndex = mesh->GetPolygonVertex(i, j);

			GeometryGenerator::Vertex temp; //임시로 정점데이터를 담을 객체
			SkinnedVertex tSkinnedVtx;	//임시로 스킨드정점 데이터를 넣음

			//정점
			temp.Position = positions[controllPointIndex].skinnedData.Pos; //정점데이터
			//노멀
			temp.Normal = ReadNormal(mesh, controllPointIndex, vertexCount);
			//바이노멀
			temp.BiNormal = ReadBinormal(mesh, controllPointIndex, vertexCount);
			//탄젠트
			temp.TangentU = ReadTangent(mesh, controllPointIndex, vertexCount);
			//텍스쳐UV
			auto tempUV = ReadUV(mesh, controllPointIndex, mesh->GetTextureUVIndex(i, j));

			if (isDirectX) {
				tempUV.y = 1.0 - tempUV.y;
			}

			temp.TexC = tempUV;

			//스킨드정점변수로 복사
			tSkinnedVtx.Pos = temp.Position;
			tSkinnedVtx.Normal = temp.Normal;
			tSkinnedVtx.TangentU = temp.TangentU;
			tSkinnedVtx.TexC = temp.TexC;
			tSkinnedVtx.BoneWeights = positions[controllPointIndex].skinnedData.BoneWeights;
			memcpy(&tSkinnedVtx.BoneIndices, &positions[controllPointIndex].skinnedData.BoneIndices, sizeof(positions[controllPointIndex].skinnedData.BoneIndices));

			//정점 중복체크
			auto r = findVertex.find(temp);
			int index = 0;
			/*	*/
			//중복이면 이미 있는 인덱스를 추가
			if (r != findVertex.end()) {
				data.Indices32.push_back(r->second);
				mIndex.push_back(r->second);

			}
			else {
				//중복이 아니면 인덱스 값 계산 후 저장
				index = data.Vertices.size();
				findVertex[temp] = index;
				data.Indices32.push_back(index);
				data.Vertices.push_back(temp);

				mIndex.push_back(index);
				mVertex.push_back(tSkinnedVtx);
			}
		

			++vertexCount;
		}
	}

	//서브메시 속성들 계산
	int totVertexCount = 0;
	totVertexCount += mMeshData.Vertices.size();

	int totIndexCount = 0;
	for (int i = 0; i < mSubMesh.size(); ++i) {
		totIndexCount += mSubMesh[i].IndexCount;
	}

	subData.IndexCount = data.Indices32.size();
	subData.BaseVertexLocation = totVertexCount;
	subData.StartIndexLocation = totIndexCount;

	//전체 메쉬데이터에 현재 서브메쉬 추가
	mMeshData.Vertices.insert(mMeshData.Vertices.end(), data.Vertices.begin(), data.Vertices.end());
	mMeshData.Indices32.insert(mMeshData.Indices32.end(), data.Indices32.begin(), data.Indices32.end());

	mSubMesh.push_back(std::move(subData));

	cout << "ReadMesh End" << endl;


	return;
}


XMFLOAT3& FBXReader::ReadNormal(FbxMesh* mesh, int controllPointIndex, int vertexCounter) {
	XMFLOAT3* temp = new XMFLOAT3();

	if (mesh->GetElementNormalCount() < 1) {
		cout << "Invalid Number" << endl;
		return *temp;
	}

	fbxsdk::FbxGeometryElementNormal* vertexNormal = mesh->GetElementNormal(0);

	switch (vertexNormal->GetMappingMode()) {
	case FbxGeometryElement::eByControlPoint:
		switch (vertexNormal->GetReferenceMode()) {
		case FbxGeometryElement::eDirect:
		{
			temp->x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(controllPointIndex).mData[0]);
			temp->y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(controllPointIndex).mData[1]);
			temp->z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(controllPointIndex).mData[2]);
		}

		break;
		case FbxGeometryElement::eIndexToDirect: 
		{
			int index = vertexNormal->GetIndexArray().GetAt(controllPointIndex);
			temp->x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[0]);
			temp->y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[1]);
			temp->z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[2]);
		}

		break;
		
		default: 
		{
			cout << "Error:Invlid Vertex ReferenceMode" << endl;
		}

		}
		break;
	case FbxGeometryElement::eByPolygonVertex:
		switch (vertexNormal->GetReferenceMode()) {
		case FbxGeometryElement::eDirect: 
		{
			temp->x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(vertexCounter).mData[0]);
			temp->y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(vertexCounter).mData[1]);
			temp->z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(vertexCounter).mData[2]);
		}

		break;
		case FbxGeometryElement::eIndexToDirect: 
		{
			int index = vertexNormal->GetIndexArray().GetAt(vertexCounter);
			temp->x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[0]);
			temp->y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[1]);
			temp->z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[2]);
		}

		break;
		
		default: {
			cout << "Error:Invlid Vertex ReferenceMode" << endl;
		}

		}
		break;
	}

	return *temp;

}
XMFLOAT3& FBXReader::ReadBinormal(FbxMesh* mesh, int controllPointIndex, int vertexCounter) {
	XMFLOAT3* temp = new XMFLOAT3();

	if (mesh->GetElementBinormalCount() < 1) {
		cout << "Invalid Number" << endl;
		return *temp;
	}


	fbxsdk::FbxGeometryElementBinormal* vertexBinormal = mesh->GetElementBinormal(0);

	switch (vertexBinormal->GetMappingMode()) {
	case FbxGeometryElement::eByControlPoint:
		switch (vertexBinormal->GetReferenceMode()) {
		case FbxGeometryElement::eDirect:
		{
			temp->x = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(controllPointIndex).mData[0]);
			temp->y = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(controllPointIndex).mData[1]);
			temp->z = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(controllPointIndex).mData[2]);
		}

		break;
		case FbxGeometryElement::eIndexToDirect: 
		{
			int index = vertexBinormal->GetIndexArray().GetAt(controllPointIndex);
			temp->x = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[0]);
			temp->y = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[1]);
			temp->z = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[2]);
		}

		break;
		
		default: 
		{
			cout << "Error:Invlid Vertex ReferenceMode" << endl;
		}
	
		}

		break;
	case FbxGeometryElement::eByPolygonVertex:
		switch (vertexBinormal->GetReferenceMode()) {
		case FbxGeometryElement::eDirect:
		{
			temp->x = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(vertexCounter).mData[0]);
			temp->y = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(vertexCounter).mData[1]);
			temp->z = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(vertexCounter).mData[2]);
		}

		break;
		case FbxGeometryElement::eIndexToDirect: 
		{
			int index = vertexBinormal->GetIndexArray().GetAt(vertexCounter);
			temp->x = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[0]);
			temp->y = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[1]);
			temp->z = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[2]);
		}

		break;
		
		default:
		{
			cout << "Error:Invlid Vertex ReferenceMode" << endl;
		}

		}

		break;
	}

	return *temp;

}
XMFLOAT3& FBXReader::ReadTangent(FbxMesh* mesh, int controllPointIndex, int vertexCounter) {
	XMFLOAT3* temp = new XMFLOAT3();

	if (mesh->GetElementTangentCount() < 1) {
		cout << "Invalid Number" << endl;
		return *temp;
	}

	fbxsdk::FbxGeometryElementTangent* vertexTangent = mesh->GetElementTangent(0);

	switch (vertexTangent->GetMappingMode()) {
	case FbxGeometryElement::eByControlPoint:
		switch (vertexTangent->GetReferenceMode()) {
		case FbxGeometryElement::eDirect:
		{
			temp->x = static_cast<float>(vertexTangent->GetDirectArray().GetAt(controllPointIndex).mData[0]);
			temp->y = static_cast<float>(vertexTangent->GetDirectArray().GetAt(controllPointIndex).mData[1]);
			temp->z = static_cast<float>(vertexTangent->GetDirectArray().GetAt(controllPointIndex).mData[2]);
		}

		break;
		case FbxGeometryElement::eIndexToDirect: 
		{
			int index = vertexTangent->GetIndexArray().GetAt(controllPointIndex);
			temp->x = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[0]);
			temp->y = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[1]);
			temp->z = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[2]);
		}

		break;
		
		default:
		{
			cout << "Error:Invlid Vertex ReferenceMode" << endl;
		}

		}
		break;
	case FbxGeometryElement::eByPolygonVertex:
		switch (vertexTangent->GetReferenceMode()) {
		case FbxGeometryElement::eDirect:
		{
			temp->x = static_cast<float>(vertexTangent->GetDirectArray().GetAt(vertexCounter).mData[0]);
			temp->y = static_cast<float>(vertexTangent->GetDirectArray().GetAt(vertexCounter).mData[1]);
			temp->z = static_cast<float>(vertexTangent->GetDirectArray().GetAt(vertexCounter).mData[2]);
		}

		break;
		case FbxGeometryElement::eIndexToDirect: 
		{
			int index = vertexTangent->GetIndexArray().GetAt(vertexCounter);
			temp->x = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[0]);
			temp->y = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[1]);
			temp->z = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[2]);
		}

		break;
		
		default:
		{
			cout << "Error:Invlid Vertex ReferenceMode" << endl;
		}

		}
		break;
	}

	return *temp;
}
XMFLOAT2& FBXReader::ReadUV(FbxMesh* mesh, int controllPointIndex, int vertexCounter) {
	XMFLOAT2* temp = new XMFLOAT2();

	if (mesh->GetElementUVCount() < 1) {
		cout << "Invalid Number" << endl;
		return *temp;
	}

	fbxsdk::FbxGeometryElementUV* vertexUV = mesh->GetElementUV(0);

	switch (vertexUV->GetMappingMode()) {
	case FbxGeometryElement::eByControlPoint:
		switch (vertexUV->GetReferenceMode()) {
		case FbxGeometryElement::eDirect:
		{
			temp->x = static_cast<float>(vertexUV->GetDirectArray().GetAt(controllPointIndex).mData[0]);
			temp->y = static_cast<float>(vertexUV->GetDirectArray().GetAt(controllPointIndex).mData[1]);
		}

		break;
		case FbxGeometryElement::eIndexToDirect: 
		{
			int index = vertexUV->GetIndexArray().GetAt(controllPointIndex);
			temp->x = static_cast<float>(vertexUV->GetDirectArray().GetAt(index).mData[0]);
			temp->y = static_cast<float>(vertexUV->GetDirectArray().GetAt(index).mData[1]);
		}

		break;
		
		default:
		{
			cout << "Error:Invlid Vertex ReferenceMode" << endl;
		}

		}
		break;
	case FbxGeometryElement::eByPolygonVertex:
		switch (vertexUV->GetReferenceMode()) {
		case FbxGeometryElement::eDirect:
		{
			temp->x = static_cast<float>(vertexUV->GetDirectArray().GetAt(vertexCounter).mData[0]);
			temp->y = static_cast<float>(vertexUV->GetDirectArray().GetAt(vertexCounter).mData[1]);
		}

		break;
		case FbxGeometryElement::eIndexToDirect: 
		{
			int index = vertexUV->GetIndexArray().GetAt(vertexCounter);
			temp->x = static_cast<float>(vertexUV->GetDirectArray().GetAt(index).mData[0]);
			temp->y = static_cast<float>(vertexUV->GetDirectArray().GetAt(index).mData[1]);
		}

		break;
		
		default:
		{
			cout << "Error:Invlid Vertex ReferenceMode" << endl;
		}

		}
		break;
	}

	return *temp;
}


void FBXReader::LoadSkeletonHierarchy(FbxNode* inRootNode)
{
	for (int childIndex = 0; childIndex < inRootNode->GetChildCount(); ++childIndex)
	{
		FbxNode* currNode = inRootNode->GetChild(childIndex);
		LoadSkeletonHierarchyRecursively(currNode, 0, 0, -1);
	}

}

void FBXReader::LoadSkeletonHierarchyRecursively(FbxNode* inNode, int inDepth, int myIndex, int inParentIndex)
{
	if (inNode->GetNodeAttribute() && inNode->GetNodeAttribute()->GetAttributeType() && inNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
	{
		Joint currJoint;
		currJoint.mParentIndex = inParentIndex;
		currJoint.mName = inNode->GetName();
		currJoint.mGlobalBindposeInverse = LoadFBXMatrix(inNode->EvaluateLocalTransform());
		mSkeleton.mJoints.push_back(currJoint);
	}
	for (int i = 0; i < inNode->GetChildCount(); i++)
	{
		LoadSkeletonHierarchyRecursively(inNode->GetChild(i), inDepth + 1, mSkeleton.mJoints.size(), myIndex);
	}
}

void FBXReader::GetSkinnedData(SkinnedData& data) {
	if (mSkeleton.mJoints.size() > 0) {
		std::vector<int>* hierachy = new std::vector<int>();
		std::vector<DirectX::XMFLOAT4X4 >* offset = new std::vector<DirectX::XMFLOAT4X4>();
		std::unordered_map<std::string, AnimationClip> clip;

		for (int i = 0; i < mSkeleton.mJoints.size(); ++i) {
			hierachy->push_back(mSkeleton.mJoints[i].mParentIndex);
			XMFLOAT4X4 temp;
			XMStoreFloat4x4(&temp, mSkeleton.mJoints[i].mGlobalBindposeInverse);
			offset->push_back(temp);
		}
		clip[mAnimationName] = mAnimClip;

		data.Set(*hierachy, *offset, clip);
	}
	else {

	}
}