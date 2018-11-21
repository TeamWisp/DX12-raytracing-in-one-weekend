struct VSOutput
{
	float2 pTexture    : P_TEXTURECOORD;
};

Texture2D t1 : register(t0);
SamplerState s1 : register(s0);

float4 main(VSOutput IN) : SV_Target
{
	return t1.Sample(s1, IN.pTexture);
}