#include "Component/AeonixVolumeComponent.h"
#include "AeonixNavigation.h"
#include "Data/AeonixNavDataAsset.h"
#include "Debug/AeonixDebugDrawManager.h"
#include "Interface/AeonixCollisionQueryInterface.h"
#include "Subsystem/AeonixCollisionSubsystem.h"
#include "Subsystem/AeonixSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	/** Feeds the octree generator nav-space boxes as world-space boxes rotated into the volume's frame */
	class FAeonixNavSpaceCollisionQuery : public IAeonixCollisionQueryInterface
	{
	public:
		FAeonixNavSpaceCollisionQuery(const IAeonixCollisionQueryInterface& InSource, const FTransform& InNavToWorld)
			: Source(InSource)
			, NavToWorld(InNavToWorld)
		{
		}

		bool IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
		{
			return Source.IsBlockedOriented(NavToWorld.TransformPosition(Position), NavToWorld.GetRotation(), VoxelSize, CollisionChannel, AgentRadius);
		}

		bool IsLeafBlocked(const FVector& Position, const float LeafSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
		{
			return Source.IsLeafBlockedOriented(NavToWorld.TransformPosition(Position), NavToWorld.GetRotation(), LeafSize, CollisionChannel, AgentRadius);
		}

	private:
		const IAeonixCollisionQueryInterface& Source;
		FTransform NavToWorld;
	};

	// Deeper octrees grow memory and generation time by 8x per level
	constexpr int32 MaxFittedOctreeDepth = 10;
}

UAeonixVolumeComponent::UAeonixVolumeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	Mobility = EComponentMobility::Movable;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetUsingAbsoluteScale(true);
	bHiddenInGame = true;
	ShapeColor = FColor(80, 200, 255);
	InitBoxExtent(FVector(HalfExtent));
}

void UAeonixVolumeComponent::OnRegister()
{
	SyncBoxExtent();
	Super::OnRegister();

	if (UWorld* World = GetWorld())
	{
		if (UAeonixSubsystem* Subsystem = World->GetSubsystem<UAeonixSubsystem>())
		{
			Subsystem->RegisterVolume(this);
		}
	}
}

void UAeonixVolumeComponent::OnUnregister()
{
	if (UWorld* World = GetWorld())
	{
		if (UAeonixSubsystem* Subsystem = World->GetSubsystem<UAeonixSubsystem>())
		{
			Subsystem->UnRegisterVolume(this);
		}
	}

	Super::OnUnregister();
}

void UAeonixVolumeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bIsReadyForNavigation)
	{
		return;
	}

	if (GenerationParameters.GenerationStrategy == ESVOGenerationStrategy::UseBaked)
	{
		if (!LoadFromAsset())
		{
			UE_LOG(LogAeonixNavigation, Warning, TEXT("Aeonix volume component %s on %s uses baked data but has none"), *GetName(), *GetNameSafe(GetOwner()));
		}
	}
	else
	{
		Generate();
	}
}

#if WITH_EDITOR
void UAeonixVolumeComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SyncBoxExtent();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UAeonixVolumeComponent::CancelPathRequests()
{
	if (UAeonixSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UAeonixSubsystem>() : nullptr)
	{
		Subsystem->CancelPathRequestsForVolume(*this);
	}
}

void UAeonixVolumeComponent::SyncBoxExtent()
{
	if (!GetUnscaledBoxExtent().Equals(FVector(HalfExtent)))
	{
		SetBoxExtent(FVector(HalfExtent), false);
	}
}

bool UAeonixVolumeComponent::HasData() const
{
	return NavigationData.OctreeData.LeafNodes.Num() > 0;
}

bool UAeonixVolumeComponent::IsPointInside(const FVector& WorldPosition) const
{
	const FVector NavPosition = WorldToNav(WorldPosition);
	return NavPosition.GetAbsMax() <= HalfExtent;
}

FTransform UAeonixVolumeComponent::GetNavToWorld() const
{
	return FTransform(GetComponentQuat(), GetComponentLocation());
}

IAeonixCollisionQueryInterface* UAeonixVolumeComponent::ResolveCollisionSource() const
{
	if (UActorComponent* SourceComponent = CollisionSource.GetComponent(GetOwner()))
	{
		if (IAeonixCollisionQueryInterface* Source = Cast<IAeonixCollisionQueryInterface>(SourceComponent))
		{
			return Source;
		}
		UE_LOG(LogAeonixNavigation, Warning, TEXT("Aeonix volume component %s: collision source %s does not implement IAeonixCollisionQueryInterface, using world physics"),
			*GetName(), *SourceComponent->GetName());
	}

	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UAeonixCollisionSubsystem>() : nullptr;
}

float UAeonixVolumeComponent::GetLeafVoxelSize(int32 Depth) const
{
	// Layer 0 nodes split the cube 2^Depth ways per axis; each holds a 4x4x4 leaf
	return HalfExtent * 2.f / FMath::Pow(2.f, Depth) * 0.25f;
}

bool UAeonixVolumeComponent::Generate()
{
	const IAeonixCollisionQueryInterface* Source = ResolveCollisionSource();
	if (!Source)
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("Aeonix volume component %s: no collision source"), *GetName());
		return false;
	}
	return GenerateWith(*Source);
}

bool UAeonixVolumeComponent::GenerateWith(const IAeonixCollisionQueryInterface& Source)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FTransform NavToWorld = GetNavToWorld();

	FAeonixGenerationParameters Params = GenerationParameters;
	Params.Origin = FVector::ZeroVector;
	Params.Extents = FVector(HalfExtent);
	if (World->ViewLocationsRenderedLastFrame.Num() > 0)
	{
		Params.DebugPosition = NavToWorld.InverseTransformPosition(World->ViewLocationsRenderedLastFrame[0]);
	}

	if (UAeonixDebugDrawManager* DebugManager = World->GetSubsystem<UAeonixDebugDrawManager>())
	{
		DebugManager->Clear(EAeonixDebugCategory::Octree);
	}

	CancelPathRequests();
	const double StartTime = FPlatformTime::Seconds();
	{
		FWriteScopeLock WriteLock(*OctreeDataLock);
		NavigationData.ResetForGeneration();
		NavigationData.UpdateGenerationParameters(Params);
		const FAeonixNavSpaceCollisionQuery NavSpaceSource(Source, NavToWorld);
		NavigationData.Generate(*World, NavSpaceSource, *this);
	}

	UE_LOG(LogAeonixNavigation, Display, TEXT("Aeonix volume component %s generated in %.1fms: depth %d, %d leaf nodes, %d bytes"),
		*GetName(), (FPlatformTime::Seconds() - StartTime) * 1000.0, Params.OctreeDepth,
		NavigationData.OctreeData.LeafNodes.Num(), NavigationData.OctreeData.GetSize());

	bIsReadyForNavigation = true;
	OnNavigationRegenerated.Broadcast(this);
	return true;
}

void UAeonixVolumeComponent::ClearData()
{
	CancelPathRequests();
	FWriteScopeLock WriteLock(*OctreeDataLock);
	NavigationData.ResetForGeneration();
	bIsReadyForNavigation = false;
}

bool UAeonixVolumeComponent::FitToCollisionSource()
{
	const IAeonixCollisionQueryInterface* Source = ResolveCollisionSource();
	return Source && FitToSource(*Source);
}

bool UAeonixVolumeComponent::FitToSource(const IAeonixCollisionQueryInterface& Source)
{
	FBox LocalBounds(ForceInit);
	if (!Source.GetBlockingBounds(GetNavToWorld(), LocalBounds) || !LocalBounds.IsValid)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("Aeonix volume component %s: collision source can't report blocking bounds"), *GetName());
		return false;
	}

	Modify();
	SetWorldLocation(GetNavToWorld().TransformPosition(LocalBounds.GetCenter()));
	HalfExtent = LocalBounds.GetExtent().GetMax() + FitPadding;

	if (TargetLeafVoxelSize > 0.f)
	{
		int32 Depth = 1;
		while (Depth < MaxFittedOctreeDepth && GetLeafVoxelSize(Depth) > TargetLeafVoxelSize)
		{
			++Depth;
		}
		GenerationParameters.OctreeDepth = Depth;
	}

	SyncBoxExtent();
	return true;
}

void UAeonixVolumeComponent::StoreToAsset(UAeonixNavDataAsset* Asset)
{
	if (!Asset)
	{
		return;
	}

	{
		FReadScopeLock ReadLock(*OctreeDataLock);
		Asset->Modify();
		Asset->Store(NavigationData);
	}

	Modify();
	NavDataAsset = Asset;
}

bool UAeonixVolumeComponent::LoadFromAsset()
{
	if (!NavDataAsset || !NavDataAsset->HasData())
	{
		return false;
	}

	if (!FMath::IsNearlyEqual(NavDataAsset->Extents.X, HalfExtent))
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("Aeonix volume component %s: baked data was generated for half extent %.0f, component is %.0f. Regenerate."),
			*GetName(), NavDataAsset->Extents.X, HalfExtent);
	}

	CancelPathRequests();
	{
		FWriteScopeLock WriteLock(*OctreeDataLock);
		NavDataAsset->Restore(NavigationData, GenerationParameters);
	}

	bIsReadyForNavigation = true;
	OnNavigationRegenerated.Broadcast(this);
	return true;
}

void UAeonixVolumeComponent::AeonixDrawDebugString(const FVector& Position, const FString& String, const FColor& Color) const
{
	if (UAeonixDebugDrawManager* DebugManager = GetWorld() ? GetWorld()->GetSubsystem<UAeonixDebugDrawManager>() : nullptr)
	{
		DebugManager->AddString(NavToWorld(Position), String, Color, 1.f, EAeonixDebugCategory::Octree);
	}
}

void UAeonixVolumeComponent::AeonixDrawDebugBox(const FVector& Position, const float Size, const FColor& Color) const
{
	if (UAeonixDebugDrawManager* DebugManager = GetWorld() ? GetWorld()->GetSubsystem<UAeonixDebugDrawManager>() : nullptr)
	{
		DebugManager->AddBox(NavToWorld(Position), FVector(Size), GetComponentQuat(), Color, EAeonixDebugCategory::Octree);
	}
}

void UAeonixVolumeComponent::AeonixDrawDebugLine(const FVector& Start, const FVector& End, const FColor& Color, float Thickness) const
{
	if (UAeonixDebugDrawManager* DebugManager = GetWorld() ? GetWorld()->GetSubsystem<UAeonixDebugDrawManager>() : nullptr)
	{
		DebugManager->AddLine(NavToWorld(Start), NavToWorld(End), Color, Thickness, EAeonixDebugCategory::Octree);
	}
}

void UAeonixVolumeComponent::AeonixDrawDebugDirectionalArrow(const FVector& Start, const FVector& End, const FColor& Color, float ArrowSize) const
{
	if (UAeonixDebugDrawManager* DebugManager = GetWorld() ? GetWorld()->GetSubsystem<UAeonixDebugDrawManager>() : nullptr)
	{
		DebugManager->AddArrow(NavToWorld(Start), NavToWorld(End), ArrowSize, Color, 0.0f, EAeonixDebugCategory::Octree);
	}
}
