cbuffer CameraConstants : register(b2)
{
	float OrthoMinX;
	float OrthoMinY;
	float OrthoMinZ;

	float OrthoMaxX;
	float OrthoMaxY;
	float OrthoMaxZ;

	float pad0;
	float pad1;
}

float Interpolate(float start, float end, float fractionTowardsEnd)
{
	return (start + (fractionTowardsEnd) * (end - start));
}

float GetFractionWithinRange(float value, float rangeStart, float rangeEnd)
{
	if (rangeStart == rangeEnd)
	{
		return .50f;
	}

	else
	{
		return (value - rangeStart) / (rangeEnd - rangeStart);
	}
}

float RangeMap(float inValue, float inStart, float inEnd, float outStart, float outEnd)
{
	float fractionVal = GetFractionWithinRange(inValue, inStart, inEnd);

	return Interpolate(outStart, outEnd, fractionVal);
}

struct vs_input_t
{
	float3 localPosition : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
};

struct v2p_t
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
};

v2p_t VertexMain(vs_input_t input)
{
	float4 clipPosition;
	clipPosition.x = RangeMap(input.localPosition.x, OrthoMinX, OrthoMaxX, -1, 1);
	clipPosition.y = RangeMap(input.localPosition.y, OrthoMinY, OrthoMaxY, -1, 1);
	clipPosition.z = RangeMap(input.localPosition.z, OrthoMinZ, OrthoMaxZ, 0, 1);
	clipPosition.w = 1;
	v2p_t v2p;
	v2p.position = clipPosition;
	v2p.color = input.color;
	v2p.uv = input.uv;
	return v2p;
}

float4 PixelMain(v2p_t input) : SV_Target0
{
	float4 color = diffuseTexture.Sample(diffuseSampler, input.uv) * input.Diffuse;
	return float4(input.color);
}

SamplerState diffuseSampler : register(s0);

Texture2D diffuseTexture : register(t0);
