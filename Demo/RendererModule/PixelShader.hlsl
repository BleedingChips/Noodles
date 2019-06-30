struct VertexInput
{
	float4 Position : SV_POSITION;
	float Property : PROPERTY;
};



float4 main(VertexInput input) : SV_TARGET
{
	if (input.Property < 0.25)
		return float4(1.0, 0.0, 0.0, 1.0);
	else if(input.Property < 0.75)
		return float4(0.0, 0.0, 1.0, 1.0);
	else
		return float4(0.5, 0.5, 0.5, 1.0);
}