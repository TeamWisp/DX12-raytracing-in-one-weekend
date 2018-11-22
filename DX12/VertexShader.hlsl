cbuffer ModelViewProjectionCB: register(b0)
{
	matrix MVP;
}

struct VertexPosColor
{
	float3 Position : POSITION;
	float2 vTexture    : TEXTURECOORD;
};

struct VertexShaderOutput
{
	float2 pTexture    : P_TEXTURECOORD;
	float4 Position : SV_Position;
};

VertexShaderOutput main(VertexPosColor IN)
{
	VertexShaderOutput OUT;

	OUT.Position = float4(IN.Position, 1.0f);
	OUT.pTexture = IN.vTexture;

	return OUT;
}