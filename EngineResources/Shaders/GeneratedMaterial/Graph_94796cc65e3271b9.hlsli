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
float EvaluateCustomNode_29(float A, float B)
{
    return (saturate(pow(max(A, 0.0001f), max(B, 0.1f)) * 12.0f));
}
float EvaluateCustomNode_30(float A, float B)
{
    return ((1.0f - saturate((A - 0.12f) * 2.2f)) * B);
}
float EvaluateCustomNode_34(float A, float2 B)
{
    return (A * 225.0f + sin(A * 41.0f + B.x * 9.0f) * 2.6f + sin(B.x * 21.0f - B.y * 13.0f) * 1.6f);
}
float EvaluateCustomNode_32(float A, float B)
{
    return (clamp((A - 0.5f) * 2.0f, -1.0f, 1.0f) * B);
}
float EvaluateCustomNode_35(float A, float B)
{
    return (pow(saturate((-A - 0.08f) * 1.95f), 1.45f) * saturate(B));
}
float EvaluateCustomNode_37(float A, float B)
{
    return ((cos(A) * 0.65f + cos(A * 1.73f + 0.9f) * 0.605f) * B * 0.62f);
}
float3 EvaluateCustomNode_42(float3 A, float B)
{
    return (normalize(normalize(A) - normalize(float3(0.0f, 0.0f, 1.0f) - normalize(A) * normalize(A).z + float3(0.001f, 0.0f, 0.0f)) * B));
}
float EvaluateCustomNode_48(float3 A, float3 B)
{
    return (saturate(dot(normalize(A), normalize(B)) * 0.58f + 0.42f));
}
float EvaluateCustomNode_49(float3 A, float3 B)
{
    return (saturate(dot(normalize(A), normalize(B)) * 0.5f + 0.5f));
}
float EvaluateCustomNode_50(float A, float B)
{
    return (0.36f + 0.64f * A + 0.18f * B);
}
float EvaluateCustomNode_36(float A, float B)
{
    return ((sin(A) * 0.65f + sin(A * 1.73f + 0.9f) * 0.35f) * B);
}
float EvaluateCustomNode_38(float A, float B)
{
    return (clamp(A * 0.20f + B * 0.85f, -1.0f, 1.0f));
}
float EvaluateCustomNode_39(float2 A, float B)
{
    return (pow(saturate(sin(A.x * B * 14.0f) * sin(A.y * B * 14.0f) * 0.6f + sin((A.x * 1.37f - A.y * 0.83f) * B * 19.0f) * 0.4f), 3.0f));
}
float EvaluateCustomNode_40(float A, float B)
{
    return (1.0f + (A - 0.25f) * B * 0.14f);
}
float EvaluateCustomNode_56(float A, float B)
{
    return (clamp(1.0f - saturate(-A) * 0.16f + saturate(A) * 0.06f, 0.82f, 1.08f) * B);
}
float EvaluateCustomNode_31(float A, float B)
{
    return (saturate((A - 1.0f) * B));
}
float EvaluateCustomNode_65(float A, float B)
{
    return (A - B * 0.06f);
}
float EvaluateCustomNode_66(float A, float B)
{
    return (A + B * 0.14f);
}
float3 EvaluateCustomNode_51(float3 A, float3 B)
{
    return (normalize(normalize(A) + normalize(B)));
}
float EvaluateCustomNode_52(float3 A, float3 B)
{
    return (saturate(dot(normalize(A), normalize(B))));
}
float EvaluateCustomNode_72(float A, float B)
{
    return (A * (B + 0.08f));
}
float EvaluateCustomNode_71(float A, float B)
{
    return (A * B * 2.2f);
}
float EvaluateCustomNode_73(float A, float B)
{
    return (A * (1.0f + B * 3.5f));
}
float EvaluateCustomNode_74(float A, float B)
{
    return ((A + B * 0.55f));
}
float EvaluateCustomNode_75(float A, float B)
{
    return (A * clamp(0.85f + B * 0.35f, 0.55f, 1.35f));
}
float3 EvaluateCustomNode_76(float3 A, float B)
{
    return ((A * 0.45f + float3(0.50f, 0.48f, 0.52f)) * B);
}
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
    float4 GraphNode_2 = GraphParameterValues[0];
    float3 GraphNode_8 = GraphNode_2.rgb;
    float4 GraphNode_3 = GraphParameterValues[1];
    float3 GraphNode_9 = GraphNode_3.rgb;
    float4 GraphNode_24 = VertexColor;
    float GraphNode_25 = GraphNode_24.r;
    float4 GraphNode_7 = GraphParameterValues[5];
    float GraphNode_23 = GraphNode_7.g;
    float GraphNode_29 = EvaluateCustomNode_29(GraphNode_25, GraphNode_23);
    float GraphNode_30 = EvaluateCustomNode_30(GraphNode_25, GraphNode_29);
    float3 GraphNode_55 = lerp(GraphNode_8, GraphNode_9, GraphNode_30);
    float3 GraphNode_41 = normalize(VertexNormalWS);
    float GraphNode_26 = GraphNode_24.g;
    float2 GraphNode_33 = UV;
    float GraphNode_34 = EvaluateCustomNode_34(GraphNode_26, GraphNode_33);
    float GraphNode_27 = GraphNode_24.b;
    float4 GraphNode_6 = GraphParameterValues[4];
    float GraphNode_21 = GraphNode_6.a;
    float GraphNode_32 = EvaluateCustomNode_32(GraphNode_27, GraphNode_21);
    float GraphNode_35 = EvaluateCustomNode_35(GraphNode_32, GraphNode_21);
    float GraphNode_37 = EvaluateCustomNode_37(GraphNode_34, GraphNode_35);
    float3 GraphNode_42 = EvaluateCustomNode_42(GraphNode_41, GraphNode_37);
    float3 GraphNode_46 = float3(0.450000f, -0.650000f, 0.610000f);
    float GraphNode_48 = EvaluateCustomNode_48(GraphNode_42, GraphNode_46);
    float3 GraphNode_47 = float3(-0.660000f, 0.440000f, 0.610000f);
    float GraphNode_49 = EvaluateCustomNode_49(GraphNode_42, GraphNode_47);
    float GraphNode_50 = EvaluateCustomNode_50(GraphNode_48, GraphNode_49);
    float GraphNode_36 = EvaluateCustomNode_36(GraphNode_34, GraphNode_35);
    float GraphNode_38 = EvaluateCustomNode_38(GraphNode_32, GraphNode_36);
    float4 GraphNode_5 = GraphParameterValues[3];
    float GraphNode_16 = GraphNode_5.b;
    float GraphNode_39 = EvaluateCustomNode_39(GraphNode_33, GraphNode_16);
    float GraphNode_17 = GraphNode_5.a;
    float GraphNode_40 = EvaluateCustomNode_40(GraphNode_39, GraphNode_17);
    float GraphNode_56 = EvaluateCustomNode_56(GraphNode_38, GraphNode_40);
    float GraphNode_57 = GraphNode_50 * GraphNode_56;
    float3 GraphNode_58 = GraphNode_55 * float3(GraphNode_57, GraphNode_57, GraphNode_57);
    float4 GraphNode_4 = GraphParameterValues[2];
    float GraphNode_10 = GraphNode_4.r;
    float GraphNode_12 = GraphNode_4.b;
    float GraphNode_44 = 0.000000f;
    float GraphNode_45 = GraphNode_44 + (1.0f - GraphNode_44) * pow(1.0f - saturate(dot(normalize(GraphNode_42), normalize(CameraVectorWS))), GraphNode_12);
    float GraphNode_11 = GraphNode_4.g;
    float GraphNode_59 = GraphNode_45 * GraphNode_11;
    float GraphNode_60 = GraphNode_10 + GraphNode_59;
    float GraphNode_18 = GraphNode_6.r;
    float GraphNode_61 = GraphNode_30 * GraphNode_18;
    float GraphNode_62 = GraphNode_60 + GraphNode_61;
    float GraphNode_28 = GraphNode_24.a;
    float GraphNode_14 = GraphNode_5.r;
    float GraphNode_31 = EvaluateCustomNode_31(GraphNode_28, GraphNode_14);
    float GraphNode_15 = GraphNode_5.g;
    float GraphNode_63 = GraphNode_31 * GraphNode_15;
    float GraphNode_64 = GraphNode_62 - GraphNode_63;
    float GraphNode_65 = EvaluateCustomNode_65(GraphNode_64, GraphNode_38);
    float GraphNode_66 = EvaluateCustomNode_66(GraphNode_65, GraphNode_35);
    float GraphNode_13 = GraphNode_4.a;
    float GraphNode_67 = 0.960000f;
    float GraphNode_68 = clamp(GraphNode_66, GraphNode_13, GraphNode_67);
    float GraphNode_69 = GraphNode_68 * GraphNode_29;
    float4 GraphNode_70 = float4(GraphNode_58, GraphNode_69);
    float3 GraphNode_43 = CameraVectorWS;
    float3 GraphNode_51 = EvaluateCustomNode_51(GraphNode_46, GraphNode_43);
    float GraphNode_52 = EvaluateCustomNode_52(GraphNode_42, GraphNode_51);
    float GraphNode_20 = GraphNode_6.b;
    float GraphNode_53 = pow(GraphNode_52, GraphNode_20);
    float GraphNode_19 = GraphNode_6.g;
    float GraphNode_54 = GraphNode_53 * GraphNode_19;
    float GraphNode_22 = GraphNode_7.r;
    float GraphNode_72 = EvaluateCustomNode_72(GraphNode_45, GraphNode_22);
    float GraphNode_71 = EvaluateCustomNode_71(GraphNode_39, GraphNode_17);
    float GraphNode_73 = EvaluateCustomNode_73(GraphNode_72, GraphNode_71);
    float GraphNode_74 = EvaluateCustomNode_74(GraphNode_54, GraphNode_73);
    float GraphNode_75 = EvaluateCustomNode_75(GraphNode_74, GraphNode_38);
    float3 GraphNode_76 = EvaluateCustomNode_76(GraphNode_8, GraphNode_75);
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
    Result.BaseColor = GraphNode_70;
#define MATERIAL_GRAPH_HAS_EMISSIVE_COLOR 1
    Result.EmissiveColor = GraphNode_76;
    return Result;
}
FMaterialGraphOutput EvaluateMaterialGraph(float2 UV)
{
    return EvaluateMaterialGraph(UV, float3(0, 0, 0), float3(0, 0, 1), float3(0, 0, 1), float4(1, 1, 1, 1), 0.0f);
}
