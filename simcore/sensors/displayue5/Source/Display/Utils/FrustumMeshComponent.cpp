
#include "FrustumMeshComponent.h"

#include "DynamicMeshBuilder.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "LocalVertexFactory.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "SceneManagement.h"
#include "StaticMeshResources.h"
#include "UObject/ConstructorHelpers.h"
#include "VertexFactory.h"

class FFrustumMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
    SIZE_T GetTypeHash() const override
    {
        static size_t UniquePointer;
        return reinterpret_cast<size_t>(&UniquePointer);
    }

    FFrustumMeshSceneProxy(UFrustumMeshComponent* Component)
        : FPrimitiveSceneProxy(Component)
        , frustum_mesh_component_(Component)
        , VertexFactory(GetScene().GetFeatureLevel(), "FFrustumMeshSceneProxy")
        , MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
    {
        const FColor VertexColor(255, 255, 255);

        int32 num_vertices = 8;

        TArray<FDynamicMeshVertex> Vertices;
        const int32 NumTris = 12;
        Vertices.AddUninitialized(8);
        IndexBuffer.Indices.AddUninitialized(NumTris * 3);

        for (int i = 0; i < 8; ++i)
        {
            FDynamicMeshVertex vert;
            vert.Color = VertexColor;
            vert.Position = FVector3f(
                Component->frusum_points_[i].X, Component->frusum_points_[i].Y, Component->frusum_points_[i].Z);
            Vertices[i] = vert;
        }

        // front
        IndexBuffer.Indices[0] = 0;
        IndexBuffer.Indices[1] = 1;
        IndexBuffer.Indices[2] = 2;
        IndexBuffer.Indices[3] = 0;
        IndexBuffer.Indices[4] = 2;
        IndexBuffer.Indices[5] = 3;

        // left
        IndexBuffer.Indices[6] = 1;
        IndexBuffer.Indices[7] = 5;
        IndexBuffer.Indices[8] = 6;
        IndexBuffer.Indices[9] = 1;
        IndexBuffer.Indices[10] = 6;
        IndexBuffer.Indices[11] = 2;

        // right
        IndexBuffer.Indices[12] = 0;
        IndexBuffer.Indices[13] = 3;
        IndexBuffer.Indices[14] = 7;
        IndexBuffer.Indices[15] = 4;
        IndexBuffer.Indices[16] = 0;
        IndexBuffer.Indices[17] = 7;

        // back
        IndexBuffer.Indices[18] = 4;
        IndexBuffer.Indices[19] = 7;
        IndexBuffer.Indices[20] = 6;
        IndexBuffer.Indices[21] = 4;
        IndexBuffer.Indices[22] = 6;
        IndexBuffer.Indices[23] = 5;

        // top
        IndexBuffer.Indices[24] = 5;
        IndexBuffer.Indices[25] = 1;
        IndexBuffer.Indices[26] = 0;
        IndexBuffer.Indices[27] = 5;
        IndexBuffer.Indices[28] = 0;
        IndexBuffer.Indices[29] = 4;

        // bottom
        IndexBuffer.Indices[30] = 7;
        IndexBuffer.Indices[31] = 3;
        IndexBuffer.Indices[32] = 2;
        IndexBuffer.Indices[33] = 7;
        IndexBuffer.Indices[34] = 2;
        IndexBuffer.Indices[35] = 6;

        VertexBuffers.InitFromDynamicVertex(&VertexFactory, Vertices);

        // Enqueue initialization of render resource
        BeginInitResource(&VertexBuffers.PositionVertexBuffer);
        BeginInitResource(&VertexBuffers.StaticMeshVertexBuffer);
        BeginInitResource(&VertexBuffers.ColorVertexBuffer);
        BeginInitResource(&IndexBuffer);
        BeginInitResource(&VertexFactory);

        // Grab material
        Material = Component->GetMaterial(0);
        if (Material == NULL)
        {
            Material = UMaterial::GetDefaultMaterial(MD_Surface);
        }
    }

    virtual ~FFrustumMeshSceneProxy()
    {
        VertexBuffers.PositionVertexBuffer.ReleaseResource();
        VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
        VertexBuffers.ColorVertexBuffer.ReleaseResource();
        IndexBuffer.ReleaseResource();
        VertexFactory.ReleaseResource();
    }

    virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
        uint32 VisibilityMap, FMeshElementCollector& Collector) const override
    {
        QUICK_SCOPE_CYCLE_COUNTER(STAT_CustomMeshSceneProxy_GetDynamicMeshElements);


        FMaterialRenderProxy* MaterialProxy = Material->GetRenderProxy();

        for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
        {
            if (VisibilityMap & (1 << ViewIndex))
            {
                const FSceneView* View = Views[ViewIndex];
                // Draw the mesh.
                FMeshBatch& Mesh = Collector.AllocateMesh();
                FMeshBatchElement& BatchElement = Mesh.Elements[0];
                BatchElement.IndexBuffer = &IndexBuffer;
                Mesh.bWireframe = false;
                Mesh.VertexFactory = &VertexFactory;
                Mesh.MaterialRenderProxy = MaterialProxy;

                bool bHasPrecomputedVolumetricLightmap;
                FMatrix PreviousLocalToWorld;
                int32 SingleCaptureIndex;
                bool bOutputVelocity;
                GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(),
                    bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

                FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer =
                     Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
                DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(),
                     GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, DrawsVelocity());
                // BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

                // BatchElement.FirstIndex = 0;
                // BatchElement.NumPrimitives = IndexBuffer.Indices.Num() / 3;
                // BatchElement.MinVertexIndex = 0;
                // BatchElement.MaxVertexIndex = VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;
                // Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
                // Mesh.Type = PT_TriangleList;
                // Mesh.DepthPriorityGroup = SDPG_World;
                // Mesh.bCanApplyViewModeOverrides = false;
                // Collector.AddMesh(ViewIndex, Mesh);
            }
        }
    }

    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
    {
        FPrimitiveViewRelevance Result;
        Result.bDrawRelevance = IsShown(View);
        Result.bShadowRelevance = IsShadowCast(View);
        Result.bDynamicRelevance = true;
        Result.bRenderInMainPass = ShouldRenderInMainPass();
        Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
        Result.bRenderCustomDepth = ShouldRenderCustomDepth();
        Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
        MaterialRelevance.SetPrimitiveViewRelevance(Result);
        Result.bVelocityRelevance = IsMovable() && Result.bOpaque && Result.bRenderInMainPass;
        return Result;
    }

    virtual bool CanBeOccluded() const override
    {
        return !MaterialRelevance.bDisableDepthTest;
    }

    virtual uint32 GetMemoryFootprint(void) const override
    {
        return (sizeof(*this) + GetAllocatedSize());
    }

    uint32 GetAllocatedSize(void) const
    {
        return (FPrimitiveSceneProxy::GetAllocatedSize());
    }

private:
    UMaterialInterface* Material;
    FStaticMeshVertexBuffers VertexBuffers;
    FDynamicMeshIndexBuffer32 IndexBuffer;
    UFrustumMeshComponent* frustum_mesh_component_;
    FLocalVertexFactory VertexFactory;
    FMaterialRelevance MaterialRelevance;
};

//////////////////////////////////////////////////////////////////////////

UFrustumMeshComponent::UFrustumMeshComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = false;

    SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);

    // static ConstructorHelpers::FObjectFinder<UMaterial> wireframe_material_finder(
    //  TEXT("Material'/Game/Materials/WireframeMaterial.WireframeMaterial'"));
    // if (wireframe_material_finder.Succeeded()) {
    //  wireframe_material_ = wireframe_material_finder.Object;
    //}
    // else {
    //  UE_LOG(LogTemp, Error, TEXT("Failed to find frustum material"));
    //}

    // SetMaterial(0, frustum_material_);

    fov_x_ = 90.0f;
    frustum_start_distance_ = 100.0f;
    frustum_end_distance_ = 3000.0f;
    aspect_ratio_ = 1.7777777777778f;

    wireframe_ = false;
}

void UFrustumMeshComponent::SetDrawingParameters(
    float start_dis, float end_dis, float fov_x, float ratio, bool wireframe)
{
    frustum_start_distance_ = start_dis;
    frustum_end_distance_ = end_dis;
    fov_x_ = fov_x;
    aspect_ratio_ = ratio;
    wireframe_ = wireframe;
    BuildMeshData();
}

void UFrustumMeshComponent::BuildMeshData()
{
    FVector Direction(1, 0, 0);
    FVector LeftVector(0, 1, 0);
    FVector UpVector(0, 0, 1);

    const float HozHalfAngleInRadians = FMath::DegreesToRadians(fov_x_ * 0.5f);

    float HozLength = 0.0f;
    float VertLength = 0.0f;

    HozLength = frustum_start_distance_ * FMath::Tan(HozHalfAngleInRadians);
    VertLength = HozLength / aspect_ratio_;

    frusum_points_.Empty();
    frusum_points_.AddUninitialized(8);

    frusum_points_[0] = (Direction * frustum_start_distance_) + (UpVector * VertLength) + (LeftVector * HozLength);
    frusum_points_[1] = (Direction * frustum_start_distance_) + (UpVector * VertLength) - (LeftVector * HozLength);
    frusum_points_[2] = (Direction * frustum_start_distance_) - (UpVector * VertLength) - (LeftVector * HozLength);
    frusum_points_[3] = (Direction * frustum_start_distance_) - (UpVector * VertLength) + (LeftVector * HozLength);

    HozLength = frustum_end_distance_ * FMath::Tan(HozHalfAngleInRadians);
    VertLength = HozLength / aspect_ratio_;

    frusum_points_[4] = (Direction * frustum_end_distance_) + (UpVector * VertLength) + (LeftVector * HozLength);
    frusum_points_[5] = (Direction * frustum_end_distance_) + (UpVector * VertLength) - (LeftVector * HozLength);
    frusum_points_[6] = (Direction * frustum_end_distance_) - (UpVector * VertLength) - (LeftVector * HozLength);
    frusum_points_[7] = (Direction * frustum_end_distance_) - (UpVector * VertLength) + (LeftVector * HozLength);

    ENQUEUE_RENDER_COMMAND(MarkDirtyCommand)([this](FRHICommandListImmediate& RHICmdList) { MarkRenderStateDirty(); });
}

FPrimitiveSceneProxy* UFrustumMeshComponent::CreateSceneProxy()
{
    FPrimitiveSceneProxy* Proxy = NULL;
    Proxy = new FFrustumMeshSceneProxy(this);
    return Proxy;
}

int32 UFrustumMeshComponent::GetNumMaterials() const
{
    return 1;
}

FBoxSphereBounds UFrustumMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    FBox BoundingBox(ForceInit);

    // Bounds are tighter if the box is generated from pre-transformed vertices.
    for (int32 Index = 0; Index < frusum_points_.Num(); ++Index)
    {
        BoundingBox += LocalToWorld.TransformPosition(frusum_points_[Index]);
        BoundingBox += LocalToWorld.TransformPosition(frusum_points_[Index]);
        BoundingBox += LocalToWorld.TransformPosition(frusum_points_[Index]);
    }

    FBoxSphereBounds NewBounds;
    NewBounds.BoxExtent = BoundingBox.GetExtent();
    NewBounds.Origin = BoundingBox.GetCenter();
    NewBounds.SphereRadius = NewBounds.BoxExtent.Size();

    return NewBounds;
}
