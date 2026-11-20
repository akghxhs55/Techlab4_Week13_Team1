#include "EnginePCH.h"
#include "Mesh.h"
#include "Renderer.h"
#include "Asset/AssetManager.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Physics/BodySetup.h"
#include <algorithm>
#include <cmath>
#include <limits>

UStaticMesh::~UStaticMesh()
{
    for (FPrimitiveSceneProxy* Proxy : RenderProxies)
        Proxy->OnMeshDestroyed();
	VertexBuffer = nullptr;
	IndexBuffer = nullptr;
    delete BodySetup;
}

UBodySetup* UStaticMesh::GetBodySetup()
{
    if (BodySetup)
        return BodySetup;

    BodySetup = NewObject<UBodySetup>(this);

    // RenderBounds는 RebuildRenderData 때만 채워지므로 원본 AABB를 쓴다
    const FBox& Bounds = MeshData.AABB;
    // 평면 메시(바닥 Plane 등)는 두께가 0이라 PhysX가 박스를 만들지 못한다. 최소 두께를 준다
    constexpr float MinThickness = 0.01f;

    FKBoxElem& Box = BodySetup->AggGeom.BoxElems.Emplace_GetRef();
    Box.Center = (Bounds.Min + Bounds.Max) * 0.5f;
    Box.X = std::max(Bounds.Max.X - Bounds.Min.X, MinThickness);
    Box.Y = std::max(Bounds.Max.Y - Bounds.Min.Y, MinThickness);
    Box.Z = std::max(Bounds.Max.Z - Bounds.Min.Z, MinThickness);

    return BodySetup;
}

UMaterial* UStaticMesh::GetMaterial(uint32 SlotIndex) const
{
	if (SlotIndex < Materials.size() && Materials[SlotIndex]) {
		return Materials[SlotIndex];
	}
	return UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial");
}

size_t UStaticMesh::FMaterialSlotsHash::operator()(const std::vector<UMaterial*>& Slots) const
{
    size_t Hash = 0;
    for (const UMaterial* Material : Slots)
        Hash ^= std::hash<const UMaterial*>{}(Material) + 0x9e3779b9u + (Hash << 6) + (Hash >> 2);
    return Hash;
}

void UStaticMesh::RebuildRenderData()
{
    RenderBounds = MeshData.AABB;
    RenderStates.clear();
    for (FPrimitiveSceneProxy* Proxy : RenderProxies)
        Proxy->OnMeshRenderDataChanged();
}

TSharedPtr<const FMeshRenderState> UStaticMesh::GetRenderState(const std::vector<UMaterial*>& Materials)
{
    auto& WeakState = RenderStates[Materials];
    if (auto State = WeakState.lock()) return State;
    auto State = MakeShared<FMeshRenderState>();
    for (const FStaticMeshSection& Section : MeshData.Sections)
    {
        UMaterial* Material = Section.MaterialSlotIndex < Materials.size()
            ? Materials[Section.MaterialSlotIndex] : GetMaterial(Section.MaterialSlotIndex);
        if (Material) State->Sections.Add({ Material, Section.StartIndex, Section.IndexCount });
    }
    WeakState = State;
    return State;
}

void UStaticMesh::RegisterProxy(FPrimitiveSceneProxy* Proxy)
{
    RenderProxies.Add(Proxy);
}

void UStaticMesh::UnregisterProxy(FPrimitiveSceneProxy* Proxy)
{
    const auto It = std::find(RenderProxies.begin(), RenderProxies.end(), Proxy);
    if (It != RenderProxies.end())
        RenderProxies.RemoveAtSwap(static_cast<uint32>(It - RenderProxies.begin()));
}
