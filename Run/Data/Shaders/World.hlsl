struct vs_input_t
{
	float3 localPosition : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
};

struct v2p_t
{
	float4 position : SV_Position;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
	float4 worldPosition : WorldPos;
};

cbuffer CameraConstants : register(b2)
{
	float4x4 projectionMatrix;
	float4x4 viewMatrix;
}

cbuffer ModelConstants : register(b3)
{
	float4x4 modelMatrix;
	float4 modelColor;
}

cbuffer GameConstants : register(b5)
{
	float4 cameraWorldPos;
	float4 indoorLightColor;
	float4 outdoorLightColor;
	float4 skyColor;
	float fogStartDistance;
	float fogEndDistance;
	float fogMaxAlpha;
	float worldTime;
}

Texture2D diffuseTexture : register(t0);
SamplerState diffuseSampler : register(s0);

float3 DiminishingAdd(float3 a, float3 b)
{
	return 1.f - ((1 - a) * (1 - b));
}


v2p_t VertexMain(vs_input_t input)
{
	float4 localPosition = float4(input.localPosition.x, input.localPosition.y, input.localPosition.z, 1.f);
	float4 modelSpacePos = mul(modelMatrix, localPosition);

	modelSpacePos = float4(modelSpacePos.xyz, 1.0f); 

	float4 viewSpacePos = mul(viewMatrix, modelSpacePos);
	float4 ndcPos = mul(projectionMatrix, viewSpacePos);
	v2p_t v2p;
	v2p.position = ndcPos;
	v2p.worldPosition = modelSpacePos;
	v2p.color = input.color;
	v2p.uv = input.uv;
	return v2p;
}

float4 PixelMain(v2p_t input) : SV_Target0
{
	float4 output = float4(diffuseTexture.Sample(diffuseSampler, input.uv));
	if (output.a < 0.01)
	{
		discard;
	}

	float outdoorLightInfluence = input.color.r;
	float indoorLightInfluence = input.color.g;
	float3 outdoorLight = outdoorLightInfluence * outdoorLightColor.rgb;
	float3 indoorLight = indoorLightInfluence * indoorLightColor.rgb;
	float3 diffuseLight = DiminishingAdd(outdoorLight, indoorLight);
	float3 diffuseRGB = output.rgb * diffuseLight;

	float distanceFromCamera = length(input.worldPosition.xyz - cameraWorldPos.xyz);
	float fogFraction = fogMaxAlpha * saturate((distanceFromCamera - fogStartDistance) / (fogEndDistance - fogStartDistance));
	float3 outputRGB = lerp(diffuseRGB, skyColor.xyz, fogFraction * fogMaxAlpha);
	float outputAlpha = saturate(output.a + fogFraction );
	float4 outputRGBA = float4(outputRGB, outputAlpha);

	return outputRGBA;
}