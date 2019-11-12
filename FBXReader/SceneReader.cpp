#include "SceneReader.h"


using namespace std;

bool SceneReader::Init(std::string fName) {
	//File 읽기

	FILE* scene;
	fopen_s(&scene, fName.c_str(), "rt");

	if (scene == nullptr) {
		cerr << "Can't Open File" << endl;
		return false;
	}

	char geometryFileName[MAX_FILENAMELEN] = { 0, };
	char materialFileName[MAX_FILENAMELEN] = { 0, };
	char renderItemFileName[MAX_FILENAMELEN] = { 0, };
	char timetableFileName[MAX_FILENAMELEN] = { 0, };

	fscanf_s(scene, "GeometryFile:%s\n", geometryFileName, MAX_FILENAMELEN);
	fscanf_s(scene, "MaterialFile:%s\n", materialFileName, MAX_FILENAMELEN);
	fscanf_s(scene, "RenderIteFile:%s\n", renderItemFileName, MAX_FILENAMELEN);
	fscanf_s(scene, "TimeTableFile:%s\n", timetableFileName, MAX_FILENAMELEN);

	mGeoMetryFileName.assign(geometryFileName);
	mMaterialFileName.assign(materialFileName);
	mRenderItemFileName.assign(renderItemFileName);
	mTimetableFileName.assign(timetableFileName);

	return true;
}


void SceneReader::ReadGeometryFile() {
	//File 읽기
	std::ifstream in(mGeoMetryFileName.c_str());

	if (!in.is_open()) {
		cerr << "Can't Open File" << endl;
		return;
	}

	char geoName[MAX_BUFFER] = { 0, };

	//파일 한 라인씩 읽기
	while (in) {
		char str[MAX_LINE];	//라인을 읽기 위한 버퍼
		char check = 0x00;	//계층구조 파악용 체크변수 라인의 \t가 존재하면 계층구조 있음

		in.getline(str, MAX_LINE);
		check = str[0];

		if (check != '\t') {
			//GeoName
			sscanf_s(str, "%s\n", geoName, MAX_BUFFER);

			mGeometryAttrs[geoName];
		}
		else {
			//Geo-Arge Info
			GeometryAttr attr;	//속성

			//ArgsName
			char argsName[MAX_BUFFER] = { 0, };
			sscanf_s(str, "\t%s\n", argsName, MAX_BUFFER);
			attr.DrawArgsName.assign(argsName);




			mGeometryAttrs[geoName].push_back(attr);
		}

	}

	std::cout << "Success Reading GeometryFile!" << std::endl;
	in.close();
}

void SceneReader::ReadMaterialFile() {
	//File 읽기
	std::ifstream in(mMaterialFileName.c_str());

	if (!in.is_open()) {
		cerr << "Can't Open File" << endl;
		return;
	}

	while (in) {
		MaterialAttr attr;	//속성
		char str[MAX_LINE];	//한 라인을 읽기 위한 버퍼

		//MaterialName 
		in.getline(str, MAX_LINE);

		if (strcmp(str, "") == 0)
			continue;

		char materialName[MAX_BUFFER] = { 0, };
		sscanf_s(str, "Name:%s\n", materialName, MAX_BUFFER);
		attr.MaterialName.assign(materialName);

		//MaterialInfo - Diffuse
		in.getline(str, MAX_LINE);
		char diffuseTexName[MAX_BUFFER] = { 0, };
		sscanf_s(str, "\tDiffuse:%s\n", diffuseTexName, MAX_BUFFER);
		attr.DiffuseTextureName.assign(diffuseTexName);

		//MaterialInfo - Normal
		in.getline(str, MAX_LINE);
		char normalTexName[MAX_BUFFER] = { 0, };
		sscanf_s(str, "\tNormal:%s\n", normalTexName, MAX_BUFFER);
		attr.NormalTextureName.assign(normalTexName);

		//MaterialInfo - Specular
		in.getline(str, MAX_LINE);
		char specTexName[MAX_BUFFER] = { 0, };
		sscanf_s(str, "\tSpecular:%s\n", specTexName, MAX_BUFFER);
		attr.SpecularTextureName.assign(specTexName);

		//DiffuseAlbedo
		{
			in.getline(str, MAX_LINE);
			float x = 0, y = 0, z = 0, w = 0;
			sscanf_s(str, "\tDiffuseAlbedo:%f %f %f %f\n", &x, &y, &z, &w);
			attr.DiffuseAlbedo = DirectX::XMFLOAT4(x, y, z, w);
		}

		//Fresnel
		{
			in.getline(str, MAX_LINE);
			float x = 0, y = 0, z = 0;
			sscanf_s(str, "\tFresnelR0:%f %f %f\n", &x, &y, &z);
			attr.FresnelR0 = DirectX::XMFLOAT3(x, y, z);
		}

		//Roughness
		{
			in.getline(str, MAX_LINE);
			float roughness;
			sscanf_s(str, "\tRoughness:%f\n", &roughness);
			attr.Roughness = roughness;
		}

		mMaterialAttrs.push_back(attr);
	}

	std::cout << "Success Reading MaterialFile!" << std::endl;
	in.close();
}

void SceneReader::ReadRenderItemFile() {
	//File 읽기
	std::ifstream in(mRenderItemFileName.c_str());

	if (!in.is_open()) {
		cerr << "Can't Open File" << endl;
		return;
	}

	char geoName[MAX_BUFFER];
	RenderItemAttr attr;	//속성

	while (in) {

		char str[MAX_LINE];	//한 라인을 읽기 위한 버퍼
		char check = 0x00;

		in.getline(str, MAX_LINE);
		check = str[0];

		if (check != '\t') {
			//RenderItem GeoName 
			sscanf_s(str, "%s\n", geoName, MAX_BUFFER);

			attr.GeometryName = geoName;
		}
		else {
			//RenderItem ArgsName
			char argsName[MAX_BUFFER] = { 0, };
			sscanf_s(str, "\t%s\n", argsName, MAX_BUFFER);

			auto& curGeo = mGeometryAttrs[geoName];

			for (auto& e : curGeo) {
				if (e.DrawArgsName.compare(argsName) == 0) {
					attr.GeometryInfo = &e;
					break;
				}
			}

			//InstNum
			in.getline(str, MAX_LINE);
			unsigned int instNum = 0;
			sscanf_s(str, "\tInstanceNum:%d\n", &instNum);
			attr.InstanceNum = instNum;

			//VisibleInstNum
			in.getline(str, MAX_LINE);
			unsigned int visibleInstNum = 0;
			sscanf_s(str, "\tVisibleInstanceNum:%d\n", &visibleInstNum);

			if (instNum <= visibleInstNum)
				attr.VisibleInstanceNum = visibleInstNum;
			else
				attr.VisibleInstanceNum = 0;

			//IsAnim
			in.getline(str, MAX_LINE);
			char isAnim[MAX_BUFFER] = { 0, };
			sscanf_s(str, "\tIsAnim:%s\n", isAnim, MAX_BUFFER);
			attr.IsAnimation.assign(isAnim);

			//TimePos
			in.getline(str, MAX_LINE);
			float timePos = 0.0f;
			sscanf_s(str, "\tTimePos:%f\n", &timePos);
			attr.TimePos = timePos;

			//ClipName
			in.getline(str, MAX_LINE);
			char clipName[MAX_BUFFER] = { 0, };
			sscanf_s(str, "\tClipName:%s\n", clipName, MAX_BUFFER);
			attr.ClipName.assign(clipName);

			for (unsigned int i = 0; i < attr.InstanceNum; ++i) {
				//InstanceIndex
				in.getline(str, MAX_LINE);
				unsigned int InstanceNum = 0;
				sscanf_s(str, "\t\tInstanceIndex:%u\n", &InstanceNum);

				attr.InstanceIndices.push_back(InstanceNum);

				//RenderItem World - Position
				{
					in.getline(str, MAX_LINE);
					float x = 0, y = 0, z = 0;
					sscanf_s(str, "\t\tPosition:%f %f %f\n", &x, &y, &z);

					attr.Positions.push_back(DirectX::XMFLOAT3(x, y, z));
				}

				//RenderItem World - Rotation
				{
					in.getline(str, MAX_LINE);
					float x = 0, y = 0, z = 0;
					sscanf_s(str, "\t\tRotation:%f %f %f\n", &x, &y, &z);

					attr.Rotations.push_back(DirectX::XMFLOAT3(x, y, z));
				}

				//RenderItem World - Scale
				{
					in.getline(str, MAX_LINE);
					float x = 0, y = 0, z = 0;
					sscanf_s(str, "\t\tScale:%f %f %f\n", &x, &y, &z);

					attr.Scales.push_back(DirectX::XMFLOAT3(x, y, z));
				}

				//RenderItem Texture - TexturePosition
				{
					in.getline(str, MAX_LINE);
					float x = 0, y = 0, z = 0;
					sscanf_s(str, "\t\tTexturePosition:%f %f %f\n", &x, &y, &z);

					attr.TexturePositions.push_back(DirectX::XMFLOAT3(x, y, z));
				}

				//RenderItem Texture - TextureRotation
				{
					in.getline(str, MAX_LINE);
					float x = 0, y = 0, z = 0;
					sscanf_s(str, "\t\tTextureRotation:%f %f %f\n", &x, &y, &z);

					attr.TextureRotations.push_back(DirectX::XMFLOAT3(x, y, z));
				}

				//RenderItem Texture - TextureScale
				{
					in.getline(str, MAX_LINE);
					float x = 0, y = 0, z = 0;
					sscanf_s(str, "\t\tTextureScale:%f %f %f\n", &x, &y, &z);

					attr.TextureScales.push_back(DirectX::XMFLOAT3(x, y, z));
				}

				//RenderItem MaterialName
				in.getline(str, MAX_LINE);
				char matName[MAX_BUFFER] = { 0, };
				sscanf_s(str, "\t\tMaterial:%s\n", matName, MAX_BUFFER);
				attr.MaterialName.push_back(std::string(matName));

				in.getline(str, MAX_LINE);
			}

			mRenderItemAttrs.push_back(attr);
			attr.InstanceIndices.clear();
			attr.Positions.clear();
			attr.Rotations.clear();
			attr.Scales.clear();
			attr.TexturePositions.clear();
			attr.TextureRotations.clear();
			attr.TextureScales.clear();
			attr.MaterialName.clear();

			memset(geoName, 0x00, MAX_BUFFER);

		}
	}

	std::cout << "Success Reading RenderItemFile!" << std::endl;
	in.close();
}

void SceneReader::ReadTimetableFile() {
	//File 읽기
	std::ifstream in(mTimetableFileName.c_str());

	if (!in.is_open()) {
		cerr << "Can't Open File" << endl;
		return;
	}

	char str[MAX_LINE];

	while (in) {
		in.getline(str, MAX_LINE);
		std::cout << str << std::endl;
	}

	in.close();
}