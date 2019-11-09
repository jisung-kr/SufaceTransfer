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

	//���� ���� ��ġ
	vout.PosL = vin.PosL;

	float4 posW = mul(float4(vin.PosL, 1.0f), gInstanceData[instanceID].World);

	//�ϴ� ���� �߽��� �׻� ī�޶� ��ġ�� ����.
	posW.xyz += gEyePosW;

	//z/w = 1 �� �ǵ���(�ϴ� ���� �׻� �� ��鿡 �ֵ���) z = w�� ����
	vout.PosH = mul(posW, gViewProj).xyww;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target{
	return gCubeMap.Sample(gsamLinearWrap, pin.PosL);
}