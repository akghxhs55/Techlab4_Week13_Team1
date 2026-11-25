struct FMaterialGraphOutput
{
    float4 BaseColor;
    float3 SpecularColor;
    float Shininess;
    float3 EmissiveColor;
    float EmissiveFactor;
    float Metallic;
    float Roughness;
    float OpacityMask;
    float3 Normal;
    float AmbientOcclusion;
};
cbuffer MaterialGraphParameters : register(b4)
{
    float4 GraphParameterValues[16];
};
float MaterialGraphHash21(float2 P)
{
    P = frac(P * float2(123.34f, 456.21f));
    P += dot(P, P + 45.32f);
    return frac(P.x * P.y);
}
float MaterialGraphValueNoise(float2 P)
{
    float2 I = floor(P);
    float2 F = frac(P);
    F = F * F * (3.0f - 2.0f * F);
    float A = MaterialGraphHash21(I);
    float B = MaterialGraphHash21(I + float2(1.0f, 0.0f));
    float C = MaterialGraphHash21(I + float2(0.0f, 1.0f));
    float D = MaterialGraphHash21(I + float2(1.0f, 1.0f));
    return lerp(lerp(A, B, F.x), lerp(C, D, F.x), F.y);
}
float MaterialGraphNoise2D(float2 P)
{
    return MaterialGraphValueNoise(P) * 0.65f + MaterialGraphValueNoise(P * 2.03f) * 0.35f;
}
FMaterialGraphOutput EvaluateMaterialGraph(float2 UV, float3 WorldPosition, float3 CameraVectorWS, float3 VertexNormalWS, float4 VertexColor, float Time)
{
    FMaterialGraphOutput Result = (FMaterialGraphOutput)0;
    float3 GraphNode_14 = WorldPosition;
    float GraphNode_15 = GraphNode_14.b;
    float GraphNode_16 = GraphParameterValues[2].x;
    float GraphNode_18 = GraphNode_15 - GraphNode_16;
    float GraphNode_17 = GraphParameterValues[3].x;
    float GraphNode_19 = GraphNode_17 - GraphNode_16;
    float GraphNode_20 = GraphNode_18 / GraphNode_19;
    float GraphNode_21 = saturate(GraphNode_20);
    float GraphNode_22 = 1.0f - GraphNode_21;
    float GraphNode_23 = 0.800000f;
    float GraphNode_24 = GraphNode_22 * GraphNode_23;
    float2 GraphNode_2 = UV;
    float2 GraphNode_3 = float2(28.000000f, 28.000000f);
    float2 GraphNode_4 = GraphNode_2 * GraphNode_3;
    float GraphNode_5 = MaterialGraphNoise2D(GraphNode_4);
    float GraphNode_25 = 0.250000f;
    float GraphNode_26 = GraphNode_5 * GraphNode_25;
    float GraphNode_27 = GraphNode_24 + GraphNode_26;
    float GraphNode_6 = GraphParameterValues[0].x;
    float GraphNode_7 = GraphNode_27 - GraphNode_6;
    float4 GraphNode_11 = float4(0.000000f, 0.000000f, 0.000000f, 1.000000f);
    float4 GraphNode_12 = GraphParameterValues[1];
    float GraphNode_8 = 0.060000f;
    float GraphNode_9 = step(GraphNode_8, GraphNode_7);
    float GraphNode_10 = 1.0f - GraphNode_9;
    float4 GraphNode_13 = lerp(GraphNode_11, GraphNode_12, GraphNode_10);
    Result.BaseColor = float4(1, 1, 1, 1);
    Result.SpecularColor = float3(1, 1, 1);
    Result.Shininess = 32.0f;
    Result.EmissiveColor = float3(0, 0, 0);
    Result.EmissiveFactor = 1.0f;
    Result.Metallic = 0.0f;
    Result.Roughness = 0.8f;
    Result.OpacityMask = 1.0f;
    Result.Normal = float3(0, 0, 1);
    Result.AmbientOcclusion = 1.0f;
#define MATERIAL_GRAPH_HAS_EMISSIVE_COLOR 1
    Result.EmissiveColor = GraphNode_13.rgb;
#define MATERIAL_GRAPH_HAS_OPACITY_MASK 1
    Result.OpacityMask = GraphNode_7;
    return Result;
}
FMaterialGraphOutput EvaluateMaterialGraph(float2 UV)
{
    return EvaluateMaterialGraph(UV, float3(0, 0, 0), float3(0, 0, 1), float3(0, 0, 1), float4(1, 1, 1, 1), 0.0f);
}
