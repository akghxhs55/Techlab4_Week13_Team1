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
float EvaluateCustomNode_34(float A, float B)
{
    return (saturate(pow(max(A, 0.0001f), max(B, 0.1f)) * 12.0f));
}
float EvaluateCustomNode_35(float A, float B)
{
    return ((1.0f - saturate((A - 0.12f) * 2.2f)) * B);
}
float EvaluateCustomNode_39(float A, float2 B)
{
    return (A * 225.0f + sin(A * 41.0f + B.x * 9.0f) * 2.6f + sin(B.x * 21.0f - B.y * 13.0f) * 1.6f);
}
float EvaluateCustomNode_37(float A, float B)
{
    return (clamp((A - 0.5f) * 2.0f, -1.0f, 1.0f) * B);
}
float EvaluateCustomNode_40(float A, float B)
{
    return (pow(saturate((-A - 0.08f) * 1.95f), 1.45f) * saturate(B));
}
float EvaluateCustomNode_42(float A, float B)
{
    return ((cos(A) * 0.65f + cos(A * 1.73f + 0.9f) * 0.605f) * B * 0.62f);
}
float3 EvaluateCustomNode_47(float3 A, float B)
{
    return (normalize(normalize(A) - normalize(float3(0.0f, 0.0f, 1.0f) - normalize(A) * normalize(A).z + float3(0.001f, 0.0f, 0.0f)) * B));
}
float EvaluateCustomNode_53(float3 A, float3 B)
{
    return (saturate(dot(normalize(A), normalize(B)) * 0.58f + 0.42f));
}
float EvaluateCustomNode_54(float3 A, float3 B)
{
    return (saturate(dot(normalize(A), normalize(B)) * 0.5f + 0.5f));
}
float EvaluateCustomNode_55(float A, float B)
{
    return (0.36f + 0.64f * A + 0.18f * B);
}
float EvaluateCustomNode_41(float A, float B)
{
    return ((sin(A) * 0.65f + sin(A * 1.73f + 0.9f) * 0.35f) * B);
}
float EvaluateCustomNode_43(float A, float B)
{
    return (clamp(A * 0.20f + B * 0.85f, -1.0f, 1.0f));
}
float EvaluateCustomNode_44(float2 A, float B)
{
    return (pow(saturate(sin(A.x * B * 14.0f) * sin(A.y * B * 14.0f) * 0.6f + sin((A.x * 1.37f - A.y * 0.83f) * B * 19.0f) * 0.4f), 3.0f));
}
float EvaluateCustomNode_45(float A, float B)
{
    return (1.0f + (A - 0.25f) * B * 0.14f);
}
float EvaluateCustomNode_61(float A, float B)
{
    return (clamp(1.0f - saturate(-A) * 0.16f + saturate(A) * 0.06f, 0.82f, 1.08f) * B);
}
float EvaluateCustomNode_88(float A, float B)
{
    return (saturate(1.0f - abs(A - 0.485f) * 24.0f) * saturate(A * 6.0f) * B);
}
float EvaluateCustomNode_96(float A, float B)
{
    return (clamp(1.0f - A * 0.44f - B * 0.18f, 0.44f, 1.0f));
}
float EvaluateCustomNode_89(float A, float B)
{
    return (saturate(1.0f - saturate((A - 1.03f) * 3.8f)) * B);
}
float EvaluateCustomNode_90(float A, float B)
{
    return (saturate(A * B * 0.22f));
}
float3 EvaluateCustomNode_98(float3 A, float B)
{
    return (A * 0.30f + float3(0.06f, 0.035f, 0.025f) * saturate(B + 1.0f));
}
float EvaluateCustomNode_86(float A, float2 B)
{
    return (smoothstep(0.495f, 0.528f, A + 0.014f * sin(B.y * 520.0f + sin(B.x * 110.0f) * 2.0f)));
}
float EvaluateCustomNode_83(float2 A, float B)
{
    return ((pow(saturate(0.5f + 0.5f * sin(A.y * 540.0f + sin(A.x * 75.0f + A.y * 22.0f) * 1.9f)), 24.0f) + 0.75f * pow(saturate(0.5f + 0.5f * sin(A.y * 860.0f - sin(A.x * 58.0f) * 1.4f)), 28.0f)) * B);
}
float EvaluateCustomNode_84(float A, float B)
{
    return (saturate(A * smoothstep(0.18f, 0.85f, B)));
}
float EvaluateCustomNode_87(float A, float B)
{
    return (saturate(1.0f - A * (1.0f - B * 0.88f)));
}
float EvaluateCustomNode_99(float A, float B)
{
    return (saturate(A * (1.0f - B)));
}
float EvaluateCustomNode_36(float A, float B)
{
    return (saturate((A - 1.0f) * B));
}
float EvaluateCustomNode_70(float A, float B)
{
    return (A - B * 0.06f);
}
float EvaluateCustomNode_71(float A, float B)
{
    return (A + B * 0.14f);
}
float EvaluateCustomNode_85(float2 A, float B)
{
    return ((0.16f + 0.84f * pow(saturate(0.5f + 0.5f * sin(A.y * 920.0f + sin(A.x * 64.0f) * 0.7f)), 3.2f)) * B);
}
float EvaluateCustomNode_91(float A, float B)
{
    return (clamp(1.0f - A * 0.78f + B * 0.40f, 0.14f, 1.0f));
}
float EvaluateCustomNode_93(float A, float B)
{
    return (saturate(A + B * 0.14f));
}
float EvaluateCustomNode_95(float A, float B)
{
    return (max(A, B));
}
float3 EvaluateCustomNode_56(float3 A, float3 B)
{
    return (normalize(normalize(A) + normalize(B)));
}
float EvaluateCustomNode_57(float3 A, float3 B)
{
    return (saturate(dot(normalize(A), normalize(B))));
}
float EvaluateCustomNode_103(float A, float B)
{
    return (A * (B + 0.08f));
}
float EvaluateCustomNode_102(float A, float B)
{
    return (A * B * 2.2f);
}
float EvaluateCustomNode_104(float A, float B)
{
    return (A * (1.0f + B * 3.5f));
}
float EvaluateCustomNode_105(float A, float B)
{
    return ((A + B * 0.55f));
}
float EvaluateCustomNode_106(float A, float B)
{
    return (A * clamp(0.85f + B * 0.35f, 0.55f, 1.35f));
}
float EvaluateCustomNode_107(float A, float B)
{
    return (A * B);
}
float3 EvaluateCustomNode_108(float3 A, float B)
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
    float3 GraphNode_10 = GraphNode_2.rgb;
    float4 GraphNode_3 = GraphParameterValues[1];
    float3 GraphNode_11 = GraphNode_3.rgb;
    float4 GraphNode_29 = VertexColor;
    float GraphNode_30 = GraphNode_29.r;
    float4 GraphNode_7 = GraphParameterValues[5];
    float GraphNode_25 = GraphNode_7.g;
    float GraphNode_34 = EvaluateCustomNode_34(GraphNode_30, GraphNode_25);
    float GraphNode_35 = EvaluateCustomNode_35(GraphNode_30, GraphNode_34);
    float3 GraphNode_60 = lerp(GraphNode_10, GraphNode_11, GraphNode_35);
    float3 GraphNode_46 = normalize(VertexNormalWS);
    float GraphNode_31 = GraphNode_29.g;
    float2 GraphNode_38 = UV;
    float GraphNode_39 = EvaluateCustomNode_39(GraphNode_31, GraphNode_38);
    float GraphNode_32 = GraphNode_29.b;
    float4 GraphNode_6 = GraphParameterValues[4];
    float GraphNode_23 = GraphNode_6.a;
    float GraphNode_37 = EvaluateCustomNode_37(GraphNode_32, GraphNode_23);
    float GraphNode_40 = EvaluateCustomNode_40(GraphNode_37, GraphNode_23);
    float GraphNode_42 = EvaluateCustomNode_42(GraphNode_39, GraphNode_40);
    float3 GraphNode_47 = EvaluateCustomNode_47(GraphNode_46, GraphNode_42);
    float3 GraphNode_51 = float3(0.450000f, -0.650000f, 0.610000f);
    float GraphNode_53 = EvaluateCustomNode_53(GraphNode_47, GraphNode_51);
    float3 GraphNode_52 = float3(-0.660000f, 0.440000f, 0.610000f);
    float GraphNode_54 = EvaluateCustomNode_54(GraphNode_47, GraphNode_52);
    float GraphNode_55 = EvaluateCustomNode_55(GraphNode_53, GraphNode_54);
    float GraphNode_41 = EvaluateCustomNode_41(GraphNode_39, GraphNode_40);
    float GraphNode_43 = EvaluateCustomNode_43(GraphNode_37, GraphNode_41);
    float4 GraphNode_5 = GraphParameterValues[3];
    float GraphNode_18 = GraphNode_5.b;
    float GraphNode_44 = EvaluateCustomNode_44(GraphNode_38, GraphNode_18);
    float GraphNode_19 = GraphNode_5.a;
    float GraphNode_45 = EvaluateCustomNode_45(GraphNode_44, GraphNode_19);
    float GraphNode_61 = EvaluateCustomNode_61(GraphNode_43, GraphNode_45);
    float GraphNode_62 = GraphNode_55 * GraphNode_61;
    float3 GraphNode_63 = GraphNode_60 * float3(GraphNode_62, GraphNode_62, GraphNode_62);
    float4 GraphNode_75 = GraphTexture0.Sample(ColorSampler, GraphNode_38);
    float GraphNode_76 = GraphNode_75.r;
    float GraphNode_9 = GraphParameterValues[7].x;
    float GraphNode_80 = GraphNode_76 * GraphNode_9;
    float4 GraphNode_8 = GraphParameterValues[6];
    float GraphNode_26 = GraphNode_8.r;
    float GraphNode_88 = EvaluateCustomNode_88(GraphNode_80, GraphNode_26);
    float GraphNode_77 = GraphNode_75.g;
    float GraphNode_81 = GraphNode_77 * GraphNode_9;
    float GraphNode_96 = EvaluateCustomNode_96(GraphNode_88, GraphNode_81);
    float3 GraphNode_97 = GraphNode_63 * float3(GraphNode_96, GraphNode_96, GraphNode_96);
    float GraphNode_78 = GraphNode_75.b;
    float GraphNode_82 = GraphNode_78 * GraphNode_9;
    float GraphNode_33 = GraphNode_29.a;
    float GraphNode_27 = GraphNode_8.g;
    float GraphNode_89 = EvaluateCustomNode_89(GraphNode_33, GraphNode_27);
    float GraphNode_90 = EvaluateCustomNode_90(GraphNode_82, GraphNode_89);
    float3 GraphNode_98 = EvaluateCustomNode_98(GraphNode_10, GraphNode_90);
    float GraphNode_86 = EvaluateCustomNode_86(GraphNode_80, GraphNode_38);
    float GraphNode_28 = GraphNode_8.b;
    float GraphNode_83 = EvaluateCustomNode_83(GraphNode_38, GraphNode_28);
    float GraphNode_79 = GraphNode_75.a;
    float GraphNode_84 = EvaluateCustomNode_84(GraphNode_83, GraphNode_79);
    float GraphNode_87 = EvaluateCustomNode_87(GraphNode_86, GraphNode_84);
    float GraphNode_99 = EvaluateCustomNode_99(GraphNode_86, GraphNode_87);
    float3 GraphNode_100 = lerp(GraphNode_97, GraphNode_98, GraphNode_99);
    float4 GraphNode_4 = GraphParameterValues[2];
    float GraphNode_12 = GraphNode_4.r;
    float GraphNode_14 = GraphNode_4.b;
    float GraphNode_49 = 0.000000f;
    float GraphNode_50 = GraphNode_49 + (1.0f - GraphNode_49) * pow(1.0f - saturate(dot(normalize(GraphNode_47), normalize(CameraVectorWS))), GraphNode_14);
    float GraphNode_13 = GraphNode_4.g;
    float GraphNode_64 = GraphNode_50 * GraphNode_13;
    float GraphNode_65 = GraphNode_12 + GraphNode_64;
    float GraphNode_20 = GraphNode_6.r;
    float GraphNode_66 = GraphNode_35 * GraphNode_20;
    float GraphNode_67 = GraphNode_65 + GraphNode_66;
    float GraphNode_16 = GraphNode_5.r;
    float GraphNode_36 = EvaluateCustomNode_36(GraphNode_33, GraphNode_16);
    float GraphNode_17 = GraphNode_5.g;
    float GraphNode_68 = GraphNode_36 * GraphNode_17;
    float GraphNode_69 = GraphNode_67 - GraphNode_68;
    float GraphNode_70 = EvaluateCustomNode_70(GraphNode_69, GraphNode_43);
    float GraphNode_71 = EvaluateCustomNode_71(GraphNode_70, GraphNode_40);
    float GraphNode_15 = GraphNode_4.a;
    float GraphNode_72 = 0.960000f;
    float GraphNode_73 = clamp(GraphNode_71, GraphNode_15, GraphNode_72);
    float GraphNode_74 = GraphNode_73 * GraphNode_34;
    float GraphNode_85 = EvaluateCustomNode_85(GraphNode_38, GraphNode_81);
    float GraphNode_91 = EvaluateCustomNode_91(GraphNode_81, GraphNode_85);
    float GraphNode_92 = GraphNode_74 * GraphNode_91;
    float GraphNode_93 = EvaluateCustomNode_93(GraphNode_92, GraphNode_88);
    float GraphNode_94 = GraphNode_93 * GraphNode_87;
    float GraphNode_95 = EvaluateCustomNode_95(GraphNode_94, GraphNode_90);
    float4 GraphNode_101 = float4(GraphNode_100, GraphNode_95);
    float3 GraphNode_48 = CameraVectorWS;
    float3 GraphNode_56 = EvaluateCustomNode_56(GraphNode_51, GraphNode_48);
    float GraphNode_57 = EvaluateCustomNode_57(GraphNode_47, GraphNode_56);
    float GraphNode_22 = GraphNode_6.b;
    float GraphNode_58 = pow(GraphNode_57, GraphNode_22);
    float GraphNode_21 = GraphNode_6.g;
    float GraphNode_59 = GraphNode_58 * GraphNode_21;
    float GraphNode_24 = GraphNode_7.r;
    float GraphNode_103 = EvaluateCustomNode_103(GraphNode_50, GraphNode_24);
    float GraphNode_102 = EvaluateCustomNode_102(GraphNode_44, GraphNode_19);
    float GraphNode_104 = EvaluateCustomNode_104(GraphNode_103, GraphNode_102);
    float GraphNode_105 = EvaluateCustomNode_105(GraphNode_59, GraphNode_104);
    float GraphNode_106 = EvaluateCustomNode_106(GraphNode_105, GraphNode_43);
    float GraphNode_107 = EvaluateCustomNode_107(GraphNode_106, GraphNode_87);
    float3 GraphNode_108 = EvaluateCustomNode_108(GraphNode_10, GraphNode_107);
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
    Result.BaseColor = GraphNode_101;
#define MATERIAL_GRAPH_HAS_EMISSIVE_COLOR 1
    Result.EmissiveColor = GraphNode_108;
    return Result;
}
FMaterialGraphOutput EvaluateMaterialGraph(float2 UV)
{
    return EvaluateMaterialGraph(UV, float3(0, 0, 0), float3(0, 0, 1), float3(0, 0, 1), float4(1, 1, 1, 1), 0.0f);
}
