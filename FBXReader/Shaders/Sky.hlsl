#include "Common.hlsl"

struct VertexIn {
	float3 PosL	: POSITION;
	float3 NormalL	: NORMAL;
	float2 TexC	: TEXCOORD;
};

struct VertexOut {
	float4 PosH	: SV_POSITION;
	float3 PosL	: POSITION;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID) {
	VertexOut vout;

	//로컬 정점 위치
	vout.PosL = vin.PosL;

	float4 posW = mul(float4(vin.PosL, 1.0f), gInstanceData[instanceID].World);

	//하늘 구의 중심을 항상 카메라 위치에 고정.
	posW.xyz += gEyePosW;

	//z/w = 1 이 되도록(하늘 구가 항상 먼 평면에 있도록) z = w로 설정
	vout.PosH = mul(posW, gViewProj).xyww;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target{
	return gCubeMap.Sample(gsamLinearWrap, pin.PosL);
}