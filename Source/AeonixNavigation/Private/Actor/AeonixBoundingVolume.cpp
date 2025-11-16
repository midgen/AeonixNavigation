

#include "Actor/AeonixBoundingVolume.h"
#include "Subsystem/AeonixSubsystem.h"
#include "Subsystem/AeonixCollisionSubsystem.h"
#include "AeonixNavigation.h"
#include "Debug/AeonixDebugDrawManager.h"

#include "Components/BrushComponent.h"
#include "Components/LineBatchComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "Serialization/CustomVersion.h"

#include <chrono>

using namespace std::chrono;

// Custom version for AeonixBoundingVolume serialization
namespace FAeonixBoundingVolumeVersion
{
	enum Type
	{
		// Before any custom version was added
		BeforeCustomVersionWasAdded = 0,
		// Added serialization of Origin and Extents for baked navigation data
		SerializeGenerationBounds = 1,
		// Added serialization of VoxelPower for baked navigation data
		SerializeVoxelPower = 2,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// Unique GUID for this custom version
	const FGuid GUID(0x8A3F7E21, 0x4B5C8D92, 0xA7E13F64, 0x2C9B4D8F);
}

// Register the custom version
FCustomVersionRegistration GRegisterAeonixBoundingVolumeVersion(FAeonixBoundingVolumeVersion::GUID, FAeonixBoundingVolumeVersion::LatestVersion, TEXT("AeonixBoundingVolumeVer"));

AAeonixBoundingVolume::AAeonixBoundingVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->Mobility = EComponentMobility::Static;
	BrushColor = FColor(255, 255, 255, 255);
	bColored = true;
}

// Regenerates the SVO Navigation Data
bool AAeonixBoundingVolume::Generate()
{
	// Reset nav data
	NavigationData.ResetForGeneration();
	// Update parameters
	NavigationData.UpdateGenerationParameters(GenerationParameters);

	if (!CollisionQueryInterface.GetInterface())
	{
		UAeonixCollisionSubsystem* CollisionSubsystem = GetWorld()->GetSubsystem<UAeonixCollisionSubsystem>();
		CollisionQueryInterface = CollisionSubsystem;
		UE_LOG(LogAeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid CollisionQueryInterface found"));
	}

#if WITH_EDITOR

	// If we're running the game, use the first player controller position for debugging
	APlayerController* pc = GetWorld()->GetFirstPlayerController();
	if (pc)
	{
		NavigationData.SetDebugPosition(pc->GetPawn()->GetActorLocation());
	}
	// otherwise, use the viewport camera location if we're just in the editor
	else if (GetWorld()->ViewLocationsRenderedLastFrame.Num() > 0)
	{
		NavigationData.SetDebugPosition(GetWorld()->ViewLocationsRenderedLastFrame[0]);
	}

	// Clear only octree debug visualization using the debug manager (doesn't affect other systems)
	if (UAeonixDebugDrawManager* DebugManager = GetWorld()->GetSubsystem<UAeonixDebugDrawManager>())
	{
		DebugManager->Clear(EAeonixDebugCategory::Octree);
	}

	// Setup timing
	milliseconds startMs = duration_cast<milliseconds>(
		system_clock::now().time_since_epoch());

#endif // WITH_EDITOR

	UpdateBounds();

	// Acquire write lock for thread-safe octree modification
	{
		FWriteScopeLock WriteLock(OctreeDataLock);
		NavigationData.Generate(*GetWorld(), *CollisionQueryInterface.GetInterface(), *this);
	}

#if WITH_EDITOR

	int32 BuildTime = (duration_cast<milliseconds>(
						   system_clock::now().time_since_epoch()) -
					   startMs)
						  .count();

	int32 TotalNodes = 0;

	for (int i = 0; i < NavigationData.OctreeData.GetNumLayers(); i++)
	{
		TotalNodes += NavigationData.OctreeData.Layers[i].Num();
	}

	int32 TotalBytes = sizeof(AeonixNode) * TotalNodes;
	TotalBytes += sizeof(AeonixLeafNode) * NavigationData.OctreeData.LeafNodes.Num();

	UE_LOG(LogAeonixNavigation, Display, TEXT("Generation Time : %d"), BuildTime);
	UE_LOG(LogAeonixNavigation, Display, TEXT("Total Layers-Nodes : %d-%d"), NavigationData.OctreeData.GetNumLayers(), TotalNodes);
	UE_LOG(LogAeonixNavigation, Display, TEXT("Total Leaf Nodes : %d"), NavigationData.OctreeData.LeafNodes.Num());
	UE_LOG(LogAeonixNavigation, Display, TEXT("Total Size (bytes): %d"), TotalBytes);
#endif

	// Mark volume as ready for navigation after successful generation
	bIsReadyForNavigation = true;

#if WITH_EDITOR
	// Mark the actor as modified so Unreal knows to save the NavigationData
	// (NavigationData is not a UPROPERTY, so we need to manually mark as dirty)
	Modify();
	UE_LOG(LogAeonixNavigation, Log, TEXT("Actor marked as modified to ensure NavigationData is saved"));
#endif

	// Broadcast that navigation has been regenerated
	OnNavigationRegenerated.Broadcast(this);

	return true;
}

void AAeonixBoundingVolume::RegenerateDynamicSubregions()
{
	UE_LOG(LogAeonixNavigation, Display, TEXT("RegenerateDynamicSubregions called for bounding volume %s"), *GetName());

	const FAeonixGenerationParameters& Params = NavigationData.GetParams();
	if (Params.DynamicRegionBoxes.Num() == 0)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("No dynamic regions registered for bounding volume %s. Add modifier volumes with DynamicRegion type."), *GetName());
		return;
	}

	if (!CollisionQueryInterface.GetInterface())
	{
		UAeonixCollisionSubsystem* CollisionSubsystem = GetWorld()->GetSubsystem<UAeonixCollisionSubsystem>();
		CollisionQueryInterface = CollisionSubsystem;
		UE_LOG(LogAeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid CollisionQueryInterface found"));
	}

	// Clear octree debug visualization (leaf voxels need to be re-rendered with updated data)
	if (UAeonixDebugDrawManager* DebugManager = GetWorld()->GetSubsystem<UAeonixDebugDrawManager>())
	{
		DebugManager->Clear(EAeonixDebugCategory::Octree);
	}

	// Acquire write lock for thread-safe octree modification
	{
		FWriteScopeLock WriteLock(OctreeDataLock);
		NavigationData.RegenerateDynamicSubregions(*CollisionQueryInterface.GetInterface(), *this);
	}

	// Draw debug boxes showing which regions were regenerated
	for (const FBox& DynamicRegion : Params.DynamicRegionBoxes)
	{
		DrawDebugBox(GetWorld(), DynamicRegion.GetCenter(), DynamicRegion.GetExtent(),
			FColor::Cyan, false, 5.0f, 0, 2.0f);
	}

	UE_LOG(LogAeonixNavigation, Display, TEXT("RegenerateDynamicSubregions complete for bounding volume %s"), *GetName());

	// Broadcast that navigation has been regenerated
	OnNavigationRegenerated.Broadcast(this);
}

bool AAeonixBoundingVolume::HasData() const
{
	return NavigationData.OctreeData.LeafNodes.Num() > 0;
}

void AAeonixBoundingVolume::UpdateBounds()
{
	FVector Origin, Extent;
	FBox Bounds = GetComponentsBoundingBox(true);
	Bounds.GetCenterAndExtents(Origin, Extent);

	NavigationData.SetExtents(Origin, Extent);
}

void AAeonixBoundingVolume::SetDebugFilterBox(const FBox& FilterBox)
{
	GenerationParameters.DebugFilterBox = FilterBox;
	GenerationParameters.bUseDebugFilterBox = true;
	UE_LOG(LogAeonixNavigation, Log, TEXT("Bounding volume %s now using debug filter box: %s"),
		*GetName(), *FilterBox.ToString());
}

void AAeonixBoundingVolume::ClearDebugFilterBox()
{
	GenerationParameters.bUseDebugFilterBox = false;
	UE_LOG(LogAeonixNavigation, Verbose, TEXT("Bounding volume %s cleared debug filter box"), *GetName());
}

void AAeonixBoundingVolume::AddDynamicRegion(const FBox& RegionBox)
{
	GenerationParameters.DynamicRegionBoxes.Add(RegionBox);
	UE_LOG(LogAeonixNavigation, Log, TEXT("Bounding volume %s registered dynamic region box: %s"),
		*GetName(), *RegionBox.ToString());
}

void AAeonixBoundingVolume::ClearDynamicRegions()
{
	GenerationParameters.DynamicRegionBoxes.Empty();
	UE_LOG(LogAeonixNavigation, Verbose, TEXT("Bounding volume %s cleared dynamic regions"), *GetName());
}

void AAeonixBoundingVolume::AeonixDrawDebugString(const FVector& Position, const FString& String, const FColor& Color) const
{
	if (UAeonixDebugDrawManager* DebugManager = GetWorld()->GetSubsystem<UAeonixDebugDrawManager>())
	{
		DebugManager->AddString(Position, String, Color, 1.f, EAeonixDebugCategory::Octree);
	}
}

void AAeonixBoundingVolume::AeonixDrawDebugBox(const FVector& Position, const float Size, const FColor& Color) const
{
	if (UAeonixDebugDrawManager* DebugManager = GetWorld()->GetSubsystem<UAeonixDebugDrawManager>())
	{
		DebugManager->AddBox(Position, FVector(Size), FQuat::Identity, Color, EAeonixDebugCategory::Octree);
	}
}

void AAeonixBoundingVolume::AeonixDrawDebugLine(const FVector& Start, const FVector& End, const FColor& Color, float Thickness) const
{
	if (UAeonixDebugDrawManager* DebugManager = GetWorld()->GetSubsystem<UAeonixDebugDrawManager>())
	{
		DebugManager->AddLine(Start, End, Color, Thickness, EAeonixDebugCategory::Octree);
	}
}

void AAeonixBoundingVolume::AeonixDrawDebugDirectionalArrow(const FVector& Start, const FVector& End, const FColor& Color, float ArrowSize) const
{
	if (UAeonixDebugDrawManager* DebugManager = GetWorld()->GetSubsystem<UAeonixDebugDrawManager>())
	{
		DebugManager->AddArrow(Start, End, ArrowSize, Color, 0.0f, EAeonixDebugCategory::Octree);
	}
}

void AAeonixBoundingVolume::ClearData()
{
	NavigationData.ResetForGeneration();

	// Clear only octree debug visualization using the debug manager (doesn't affect other systems)
	if (UAeonixDebugDrawManager* DebugManager = GetWorld()->GetSubsystem<UAeonixDebugDrawManager>())
	{
		DebugManager->Clear(EAeonixDebugCategory::Octree);
	}
}


void AAeonixBoundingVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	AeonixSubsystemInterface = GetWorld()->GetSubsystem<UAeonixSubsystem>();
	if (!AeonixSubsystemInterface.GetInterface())
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	}
	else
	{
		AeonixSubsystemInterface->RegisterVolume(this, EAeonixMassEntityFlag::Disabled);
	}
}

void AAeonixBoundingVolume::Destroyed()
{
	if (!AeonixSubsystemInterface.GetInterface())
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	}
	else
	{
		AeonixSubsystemInterface->UnRegisterVolume(this, EAeonixMassEntityFlag::Disabled);
	}
	
	Super::Destroyed();
}

void AAeonixBoundingVolume::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Register our custom version
	Ar.UsingCustomVersion(FAeonixBoundingVolumeVersion::GUID);

	// Log the generation strategy to help diagnose serialization issues
	UE_LOG(LogAeonixNavigation, Log, TEXT("Serialize called: %s, GenerationStrategy=%s"),
		Ar.IsSaving() ? TEXT("SAVING") : TEXT("LOADING"),
		GenerationParameters.GenerationStrategy == ESVOGenerationStrategy::UseBaked ? TEXT("UseBaked") : TEXT("GenerateOnBeginPlay"));

	if (GenerationParameters.GenerationStrategy == ESVOGenerationStrategy::UseBaked)
	{
		Ar << NavigationData.OctreeData;

		// When saving, always use the latest version format
		// When loading, check what version was saved to know what data is available
		if (Ar.IsSaving())
		{
			// Always save with latest format (includes Origin, Extents, and VoxelPower)
			// Get the actual parameters from NavigationData (where Generate() stored them)
			const FAeonixGenerationParameters& Params = NavigationData.GetParams();
			FVector OriginToSave = Params.Origin;
			FVector ExtentsToSave = Params.Extents;
			int32 VoxelPowerToSave = Params.VoxelPower;

			Ar << OriginToSave;
			Ar << ExtentsToSave;
			Ar << VoxelPowerToSave;
			UE_LOG(LogAeonixNavigation, Log, TEXT("Saving baked navigation data: %d leaf nodes, Origin=%s, Extents=%s, VoxelPower=%d"),
				NavigationData.OctreeData.LeafNodes.Num(),
				*OriginToSave.ToCompactString(),
				*ExtentsToSave.ToCompactString(),
				VoxelPowerToSave);
		}
		else if (Ar.IsLoading())
		{
			const int32 AeonixVersion = Ar.CustomVer(FAeonixBoundingVolumeVersion::GUID);

			if (AeonixVersion >= FAeonixBoundingVolumeVersion::SerializeVoxelPower)
			{
				// Version 2+: Load Origin, Extents, and VoxelPower
				FVector LoadedOrigin;
				FVector LoadedExtents;
				int32 LoadedVoxelPower;
				Ar << LoadedOrigin;
				Ar << LoadedExtents;
				Ar << LoadedVoxelPower;

				// Restore all parameters to NavigationData
				FAeonixGenerationParameters RestoredParams = GenerationParameters;
				RestoredParams.Origin = LoadedOrigin;
				RestoredParams.Extents = LoadedExtents;
				RestoredParams.VoxelPower = LoadedVoxelPower;
				NavigationData.UpdateGenerationParameters(RestoredParams);

				UE_LOG(LogAeonixNavigation, Log, TEXT("Baked navigation data loaded - %d leaf nodes with serialized bounds (Origin=%s, Extents=%s, VoxelPower=%d), marked ready for navigation"),
					NavigationData.OctreeData.LeafNodes.Num(),
					*LoadedOrigin.ToCompactString(),
					*LoadedExtents.ToCompactString(),
					LoadedVoxelPower);
			}
			else if (AeonixVersion >= FAeonixBoundingVolumeVersion::SerializeGenerationBounds)
			{
				// Version 1: Load Origin and Extents only (no VoxelPower)
				FVector LoadedOrigin;
				FVector LoadedExtents;
				Ar << LoadedOrigin;
				Ar << LoadedExtents;

				// Restore Origin/Extents, use VoxelPower from actor property
				FAeonixGenerationParameters RestoredParams = GenerationParameters;
				RestoredParams.Origin = LoadedOrigin;
				RestoredParams.Extents = LoadedExtents;
				// VoxelPower comes from GenerationParameters property
				NavigationData.UpdateGenerationParameters(RestoredParams);

				UE_LOG(LogAeonixNavigation, Warning, TEXT("Baked navigation data loaded from version 1 format - %d leaf nodes with Origin/Extents but missing VoxelPower. Using VoxelPower=%d from actor property. Please regenerate navigation data."),
					NavigationData.OctreeData.LeafNodes.Num(),
					RestoredParams.VoxelPower);
			}
			else
			{
				// Version 0: Old format - defer bounds calculation to BeginPlay when geometry is fully initialized
				bNeedsLegacyBoundsUpdate = true;
				UE_LOG(LogAeonixNavigation, Warning, TEXT("Baked navigation data loaded from old format - %d leaf nodes. Bounds will be recalculated in BeginPlay. Please regenerate navigation data to fix potential rendering issues."), NavigationData.OctreeData.LeafNodes.Num());
			}

			if (NavigationData.OctreeData.LeafNodes.Num() > 0)
			{
				bIsReadyForNavigation = true;
			}
		}
	}
}

void AAeonixBoundingVolume::BeginPlay()
{
	AeonixSubsystemInterface = GetWorld()->GetSubsystem<UAeonixSubsystem>();
	if (!AeonixSubsystemInterface.GetInterface())
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	}
	else
	{
		AeonixSubsystemInterface->RegisterVolume(this, EAeonixMassEntityFlag::Disabled);
	}

	CollisionQueryInterface = GetWorld()->GetSubsystem<UAeonixCollisionSubsystem>();
	if (!CollisionQueryInterface.GetInterface())
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid CollisionQueryInterface found"));
	}

	// Handle legacy baked data that needs bounds update (old format loaded in Serialize)
	if (bNeedsLegacyBoundsUpdate)
	{
		UpdateBounds();
		bNeedsLegacyBoundsUpdate = false;
		UE_LOG(LogAeonixNavigation, Log, TEXT("Legacy bounds update completed. Origin=%s, Extents=%s"),
			*NavigationData.GetParams().Origin.ToCompactString(),
			*NavigationData.GetParams().Extents.ToCompactString());
	}

	if (!bIsReadyForNavigation && GenerationParameters.GenerationStrategy == ESVOGenerationStrategy::GenerateOnBeginPlay)
	{
		Generate();
	}
	else if (!bIsReadyForNavigation)
	{
		// Only update bounds if we're not using baked data (which already has serialized bounds)
		UpdateBounds();
	}
	// If bIsReadyForNavigation is already true (from baked data), skip UpdateBounds()
	// to preserve the generation-time Origin and Extents that were serialized

	bIsReadyForNavigation = true;
}

void AAeonixBoundingVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (!AeonixSubsystemInterface.GetInterface())
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	}
	else
	{
		AeonixSubsystemInterface->UnRegisterVolume(this, EAeonixMassEntityFlag::Disabled);
	}

	Super::EndPlay(EndPlayReason);
}
