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


//씬 리더기로 씬 텍스트 파일에서
//한 씬을 구성하는데 필요한 지오메트리, 텍스쳐/메테리얼, 렌더아이템, 타임테이블을 불러온다
class SceneReader {
public:
	SceneReader() = default;

private:
	//씬 안에는 아래의 파일명들이 존재
	//지오메트리
	//텍스쳐/메테리얼
	//렌더아이템 설정
	//타임테이블

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

	//Geometry 파일 읽기
	void ReadGeometryFile();
	//Material 파일 읽기
	void ReadMaterialFile();
	//RenderItem 파일 읽기
	void ReadRenderItemFile();
	//Timetable 파일 읽기
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