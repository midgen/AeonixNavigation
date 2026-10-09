#pragma once

#include "Data/AeonixData.h"
#include "Interface/AeonixDebugDrawInterface.h"
#include "Interface/AeonixNavVolumeInterface.h"

#include "Components/BoxComponent.h"
#include "Engine/EngineTypes.h"

#include "AeonixVolumeComponent.generated.h"

class IAeonixCollisionQueryInterface;
class UAeonixNavDataAsset;

/**
 *  A navigable cube that travels with whatever it is attached to.
 *
 *  Navigation data lives in the component's own frame (location + rotation; scale is ignored and
 *  the component uses absolute scale), so a volume attached to a moving, rotating actor stays
 *  valid without regenerating. Paths planned in it carry nav-space points and re-project to
 *  world space through the component's current transform.
 *
 *  Blocking geometry comes from CollisionSource, a component on the same actor implementing
 *  IAeonixCollisionQueryInterface, or from world physics when none is set. Dynamic regions are
 *  not supported.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), hidecategories = (Shape, Collision, Physics, Navigation, HLOD, Mobile))
class AEONIXNAVIGATION_API UAeonixVolumeComponent : public UBoxComponent, public IAeonixNavVolumeInterface, public IAeonixDebugDrawInterface
{
	GENERATED_BODY()

public:
	UAeonixVolumeComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin UActorComponent
	void OnRegister() override;
	void OnUnregister() override;
	void BeginPlay() override;
	//~ End UActorComponent

#if WITH_EDITOR
	//~ Begin UObject
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject
#endif

	//~ Begin IAeonixNavVolumeInterface
	const FAeonixData& GetNavData() const override { return NavigationData; }
	FAeonixData& GetMutableNavData() override { return NavigationData; }
	TSharedRef<FRWLock, ESPMode::ThreadSafe> GetSharedOctreeDataLock() const override { return OctreeDataLock; }
	bool HasData() const override;
	bool IsReadyForNavigation() const override { return bIsReadyForNavigation; }
	bool IsPointInside(const FVector& WorldPosition) const override;
	FTransform GetNavToWorld() const override;
	const FAeonixGenerationParameters& GetGenerationParameters() const override { return GenerationParameters; }
	FOnAeonixNavVolumeRegenerated& GetOnNavigationRegenerated() override { return OnNavigationRegenerated; }
	bool Generate() override;
	void ClearData() override;
	//~ End IAeonixNavVolumeInterface

	/** Generate against an explicit source instead of CollisionSource */
	bool GenerateWith(const IAeonixCollisionQueryInterface& Source);

	/**
	 * Centre the cube on the collision source's blocking bounds and size it to enclose them plus
	 * FitPadding. With TargetLeafVoxelSize set, also picks the smallest OctreeDepth whose leaf
	 * voxels are no larger than it. Returns false if the source can't report bounds.
	 */
	bool FitToCollisionSource();

	/** FitToCollisionSource against an explicit source */
	bool FitToSource(const IAeonixCollisionQueryInterface& Source);

	/** Copy the current navigation data into an asset and reference it */
	void StoreToAsset(UAeonixNavDataAsset* Asset);

	/** Load NavDataAsset into the volume. Returns false if there is no baked data. */
	bool LoadFromAsset();

	/** The source answering blocking queries: CollisionSource if set and valid, else world physics */
	IAeonixCollisionQueryInterface* ResolveCollisionSource() const;

	/** Leaf voxel edge length for an octree depth at the current HalfExtent */
	float GetLeafVoxelSize(int32 Depth) const;

	/** Half the side of the navigable cube, in world units. The cube is centred on the component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aeonix", meta = (ClampMin = "1.0", Units = "cm"))
	float HalfExtent = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix")
	FAeonixGenerationParameters GenerationParameters;

	/** Baked navigation data, loaded on BeginPlay when GenerationStrategy is Use Baked */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aeonix")
	TObjectPtr<UAeonixNavDataAsset> NavDataAsset;

	/** Component on the owning actor implementing IAeonixCollisionQueryInterface. Empty uses world physics on GenerationParameters.CollisionChannel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aeonix", meta = (UseComponentPicker, AllowAnyActor = "false"))
	FComponentReference CollisionSource;

	/** Clearance added around the blocking bounds by FitToCollisionSource */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aeonix|Fit", meta = (ClampMin = "0.0", Units = "cm"))
	float FitPadding = 0.f;

	/** Largest acceptable leaf voxel for FitToCollisionSource; 0 leaves OctreeDepth alone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aeonix|Fit", meta = (ClampMin = "0.0", Units = "cm"))
	float TargetLeafVoxelSize = 0.f;

	FOnAeonixNavVolumeRegenerated OnNavigationRegenerated;

protected:
	//~ Begin IAeonixDebugDrawInterface
	void AeonixDrawDebugString(const FVector& Position, const FString& String, const FColor& Color) const override;
	void AeonixDrawDebugBox(const FVector& Position, const float Size, const FColor& Color) const override;
	void AeonixDrawDebugLine(const FVector& Start, const FVector& End, const FColor& Color, float Thickness = 0.0f) const override;
	void AeonixDrawDebugDirectionalArrow(const FVector& Start, const FVector& End, const FColor& Color, float ArrowSize = 0.0f) const override;
	//~ End IAeonixDebugDrawInterface

private:
	void SyncBoxExtent();

	/** Before the octree is rebuilt: in-flight async pathfinds hold links into the old one */
	void CancelPathRequests();

	FAeonixData NavigationData;
	TSharedRef<FRWLock, ESPMode::ThreadSafe> OctreeDataLock = MakeShared<FRWLock, ESPMode::ThreadSafe>();
	bool bIsReadyForNavigation = false;
};
