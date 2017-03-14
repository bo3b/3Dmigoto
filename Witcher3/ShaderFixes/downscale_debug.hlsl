Texture2D<float4> StereoParams : register(t125);
Texture1D<float4> IniParams : register(t120);
Texture2D<float4> t100 : register(t100);

void main(float4 pos : SV_Position0, out float4 result : SV_Target0)
{
	float x = pos.x;
	float y = pos.y;

	result = t100.Load(float3(x, y, 0)).x;
	result /= 1024.0;
	result.w = 1;
}
