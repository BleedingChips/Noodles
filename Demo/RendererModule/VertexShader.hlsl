struct Input
{
	float2 VertexPosition : VERPOSITION;
	float2 Position : POSITION;
	float Range : RANGE;
	float Property : PROPERTY;
};

struct Output
{
	float4 Position : SV_POSITION;
	float Property : PROPERTY;
};

Output main(in Input input)
{
	Output output;
	output.Position = float4(input.VertexPosition * input.Range + input.Position, 0.5, 1.0);
	output.Position.x = output.Position.x * (768.0 / 1024.0);
	output.Property = input.Property;
	return output;
}