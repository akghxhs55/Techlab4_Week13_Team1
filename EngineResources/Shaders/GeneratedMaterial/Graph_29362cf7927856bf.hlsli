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
Texture2D GraphTexture0 : register(t2);
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
    float2 GraphNode_3 = UV;
    float4 GraphNode_4 = GraphTexture0.Sample(ColorSampler, GraphNode_3);
    GraphNode_4.rgb = pow(max(GraphNode_4.rgb, 0.0f), 2.2f);
    float4 GraphNode_2 = float4(1.000000f, 1.000000f, 1.000000f, 1.000000f);
    float4 GraphNode_5 = GraphNode_4 * GraphNode_2;
    float3 GraphNode_6 = float3(1.000000f, 1.000000f, 1.000000f);
    float GraphNode_7 = 32.000000f;
    float3 GraphNode_8 = float3(0.000000f, 0.000000f, 0.000000f);
    float GraphNode_9 = 1.000000f;
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
#define MATERIAL_GRAPH_HAS_BASE_COLOR 1
    Result.BaseColor = GraphNode_5;
#define MATERIAL_GRAPH_HAS_SPECULAR_COLOR 1
    Result.SpecularColor = GraphNode_6;
#define MATERIAL_GRAPH_HAS_SHININESS 1
    Result.Shininess = GraphNode_7;
#define MATERIAL_GRAPH_HAS_EMISSIVE_COLOR 1
    Result.EmissiveColor = GraphNode_8;
#define MATERIAL_GRAPH_HAS_EMISSIVE_FACTOR 1
    Result.EmissiveFactor = GraphNode_9;
    return Result;
}
FMaterialGraphOutput EvaluateMaterialGraph(float2 UV)
{
    return EvaluateMaterialGraph(UV, float3(0, 0, 0), float3(0, 0, 1), float3(0, 0, 1), float4(1, 1, 1, 1), 0.0f);
}
