#pragma once

#include "UObject/ObjectMacros.h"

#include "AeonixGenerationParameters.generated.h"

UENUM(BlueprintType)
enum class ESVOGenerationStrategy : uint8
{
	UseBaked UMETA(DisplayName = "Use Baked"),
	GenerateOnBeginPlay UMETA(DisplayName = "Generate OnBeginPlay")
};

USTRUCT(BlueprintType)
struct AEONIXNAVIGATION_API FAeonixGenerationParameters
{
	GENERATED_BODY()

	// Debug Parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation")
	float DebugDistance{5000.f};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation")
	bool ShowVoxels{false};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation")
	bool ShowLeafVoxels{false};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation")
	bool ShowMortonCodes{false};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation")
	bool ShowNeighbourLinks{false};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation")
	bool ShowParentChildLinks{false};

	// Generation parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation")
	int32 VoxelPower{3};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation")
	TEnumAsByte<ECollisionChannel> CollisionChannel{ECollisionChannel::ECC_MAX};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation")
	float AgentRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation")
	ESVOGenerationStrategy GenerationStrategy = ESVOGenerationStrategy::UseBaked;

	// Transient data used during generation
	FVector Origin{FVector::ZeroVector};
	FVector Extents{FVector::ZeroVector};
	FVector DebugPosition{FVector::ZeroVector};
	FBox DebugFilterBox{ForceInit};
	bool bUseDebugFilterBox{false};

	// Dynamic region support - voxels in these regions get pre-allocated leaf nodes for runtime updates
	// Key = unique GUID for each region, Value = bounding box
	TMap<FGuid, FBox> DynamicRegionBoxes;

	/** Add a dynamic region with a unique ID */
	void AddDynamicRegion(const FGuid& RegionId, const FBox& RegionBox)
	{
		DynamicRegionBoxes.Add(RegionId, RegionBox);
	}

	/** Remove a dynamic region by ID */
	void RemoveDynamicRegion(const FGuid& RegionId)
	{
		DynamicRegionBoxes.Remove(RegionId);
	}

	/** Get a dynamic region by ID (returns nullptr if not found) */
	const FBox* GetDynamicRegion(const FGuid& RegionId) const
	{
		return DynamicRegionBoxes.Find(RegionId);
	}

	/** Get all region IDs */
	TArray<FGuid> GetAllRegionIds() const
	{
		TArray<FGuid> RegionIds;
		DynamicRegionBoxes.GetKeys(RegionIds);
		return RegionIds;
	}
};