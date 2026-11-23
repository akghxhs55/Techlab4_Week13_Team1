#include "DepthOfFieldLayeredCommon.hlsli"

Texture2D<float4> SceneColor : register(t0);
Texture2D<float> SceneDepth : register(t1);
Texture2D<float> CoCTexture : register(t2);
Texture2D<float2> TileRanges : register(t3);

DOFVSOutput mainVS(uint VertexID : SV_VertexID)
{
    return DOFMainVS(VertexID);
}

struct DOFGatherOutput
{
    float4 NearColor : SV_Target0;
    float4 FarColor : SV_Target1;
};

DOFGatherOutput mainPS(DOFVSOutput Input)
{
    int2 Pixel = int2(Input.Position.xy);
    int2 ImageSize = int2(1.0f / InvWidth, 1.0f / InvHeight);
    float2 UV = Input.Position.xy * float2(InvWidth, InvHeight);
    float CenterDepth = DOFViewDepth(SceneDepth.Load(int3(Pixel, 0)));
    float CenterCoC = CoCTexture.Load(int3(Pixel, 0));
    float4 CenterColor = SceneColor.Load(int3(Pixel, 0));

    uint TileWidth, TileHeight;
    TileRanges.GetDimensions(TileWidth, TileHeight);
    int2 Tile = Pixel / 16;
    float2 NearbyMaximum = 0.0f;
    [unroll] for (int Y = -2; Y <= 2; ++Y)
    {
        [unroll] for (int X = -2; X <= 2; ++X)
        {
            int2 Neighbor = clamp(Tile + int2(X, Y), int2(0, 0), int2(TileWidth - 1, TileHeight - 1));
            NearbyMaximum = max(NearbyMaximum, TileRanges.Load(int3(Neighbor, 0)));
        }
    }
    float NearSearchRadius = min(NearbyMaximum.x, MaxRadiusPixels);
    float FarSearchRadius = min(NearbyMaximum.y, MaxRadiusPixels);

    float3 NearSum = CenterCoC < -0.5f ? CenterColor.rgb : float3(0.0f, 0.0f, 0.0f);
    float3 FarSum = CenterCoC > 0.5f ? CenterColor.rgb : float3(0.0f, 0.0f, 0.0f);
    float NearWeight = CenterCoC < -0.5f ? 1.0f : 0.0f;
    float FarWeight = CenterCoC > 0.5f ? 1.0f : 0.0f;
    float NearKernelWeight = 1.0f;
    if (NearSearchRadius > 0.5f)
    {
        [unroll] for (int Ring = 1; Ring <= 4; ++Ring)
        {
            [unroll] for (int Sample = 0; Sample < 12; ++Sample)
            {
                float Angle = 6.2831853f * (Sample + 0.5f * (Ring & 1)) / 12.0f;
                float2 Offset = float2(cos(Angle), sin(Angle)) * NearSearchRadius * sqrt((Ring - 0.5f) / 4.0f) * DOFBladeScale(Angle);
                float Distance = length(Offset);
                float KernelWeight = saturate(NearSearchRadius - Distance + 0.5f);
                NearKernelWeight += KernelWeight;
                if (KernelWeight <= 0.0f) continue;
                float2 SampleUV = saturate(UV + Offset * float2(InvWidth, InvHeight));
                int2 SamplePixel = clamp(int2(SampleUV * ImageSize), int2(0, 0), ImageSize - 1);
                float SampleCoC = CoCTexture.Load(int3(SamplePixel, 0));
                if (SampleCoC >= -0.5f) continue;
                float SampleDepth = DOFViewDepth(SceneDepth.Load(int3(SamplePixel, 0)));
                float DepthTolerance = max(CenterDepth * 0.05f, 5.0f);
                float Visibility = 1.0f - smoothstep(DepthTolerance, 2.0f * DepthTolerance, SampleDepth - CenterDepth);
                float Coverage = min(KernelWeight, saturate(-SampleCoC - Distance + 0.5f)) * Visibility;
                if (Coverage > 0.0f)
                {
                    NearSum += SceneColor.Load(int3(SamplePixel, 0)).rgb * Coverage;
                    NearWeight += Coverage;
                }
            }
        }
    }

    if (FarSearchRadius > 0.5f)
    {
        [unroll] for (int Ring = 1; Ring <= 4; ++Ring)
        {
            [unroll] for (int Sample = 0; Sample < 12; ++Sample)
            {
                float Angle = 6.2831853f * (Sample + 0.5f * (Ring & 1)) / 12.0f;
                float2 Offset = float2(cos(Angle), sin(Angle)) * FarSearchRadius * (Ring / 4.0f) * DOFBladeScale(Angle);
                float2 SampleUV = saturate(UV + Offset * float2(InvWidth, InvHeight));
                int2 SamplePixel = clamp(int2(SampleUV * ImageSize), int2(0, 0), ImageSize - 1);
                float SampleCoC = CoCTexture.Load(int3(SamplePixel, 0));
                if (SampleCoC <= 0.5f) continue;
                float SampleDepth = DOFViewDepth(SceneDepth.Load(int3(SamplePixel, 0)));
                float Distance = length(Offset);
                float Coverage = saturate(SampleCoC - Distance + 0.5f);
                if (Coverage <= 0.0f || SampleDepth > CenterDepth + max(CenterDepth * 0.01f, 1.0f)) continue;
                FarSum += SceneColor.Load(int3(SamplePixel, 0)).rgb * Coverage;
                FarWeight += Coverage;
            }
        }
    }

    float NearAlpha = saturate(NearWeight / NearKernelWeight);
    float FarAlpha = CenterCoC > 0.5f ? saturate(CenterCoC / 2.0f) : 0.0f;
    DOFGatherOutput Output;
    Output.NearColor = float4(NearWeight > 0.0f ? NearSum / NearWeight * NearAlpha : float3(0.0f, 0.0f, 0.0f), NearAlpha);
    Output.FarColor = float4(FarWeight > 0.0f ? FarSum / FarWeight : CenterColor.rgb, FarAlpha);
    return Output;
}
