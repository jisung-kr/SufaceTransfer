#pragma once
#include <iostream>
#include <fstream>
#include <DirectXPackedVector.h>
#include <DirectXMath.h>
#include <vector>
#include <unordered_map>

#include "AnimationHelper.h"

#define MAX_BUFFER 128
#define MAX_LINE 256
#define MAX_FILENAMELEN 64


struct GeometryAttr {
	std::string DrawArgsName;

};

struct MaterialAttr {
	std::string MaterialName;
	std::string DiffuseTextureName;
	std::string NormalTextureName;
	std::string SpecularTextureName;
	DirectX::XMFLOAT4 DiffuseAlbedo;
	DirectX::XMFLOAT3 FresnelR0;
	float Roughness;
};

struct Transform {
	std::vector< DirectX::XMFLOAT3> Positions;
	std::vector< DirectX::XMFLOAT3> Rotations;
	std::vector< DirectX::XMFLOAT3> Scales;
};

struct RenderItemAttr {
	std::string GeometryName;
	GeometryAttr* GeometryInfo;

	std::string RenderItemName;
	unsigned int InstanceNum;
	unsigned int VisibleInstanceNum;
	std::string IsAnimation;
	float TimePos;
	std::string ClipName;

	//Instance Attr

	std::vector<unsigned int> InstanceIndices;

	std::vector< DirectX::XMFLOAT3> Positions;
	std::vector< DirectX::XMFLOAT3> Rotations;
	std::vector< DirectX::XMFLOAT3> Scales;

	std::vector< DirectX::XMFLOAT3> TexturePositions;
	std::vector< DirectX::XMFLOAT3> TextureRotations;
	std::vector< DirectX::XMFLOAT3> TextureScales;

	std::vector<std::string> MaterialName;
};


struct TimetableAttr {
	std::string RenderItemName;
	std::vector< Keyframe> Keyframes;
};


//�� ������� �� �ؽ�Ʈ ���Ͽ���
//�� ���� �����ϴµ� �ʿ��� ������Ʈ��, �ؽ���/���׸���, ����������, Ÿ�����̺��� �ҷ��´�
class SceneReader {
public:
	SceneReader() = default;

private:
	//�� �ȿ��� �Ʒ��� ���ϸ���� ����
	//������Ʈ��
	//�ؽ���/���׸���
	//���������� ����
	//Ÿ�����̺�

	std::string mGeoMetryFileName;
	std::string mMaterialFileName;
	std::string mRenderItemFileName;
	std::string mTimetableFileName;

	std::unordered_map<std::string, std::vector<GeometryAttr>> mGeometryAttrs;
	std::vector<MaterialAttr> mMaterialAttrs;
	std::vector<RenderItemAttr> mRenderItemAttrs;
	std::vector< TimetableAttr> mTimetableAttrs;

public:
	bool Init(std::string fName);

	//Geometry ���� �б�
	void ReadGeometryFile();
	//Material ���� �б�
	void ReadMaterialFile();
	//RenderItem ���� �б�
	void ReadRenderItemFile();
	//Timetable ���� �б�
	void ReadTimetableFile();

	std::unordered_map<std::string, std::vector<GeometryAttr>>& GetGeometryAttrs() { return mGeometryAttrs; }
	std::vector<MaterialAttr>& GetMaterialAttrs() { return mMaterialAttrs; }
	std::vector<RenderItemAttr>& GetRenderItemAttrs() { return mRenderItemAttrs; }
	std::vector<TimetableAttr>& GetTimetableAttrs() { return mTimetableAttrs; }

	void ShowFileName() {
		std::cout << mGeoMetryFileName << std::endl;
		std::cout << mMaterialFileName << std::endl;
		std::cout << mRenderItemFileName << std::endl;
		std::cout << mTimetableFileName << std::endl;
	}

};