

#include "Actor/AeonixBoundingVolume.h"
#include "Actor/AeonixModifierVolume.h"
#include "Subsystem/AeonixSubsystem.h"
#include "Subsystem/AeonixCollisionSubsystem.h"
#include "AeonixNavigation.h"
#include "Debug/AeonixDebugDrawManager.h"
#include "Data/AeonixAsyncRegen.h"
#include "Settings/AeonixSettings.h"
#include "Library/libmorton/morton.h"

#include "Components/BrushComponent.h"
#include "Components/LineBatchComponent.h"
#include "Engine/CollisionProfile.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "Serialization/CustomVersion.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"

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
		// Added serialization of DynamicRegionBoxes for persistent dynamic region registration
		SerializeDynamicRegions = 3,

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
	GetBrushComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
	UE_LOG(LogAeonixRegen, Display, TEXT("RegenerateDynamicSubregions called for bounding volume %s"), *GetName());

	// Use GenerationParameters which gets updated at runtime by AddDynamicRegion
	if (GenerationParameters.DynamicRegionBoxes.Num() == 0)
	{
		UE_LOG(LogAeonixRegen, Warning, TEXT("No dynamic regions registered for bounding volume %s. Add modifier volumes with DynamicRegion type."), *GetName());
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
	for (const auto& RegionPair : GenerationParameters.DynamicRegionBoxes)
	{
		const FBox& DynamicRegion = RegionPair.Value;
		DrawDebugBox(GetWorld(), DynamicRegion.GetCenter(), DynamicRegion.GetExtent(),
			FColor::Cyan, false, 5.0f, 0, 2.0f);
	}

	UE_LOG(LogAeonixRegen, Display, TEXT("RegenerateDynamicSubregions complete for bounding volume %s"), *GetName());

#if WITH_EDITOR
	// Mark actor as modified so Unreal saves the updated navigation data
	Modify();
	UE_LOG(LogAeonixRegen, Log, TEXT("Dynamic subregion changes marked for save"));
#endif

	// Broadcast that navigation has been regenerated
	OnNavigationRegenerated.Broadcast(this);
}

void AAeonixBoundingVolume::RegenerateDynamicSubregionsAsync()
{
	UE_LOG(LogAeonixRegen, Display, TEXT("RegenerateDynamicSubregionsAsync called for bounding volume %s"), *GetName());

	// Use GenerationParameters which gets updated at runtime by AddDynamicRegion
	if (GenerationParameters.DynamicRegionBoxes.Num() == 0)
	{
		UE_LOG(LogAeonixRegen, Warning, TEXT("No dynamic regions registered for bounding volume %s. Add modifier volumes with DynamicRegion type."), *GetName());
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

	// Prepare batch data on game thread
	FAeonixAsyncRegenBatch Batch;
	Batch.GenParams = GenerationParameters;
	Batch.VolumePtr = this;
	Batch.PhysicsScenePtr = GetWorld()->GetPhysicsScene();

	// Get chunk size from settings
	const UAeonixSettings* Settings = GetDefault<UAeonixSettings>();
	Batch.ChunkSize = Settings ? Settings->AsyncChunkSize : 75;

	// Calculate affected leaf nodes for all dynamic regions
	for (const auto& RegionPair : GenerationParameters.DynamicRegionBoxes)
	{
		const FBox& DynamicRegion = RegionPair.Value;
		const float VoxelSize = NavigationData.GetVoxelSize(0); // Layer 0 voxel size
		const int32 NodesPerSide = FMath::Pow(2.f, GenerationParameters.VoxelPower);
		const FVector VoxelOrigin = GenerationParameters.Origin - GenerationParameters.Extents;

		// Calculate Layer 0 voxel coordinate bounds that overlap with the dynamic region
		const FVector RegionMin = DynamicRegion.Min - VoxelOrigin;
		const FVector RegionMax = DynamicRegion.Max - VoxelOrigin;

		const int32 MinX = FMath::Max(0, FMath::FloorToInt(RegionMin.X / VoxelSize));
		const int32 MinY = FMath::Max(0, FMath::FloorToInt(RegionMin.Y / VoxelSize));
		const int32 MinZ = FMath::Max(0, FMath::FloorToInt(RegionMin.Z / VoxelSize));

		const int32 MaxX = FMath::Min(NodesPerSide - 1, FMath::CeilToInt(RegionMax.X / VoxelSize));
		const int32 MaxY = FMath::Min(NodesPerSide - 1, FMath::CeilToInt(RegionMax.Y / VoxelSize));
		const int32 MaxZ = FMath::Min(NodesPerSide - 1, FMath::CeilToInt(RegionMax.Z / VoxelSize));

		// Collect all affected leaf nodes
		TArray<AeonixNode>& Layer0 = NavigationData.OctreeData.GetLayer(0);
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				for (int32 Z = MinZ; Z <= MaxZ; ++Z)
				{
					mortoncode_t Code = morton3D_64_encode(X, Y, Z);

					// Find this node in Layer 0 and get its leaf index
					for (int32 NodeIdx = 0; NodeIdx < Layer0.Num(); ++NodeIdx)
					{
						if (Layer0[NodeIdx].Code == Code)
						{
							// Get the position of this Layer 0 node
							FVector NodePosition;
							NavigationData.GetNodePosition(0, Code, NodePosition);

							// Calculate leaf origin (corner of the node, not center)
							FVector LeafOrigin = NodePosition - FVector(VoxelSize * 0.5f);

							// Store data needed for async rasterization
							Batch.LeafIndicesToProcess.Add(NodeIdx); // Store array index instead of code for easy mapping
							Batch.LeafCoordinates.Add(FIntVector(X, Y, Z));
							Batch.LeafOrigins.Add(LeafOrigin);
							break;
						}
					}
				}
			}
		}
	}

	UE_LOG(LogAeonixRegen, Display, TEXT("RegenerateDynamicSubregionsAsync: Dispatching async task for %d leaves"),
		Batch.LeafIndicesToProcess.Num());

	// Dispatch async task to background thread
	FFunctionGraphTask::CreateAndDispatchWhenReady([Batch]()
	{
		AeonixAsyncRegen::ExecuteAsyncRegen(Batch);
	}, TStatId(), nullptr, ENamedThreads::AnyBackgroundThreadNormalTask);

	// Draw debug boxes showing which regions will be regenerated
	for (const auto& RegionPair : GenerationParameters.DynamicRegionBoxes)
	{
		DrawDebugBox(GetWorld(), RegionPair.Value.GetCenter(), RegionPair.Value.GetExtent(),
			FColor::Magenta, false, 5.0f, 0, 2.0f);
	}

	UE_LOG(LogAeonixRegen, Display, TEXT("RegenerateDynamicSubregionsAsync: Async task dispatched, returning to game thread"));
}

void AAeonixBoundingVolume::RegenerateDynamicSubregion(const FGuid& RegionId)
{
	UE_LOG(LogAeonixRegen, Display, TEXT("RegenerateDynamicSubregion called for region %s in volume %s"),
		*RegionId.ToString(), *GetName());

	if (!GenerationParameters.GetDynamicRegion(RegionId))
	{
		UE_LOG(LogAeonixRegen, Warning, TEXT("RegenerateDynamicSubregion: Region %s not found in volume %s"),
			*RegionId.ToString(), *GetName());
		return;
	}

	if (!CollisionQueryInterface.GetInterface())
	{
		UAeonixCollisionSubsystem* CollisionSubsystem = GetWorld()->GetSubsystem<UAeonixCollisionSubsystem>();
		CollisionQueryInterface = CollisionSubsystem;
	}

	// Clear octree debug visualization
	if (UAeonixDebugDrawManager* DebugManager = GetWorld()->GetSubsystem<UAeonixDebugDrawManager>())
	{
		DebugManager->Clear(EAeonixDebugCategory::Octree);
	}

	// Acquire write lock for thread-safe octree modification
	{
		FWriteScopeLock WriteLock(OctreeDataLock);
		TSet<FGuid> SingleRegion;
		SingleRegion.Add(RegionId);
		NavigationData.RegenerateDynamicSubregions(SingleRegion, *CollisionQueryInterface.GetInterface(), *this);
	}

#if WITH_EDITOR
	// Mark actor as modified so Unreal saves the updated navigation data
	Modify();
	UE_LOG(LogAeonixRegen, Log, TEXT("Dynamic subregion changes marked for save"));
#endif

	// Broadcast that navigation has been regenerated
	OnNavigationRegenerated.Broadcast(this);
}

void AAeonixBoundingVolume::RegenerateDynamicSubregionAsync(const FGuid& RegionId)
{
	TSet<FGuid> SingleRegion;
	SingleRegion.Add(RegionId);
	RegenerateDynamicSubregionsAsync(SingleRegion);
}

void AAeonixBoundingVolume::RegenerateDynamicSubregionsAsync(const TSet<FGuid>& RegionIds)
{
	UE_LOG(LogAeonixRegen, Display, TEXT("RegenerateDynamicSubregionsAsync called for %d specific region(s) in volume %s"),
		RegionIds.Num(), *GetName());

	if (RegionIds.Num() == 0)
	{
		UE_LOG(LogAeonixRegen, Warning, TEXT("RegenerateDynamicSubregionsAsync: No regions specified"));
		return;
	}

	// Use GenerationParameters which gets updated at runtime by AddDynamicRegion
	if (GenerationParameters.DynamicRegionBoxes.Num() == 0)
	{
		UE_LOG(LogAeonixRegen, Warning, TEXT("No dynamic regions registered for bounding volume %s"), *GetName());
		return;
	}

	if (!CollisionQueryInterface.GetInterface())
	{
		UAeonixCollisionSubsystem* CollisionSubsystem = GetWorld()->GetSubsystem<UAeonixCollisionSubsystem>();
		CollisionQueryInterface = CollisionSubsystem;
	}

	// Clear octree debug visualization
	if (UAeonixDebugDrawManager* DebugManager = GetWorld()->GetSubsystem<UAeonixDebugDrawManager>())
	{
		DebugManager->Clear(EAeonixDebugCategory::Octree);
	}

	// Prepare batch data on game thread
	FAeonixAsyncRegenBatch Batch;
	Batch.GenParams = GenerationParameters;
	Batch.VolumePtr = this;
	Batch.PhysicsScenePtr = GetWorld()->GetPhysicsScene();

	// Get chunk size from settings
	const UAeonixSettings* Settings = GetDefault<UAeonixSettings>();
	Batch.ChunkSize = Settings ? Settings->AsyncChunkSize : 75;
	Batch.RegionIdsToProcess = RegionIds; // Set the regions to process

	// Track which regions are being regenerated for path invalidation
	CurrentlyRegeneratingRegions = RegionIds;

	// Calculate affected leaf nodes for ONLY the specified regions
	for (const FGuid& RegionId : RegionIds)
	{
		const FBox* DynamicRegionPtr = GenerationParameters.GetDynamicRegion(RegionId);
		if (!DynamicRegionPtr)
		{
			UE_LOG(LogAeonixRegen, Warning, TEXT("Region ID %s not found in volume %s, skipping"),
				*RegionId.ToString(), *GetName());
			continue;
		}

		const FBox& DynamicRegion = *DynamicRegionPtr;
		const float VoxelSize = NavigationData.GetVoxelSize(0);
		const int32 NodesPerSide = FMath::Pow(2.f, GenerationParameters.VoxelPower);
		const FVector VoxelOrigin = GenerationParameters.Origin - GenerationParameters.Extents;

		// Calculate Layer 0 voxel coordinate bounds that overlap with this region
		const FVector RegionMin = DynamicRegion.Min - VoxelOrigin;
		const FVector RegionMax = DynamicRegion.Max - VoxelOrigin;

		const int32 MinX = FMath::Max(0, FMath::FloorToInt(RegionMin.X / VoxelSize));
		const int32 MinY = FMath::Max(0, FMath::FloorToInt(RegionMin.Y / VoxelSize));
		const int32 MinZ = FMath::Max(0, FMath::FloorToInt(RegionMin.Z / VoxelSize));

		const int32 MaxX = FMath::Min(NodesPerSide - 1, FMath::CeilToInt(RegionMax.X / VoxelSize));
		const int32 MaxY = FMath::Min(NodesPerSide - 1, FMath::CeilToInt(RegionMax.Y / VoxelSize));
		const int32 MaxZ = FMath::Min(NodesPerSide - 1, FMath::CeilToInt(RegionMax.Z / VoxelSize));

		// Collect all affected leaf nodes
		TArray<AeonixNode>& Layer0 = NavigationData.OctreeData.GetLayer(0);
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				for (int32 Z = MinZ; Z <= MaxZ; ++Z)
				{
					mortoncode_t Code = morton3D_64_encode(X, Y, Z);

					for (int32 NodeIdx = 0; NodeIdx < Layer0.Num(); ++NodeIdx)
					{
						if (Layer0[NodeIdx].Code == Code)
						{
							FVector NodePosition;
							NavigationData.GetNodePosition(0, Code, NodePosition);

							FVector LeafOrigin = NodePosition - FVector(VoxelSize * 0.5f);

							Batch.LeafIndicesToProcess.Add(NodeIdx);
							Batch.LeafCoordinates.Add(FIntVector(X, Y, Z));
							Batch.LeafOrigins.Add(LeafOrigin);
							break;
						}
					}
				}
			}
		}
	}

	UE_LOG(LogAeonixRegen, Display, TEXT("RegenerateDynamicSubregionsAsync: Dispatching async task for %d leaves across %d regions"),
		Batch.LeafIndicesToProcess.Num(), RegionIds.Num());

	// Dispatch async task to background thread
	FFunctionGraphTask::CreateAndDispatchWhenReady([Batch]()
	{
		AeonixAsyncRegen::ExecuteAsyncRegen(Batch);
	}, TStatId(), nullptr, ENamedThreads::AnyBackgroundThreadNormalTask);

	// Draw debug boxes for the specific regions being regenerated
	for (const FGuid& RegionId : RegionIds)
	{
		if (const FBox* RegionBox = GenerationParameters.GetDynamicRegion(RegionId))
		{
			DrawDebugBox(GetWorld(), RegionBox->GetCenter(), RegionBox->GetExtent(),
				FColor::Yellow, false, 5.0f, 0, 2.0f);
		}
	}

	UE_LOG(LogAeonixRegen, Display, TEXT("RegenerateDynamicSubregionsAsync: Selective async task dispatched for %d region(s)"),
		RegionIds.Num());
}

bool AAeonixBoundingVolume::HasData() const
{
	return NavigationData.OctreeData.LeafNodes.Num() > 0;
}

bool AAeonixBoundingVolume::IsPointInside(const FVector& Point) const
{
	const FBox Bounds = GetComponentsBoundingBox(true);
	return Bounds.IsInsideOrOn(Point);
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

void AAeonixBoundingVolume::AddDynamicRegion(const FGuid& RegionId, const FBox& RegionBox)
{
	// Check if region already exists
	const FBox* ExistingBox = GenerationParameters.GetDynamicRegion(RegionId);

	if (ExistingBox)
	{
		// Region already exists - check if bounds have changed
		if (!ExistingBox->Equals(RegionBox, 0.001f))
		{
			// Bounds changed - update it
			GenerationParameters.AddDynamicRegion(RegionId, RegionBox);
			UE_LOG(LogAeonixNavigation, Log, TEXT("Bounding volume %s updated dynamic region (ID: %s) with new box: %s (was: %s)"),
				*GetName(), *RegionId.ToString(), *RegionBox.ToString(), *ExistingBox->ToString());
		}
		else
		{
			// Same bounds - already registered (this is normal during level load)
			UE_LOG(LogAeonixNavigation, Verbose, TEXT("Bounding volume %s: dynamic region (ID: %s) already registered with same bounds"),
				*GetName(), *RegionId.ToString());
		}
	}
	else
	{
		// New region - add it
		GenerationParameters.AddDynamicRegion(RegionId, RegionBox);
		UE_LOG(LogAeonixNavigation, Log, TEXT("Bounding volume %s registered new dynamic region (ID: %s) box: %s"),
			*GetName(), *RegionId.ToString(), *RegionBox.ToString());
	}
}

void AAeonixBoundingVolume::RemoveDynamicRegion(const FGuid& RegionId)
{
	GenerationParameters.RemoveDynamicRegion(RegionId);
	UE_LOG(LogAeonixNavigation, Verbose, TEXT("Bounding volume %s removed dynamic region: %s"),
		*GetName(), *RegionId.ToString());
}

void AAeonixBoundingVolume::ClearDynamicRegions()
{
	GenerationParameters.DynamicRegionBoxes.Empty();
	DirtyRegionIds.Empty();
	DirtyRegionTimestamps.Empty();
	UE_LOG(LogAeonixNavigation, Verbose, TEXT("Bounding volume %s cleared dynamic regions and dirty state"), *GetName());
}

void AAeonixBoundingVolume::ValidateDynamicRegions()
{
	if (!GetWorld())
	{
		return;
	}

	// Track which loaded regions have been found in the level
	TSet<FGuid> FoundRegionIds;

	// Log what GUIDs are in the loaded DynamicRegionBoxes
	UE_LOG(LogAeonixNavigation, Log, TEXT("ValidateDynamicRegions: BoundingVolume %s has %d loaded regions:"),
		*GetName(), GenerationParameters.DynamicRegionBoxes.Num());
	for (const auto& RegionPair : GenerationParameters.DynamicRegionBoxes)
	{
		UE_LOG(LogAeonixNavigation, Log, TEXT("  - Loaded GUID: %s"), *RegionPair.Key.ToString());
	}

	// Log bounding volume info for spatial checks
	const FBox BoundingBox = GetComponentsBoundingBox(true);
	UE_LOG(LogAeonixNavigation, Log, TEXT("ValidateDynamicRegions: BoundingVolume %s at %s, Bounds: Min=%s Max=%s"),
		*GetName(), *GetActorLocation().ToString(), *BoundingBox.Min.ToString(), *BoundingBox.Max.ToString());

	// Iterate through all modifier volumes in the level
	for (TActorIterator<AAeonixModifierVolume> It(GetWorld()); It; ++It)
	{
		AAeonixModifierVolume* ModifierVolume = *It;
		if (!ModifierVolume)
		{
			continue;
		}

		// Check if this modifier has the DynamicRegion flag
		const bool bIsDynamicRegion = (ModifierVolume->ModifierTypes & static_cast<int32>(EAeonixModifierType::DynamicRegion)) != 0;

		// Log detailed info for spatial check
		const FVector ModifierLocation = ModifierVolume->GetActorLocation();
		const bool bIsInside = IsPointInside(ModifierLocation);

		UE_LOG(LogAeonixNavigation, Log, TEXT("  ModifierVolume %s: Location=%s, IsDynamicRegion=%d, IsPointInside=%d, GUID=%s"),
			*ModifierVolume->GetName(), *ModifierLocation.ToString(), bIsDynamicRegion, bIsInside, *ModifierVolume->DynamicRegionId.ToString());

		if (!bIsDynamicRegion)
		{
			continue;
		}

		// Check if this modifier is inside this bounding volume
		if (!bIsInside)
		{
			continue;
		}

		const FGuid& RegionId = ModifierVolume->DynamicRegionId;

		UE_LOG(LogAeonixNavigation, Log, TEXT("ValidateDynamicRegions: ModifierVolume %s has GUID %s"),
			*ModifierVolume->GetName(), *RegionId.ToString());

		// Check if this region exists in our loaded data
		if (GenerationParameters.DynamicRegionBoxes.Contains(RegionId))
		{
			FoundRegionIds.Add(RegionId);
			UE_LOG(LogAeonixNavigation, Verbose, TEXT("Validated dynamic region %s for modifier volume %s in bounding volume %s"),
				*RegionId.ToString(), *ModifierVolume->GetName(), *GetName());
		}
		else
		{
			UE_LOG(LogAeonixNavigation, Warning, TEXT("Modifier volume %s has dynamic region %s, but it was not found in loaded navigation data for bounding volume %s. The volume may have been added after the last generation. Consider regenerating navigation."),
				*ModifierVolume->GetName(), *RegionId.ToString(), *GetName());
		}
	}

	// Check for dynamic regions in loaded data that don't have corresponding modifier volumes
	for (const auto& RegionPair : GenerationParameters.DynamicRegionBoxes)
	{
		const FGuid& RegionId = RegionPair.Key;
		if (!FoundRegionIds.Contains(RegionId))
		{
			UE_LOG(LogAeonixNavigation, Warning, TEXT("Bounding volume %s has dynamic region %s in loaded navigation data, but no corresponding modifier volume was found in the level. The volume may have been deleted. Consider regenerating navigation or the dynamic region may not function correctly."),
				*GetName(), *RegionId.ToString());
		}
	}

	if (GenerationParameters.DynamicRegionBoxes.Num() > 0)
	{
		UE_LOG(LogAeonixNavigation, Log, TEXT("Dynamic region validation complete for bounding volume %s: %d loaded regions, %d matched with modifier volumes"),
			*GetName(), GenerationParameters.DynamicRegionBoxes.Num(), FoundRegionIds.Num());
	}
}


void AAeonixBoundingVolume::RequestDynamicRegionRegen(const FGuid& RegionId)
{
	if (!RegionId.IsValid())
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("RequestDynamicRegionRegen: Invalid region ID for volume %s"), *GetName());
		return;
	}

	// Only record timestamp if this is the first time marking this region dirty
	if (!DirtyRegionIds.Contains(RegionId))
	{
		double CurrentTime = GetWorld()->GetTimeSeconds();
		DirtyRegionTimestamps.Add(RegionId, CurrentTime);
		DirtyRegionIds.Add(RegionId);

		UE_LOG(LogAeonixRegen, Verbose, TEXT("Region %s marked dirty for volume %s (total dirty: %d)"),
			*RegionId.ToString(), *GetName(), DirtyRegionIds.Num());
	}
}

void AAeonixBoundingVolume::TryProcessDirtyRegions()
{
	if (DirtyRegionIds.Num() == 0)
		return;

	double CurrentTime = GetWorld()->GetTimeSeconds();

	// Get settings for delays and cooldown
	const UAeonixSettings* Settings = GetDefault<UAeonixSettings>();
	const float SettingsCooldown = Settings ? Settings->DynamicRegenCooldown : DynamicRegenCooldown;
	const float SettingsRuntimeDelay = Settings ? Settings->DirtyRegionProcessDelay : DirtyRegionProcessDelay;
	const float SettingsEditorDelay = Settings ? Settings->EditorDirtyRegionProcessDelay : EditorDirtyRegionProcessDelay;

	// Check cooldown (time since last regeneration)
	if (CurrentTime - LastDynamicRegenTime < SettingsCooldown)
		return; // Still in cooldown

	// Use appropriate delay based on whether we're in editor or runtime
	const float ProcessDelay = GetWorld()->IsGameWorld() ? SettingsRuntimeDelay : SettingsEditorDelay;

	// Find regions that have been dirty long enough (allows physics to settle)
	TSet<FGuid> RegionsToProcess;
	for (const FGuid& RegionId : DirtyRegionIds)
	{
		double* DirtyTime = DirtyRegionTimestamps.Find(RegionId);
		if (DirtyTime && (CurrentTime - *DirtyTime) >= ProcessDelay)
		{
			RegionsToProcess.Add(RegionId);
		}
	}

	// Only process if we have regions that have been dirty long enough
	if (RegionsToProcess.Num() == 0)
	{
		// Log how long until regions become eligible (helpful for debugging)
		if (DirtyRegionIds.Num() > 0)
		{
			double OldestDirtyTime = TNumericLimits<double>::Max();
			for (const FGuid& RegionId : DirtyRegionIds)
			{
				if (double* DirtyTime = DirtyRegionTimestamps.Find(RegionId))
				{
					OldestDirtyTime = FMath::Min(OldestDirtyTime, *DirtyTime);
				}
			}
			float TimeUntilEligible = ProcessDelay - (CurrentTime - OldestDirtyTime);
			UE_LOG(LogAeonixRegen, Verbose, TEXT("Volume %s: %d dirty region(s) not yet eligible (%.2fs remaining, delay=%.2fs)"),
				*GetName(), DirtyRegionIds.Num(), FMath::Max(0.0f, TimeUntilEligible), ProcessDelay);
		}
		return;
	}

	UE_LOG(LogAeonixRegen, Display, TEXT("Processing %d dirty region(s) for volume %s (total dirty: %d, delay used: %.2fs)"),
		RegionsToProcess.Num(), *GetName(), DirtyRegionIds.Num(), ProcessDelay);

	// In editor, use synchronous regeneration for immediate feedback
	// At runtime, use async to avoid hitches
	if (!GetWorld()->IsGameWorld())
	{
		// Synchronous regeneration (same as manual button click)
		for (const FGuid& RegionId : RegionsToProcess)
		{
			RegenerateDynamicSubregion(RegionId);
		}
	}
	else
	{
		// Async regeneration for runtime
		RegenerateDynamicSubregionsAsync(RegionsToProcess);
	}

	// Remove processed regions from dirty set
	for (const FGuid& RegionId : RegionsToProcess)
	{
		DirtyRegionIds.Remove(RegionId);
		DirtyRegionTimestamps.Remove(RegionId);
	}

	LastDynamicRegenTime = CurrentTime;
}

void AAeonixBoundingVolume::EnqueueRegenResults(TArray<FAeonixLeafRasterResult>&& Results, int32 TotalLeaves)
{
	// Store the new batch of results
	PendingRegenResults = MoveTemp(Results);
	NextResultIndexToProcess = 0;
	CurrentRegenTotalLeaves = TotalLeaves;

	UE_LOG(LogAeonixRegen, Display, TEXT("Enqueued %d regeneration results for time-budgeted processing"), PendingRegenResults.Num());
}

void AAeonixBoundingVolume::ProcessPendingRegenResults(float DeltaTime)
{
	if (PendingRegenResults.Num() == 0 || NextResultIndexToProcess >= PendingRegenResults.Num())
	{
		return;
	}

	// Get time budget from settings
	const UAeonixSettings* Settings = GetDefault<UAeonixSettings>();
	const float TimeBudgetMs = Settings ? Settings->DynamicRegenTimeBudgetMs : 5.0f;

	// Convert to seconds for timing
	const double TimeBudgetSeconds = TimeBudgetMs * 0.001;
	const double StartTime = FPlatformTime::Seconds();

	int32 ResultsProcessedThisFrame = 0;
	int32 NodesUpdated = 0;
	int32 SkippedNodes = 0;

	// Acquire write lock to update leaf nodes
	FWriteScopeLock WriteLock(OctreeDataLock);
	FAeonixOctreeData& OctreeData = NavigationData.OctreeData;

	// Process results until time budget is exhausted
	while (NextResultIndexToProcess < PendingRegenResults.Num())
	{
		const FAeonixLeafRasterResult& Result = PendingRegenResults[NextResultIndexToProcess];

		if (Result.LeafNodeArrayIndex >= 0 && Result.LeafNodeArrayIndex < OctreeData.LeafNodes.Num())
		{
			// Clear and set new voxel data
			OctreeData.LeafNodes[Result.LeafNodeArrayIndex].Clear();
			OctreeData.LeafNodes[Result.LeafNodeArrayIndex].VoxelGrid = Result.VoxelBitmask;
			NodesUpdated++;
		}
		else
		{
			UE_LOG(LogAeonixRegen, Warning, TEXT("ProcessPendingRegenResults: Invalid leaf node index %d (total nodes: %d)"),
				Result.LeafNodeArrayIndex, OctreeData.LeafNodes.Num());
			SkippedNodes++;
		}

		NextResultIndexToProcess++;
		ResultsProcessedThisFrame++;

		// Check if we've exceeded our time budget
		const double ElapsedTime = FPlatformTime::Seconds() - StartTime;
		if (ElapsedTime >= TimeBudgetSeconds)
		{
			UE_LOG(LogAeonixRegen, Verbose, TEXT("Time budget reached: Processed %d/%d results (%.2fms elapsed)"),
				NextResultIndexToProcess, PendingRegenResults.Num(), ElapsedTime * 1000.0);
			break;
		}
	}

	// Check if we've finished processing all results
	if (NextResultIndexToProcess >= PendingRegenResults.Num())
	{
		const double TotalTime = FPlatformTime::Seconds() - StartTime;
		UE_LOG(LogAeonixRegen, Display, TEXT("Dynamic regen complete: Updated %d/%d leaf nodes (%d skipped) in %.2fms"),
			NodesUpdated, CurrentRegenTotalLeaves, SkippedNodes, TotalTime * 1000.0);

		// Clear the queue
		PendingRegenResults.Empty();
		NextResultIndexToProcess = 0;
		CurrentRegenTotalLeaves = 0;

#if WITH_EDITOR
		// Mark actor as modified so Unreal saves the updated navigation data
		Modify();
		UE_LOG(LogAeonixRegen, Log, TEXT("Dynamic subregion changes marked for save"));
#endif

		// Invalidate paths that traverse the regenerated regions
		if (CurrentlyRegeneratingRegions.Num() > 0 && GetWorld())
		{
			if (UAeonixSubsystem* Subsystem = GetWorld()->GetSubsystem<UAeonixSubsystem>())
			{
				Subsystem->InvalidatePathsInRegions(CurrentlyRegeneratingRegions);
			}
		}

		// Fire completion delegate
		if (OnNavigationRegenerated.IsBound())
		{
			OnNavigationRegenerated.Broadcast(this);
		}

		// Clear the regenerating regions set
		CurrentlyRegeneratingRegions.Empty();
	}
	else if (ResultsProcessedThisFrame > 0)
	{
		UE_LOG(LogAeonixRegen, Verbose, TEXT("Processed %d results this frame (%d/%d total, %.1f%% complete)"),
			ResultsProcessedThisFrame, NextResultIndexToProcess, PendingRegenResults.Num(),
			(float)NextResultIndexToProcess / (float)PendingRegenResults.Num() * 100.0f);
	}
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
			// Always save with latest format (includes Origin, Extents, VoxelPower, and DynamicRegionBoxes)
			// Get the actual parameters from NavigationData (where Generate() stored them)
			const FAeonixGenerationParameters& Params = NavigationData.GetParams();
			FVector OriginToSave = Params.Origin;
			FVector ExtentsToSave = Params.Extents;
			int32 VoxelPowerToSave = Params.VoxelPower;
			TMap<FGuid, FBox> DynamicRegionsToSave = Params.DynamicRegionBoxes;

			Ar << OriginToSave;
			Ar << ExtentsToSave;
			Ar << VoxelPowerToSave;
			Ar << DynamicRegionsToSave;

			UE_LOG(LogAeonixNavigation, Log, TEXT("Saving baked navigation data: %d leaf nodes, Origin=%s, Extents=%s, VoxelPower=%d, DynamicRegions=%d"),
				NavigationData.OctreeData.LeafNodes.Num(),
				*OriginToSave.ToCompactString(),
				*ExtentsToSave.ToCompactString(),
				VoxelPowerToSave,
				DynamicRegionsToSave.Num());
		}
		else if (Ar.IsLoading())
		{
			const int32 AeonixVersion = Ar.CustomVer(FAeonixBoundingVolumeVersion::GUID);

			if (AeonixVersion >= FAeonixBoundingVolumeVersion::SerializeDynamicRegions)
			{
				// Version 3+: Load Origin, Extents, VoxelPower, and DynamicRegionBoxes
				FVector LoadedOrigin;
				FVector LoadedExtents;
				int32 LoadedVoxelPower;
				TMap<FGuid, FBox> LoadedDynamicRegions;
				Ar << LoadedOrigin;
				Ar << LoadedExtents;
				Ar << LoadedVoxelPower;
				Ar << LoadedDynamicRegions;

				// Restore all parameters to NavigationData and sync with actor's GenerationParameters
				FAeonixGenerationParameters RestoredParams = GenerationParameters;
				RestoredParams.Origin = LoadedOrigin;
				RestoredParams.Extents = LoadedExtents;
				RestoredParams.VoxelPower = LoadedVoxelPower;
				RestoredParams.DynamicRegionBoxes = LoadedDynamicRegions;
				NavigationData.UpdateGenerationParameters(RestoredParams);
				GenerationParameters.DynamicRegionBoxes = LoadedDynamicRegions;

				UE_LOG(LogAeonixNavigation, Log, TEXT("Baked navigation data loaded - %d leaf nodes with serialized bounds (Origin=%s, Extents=%s, VoxelPower=%d, DynamicRegions=%d), marked ready for navigation"),
					NavigationData.OctreeData.LeafNodes.Num(),
					*LoadedOrigin.ToCompactString(),
					*LoadedExtents.ToCompactString(),
					LoadedVoxelPower,
					LoadedDynamicRegions.Num());
			}
			else if (AeonixVersion >= FAeonixBoundingVolumeVersion::SerializeVoxelPower)
			{
				// Version 2: Load Origin, Extents, and VoxelPower (no DynamicRegionBoxes)
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

				UE_LOG(LogAeonixNavigation, Log, TEXT("Baked navigation data loaded from version 2 format - %d leaf nodes with serialized bounds (Origin=%s, Extents=%s, VoxelPower=%d), marked ready for navigation"),
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

	// Handle dynamic regions - validate loaded regions have corresponding modifier volumes
	if (GenerationParameters.DynamicRegionBoxes.Num() > 0)
	{
		// Validate that loaded regions still have corresponding modifier volumes
		// (GUIDs are now properly serialized, so no remapping needed)
		ValidateDynamicRegions();

		// Auto-regenerate dynamic regions on level load if we loaded baked data
		// This ensures dynamic regions have up-to-date collision data
		if (bIsReadyForNavigation)
		{
			UE_LOG(LogAeonixNavigation, Log, TEXT("Auto-regenerating %d dynamic region(s) after level load for bounding volume %s"),
				GenerationParameters.DynamicRegionBoxes.Num(), *GetName());
			RegenerateDynamicSubregionsAsync();
		}
	}

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
