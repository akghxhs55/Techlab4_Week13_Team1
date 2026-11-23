#include "DepthOfFieldLayeredCommon.hlsli"

Texture2D<float4> SceneColor : register(t0);
Texture2D<float4> NearColor : register(t1);
Texture2D<float4> FarColor : register(t2);

DOFVSOutput mainVS(uint VertexID : SV_VertexID)
{
    return DOFMainVS(VertexID);
}

float4 mainPS(DOFVSOutput Input) : SV_Target
{
    int3 Pixel = int3(int2(Input.Position.xy), 0);
    float4 Sharp = SceneColor.Load(Pixel);
    float4 Near = NearColor.Load(Pixel);
    float4 Far = FarColor.Load(Pixel);
    float3 Background = lerp(Sharp.rgb, Far.rgb, Far.a);
    return float4(Near.rgb + Background * (1.0f - Near.a), Sharp.a);
}
