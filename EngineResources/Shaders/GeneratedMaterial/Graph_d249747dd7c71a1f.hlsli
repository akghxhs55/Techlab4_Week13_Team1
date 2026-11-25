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
    float4 GraphNode_25 = GraphParameterValues[6];
    float3 GraphNode_27 = GraphNode_25.rgb;
    float4 GraphNode_24 = GraphParameterValues[5];
    float3 GraphNode_26 = GraphNode_24.rgb;
    float4 GraphNode_2 = VertexColor;
    float GraphNode_3 = GraphNode_2.r;
    float GraphNode_5 = GraphParameterValues[0].x;
    float GraphNode_6 = pow(GraphNode_3, GraphNode_5);
    float3 GraphNode_28 = lerp(GraphNode_27, GraphNode_26, GraphNode_6);
    float GraphNode_8 = 1.000000f;
    float2 GraphNode_17 = UV;
    float GraphNode_18 = GraphParameterValues[3].x;
    float2 GraphNode_19 = GraphNode_17 * float2(GraphNode_18, GraphNode_18);
    float GraphNode_20 = MaterialGraphNoise2D(GraphNode_19);
    float GraphNode_21 = GraphParameterValues[4].x;
    float GraphNode_22 = GraphNode_20 * GraphNode_21;
    float GraphNode_23 = GraphNode_8 + GraphNode_22;
    float3 GraphNode_29 = GraphNode_28 * float3(GraphNode_23, GraphNode_23, GraphNode_23);
    float GraphNode_30 = GraphParameterValues[7].x;
    float3 GraphNode_13 = normalize(VertexNormalWS);
    float GraphNode_14 = GraphParameterValues[2].x;
    float GraphNode_15 = 0.000000f;
    float GraphNode_16 = GraphNode_15 + (1.0f - GraphNode_15) * pow(1.0f - saturate(dot(normalize(GraphNode_13), normalize(CameraVectorWS))), GraphNode_14);
    float GraphNode_31 = GraphParameterValues[8].x;
    float GraphNode_32 = GraphNode_16 * GraphNode_31;
    float GraphNode_33 = GraphNode_30 + GraphNode_32;
    float GraphNode_7 = 1.0f - GraphNode_6;
    float GraphNode_34 = GraphParameterValues[9].x;
    float GraphNode_35 = GraphNode_7 * GraphNode_34;
    float GraphNode_36 = GraphNode_33 + GraphNode_35;
    float GraphNode_4 = GraphNode_2.a;
    float GraphNode_9 = GraphNode_4 - GraphNode_8;
    float GraphNode_10 = GraphParameterValues[1].x;
    float GraphNode_11 = GraphNode_9 * GraphNode_10;
    float GraphNode_12 = saturate(GraphNode_11);
    float GraphNode_37 = GraphParameterValues[10].x;
    float GraphNode_38 = GraphNode_12 * GraphNode_37;
    float GraphNode_39 = GraphNode_36 - GraphNode_38;
    float GraphNode_40 = GraphParameterValues[11].x;
    float GraphNode_41 = clamp(GraphNode_39, GraphNode_40, GraphNode_8);
    float GraphNode_42 = GraphNode_41 * GraphNode_6;
    float4 GraphNode_43 = float4(GraphNode_29, GraphNode_42);
    float GraphNode_44 = GraphParameterValues[12].x;
    float GraphNode_45 = GraphNode_16 * GraphNode_44;
    float3 GraphNode_46 = GraphNode_26 * float3(GraphNode_45, GraphNode_45, GraphNode_45);
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
    Result.BaseColor = GraphNode_43;
#define MATERIAL_GRAPH_HAS_EMISSIVE_COLOR 1
    Result.EmissiveColor = GraphNode_46;
    return Result;
}
FMaterialGraphOutput EvaluateMaterialGraph(float2 UV)
{
    return EvaluateMaterialGraph(UV, float3(0, 0, 0), float3(0, 0, 1), float3(0, 0, 1), float4(1, 1, 1, 1), 0.0f);
}
