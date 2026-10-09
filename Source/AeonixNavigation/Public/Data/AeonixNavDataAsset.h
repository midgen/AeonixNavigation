#pragma once

#include "Data/AeonixData.h"

#include "UObject/Object.h"

#include "AeonixNavDataAsset.generated.h"

/**
 *  Baked navigation data in its own asset, referenced by a UAeonixVolumeComponent.
 *  An asset rather than data serialized on the component, so a Blueprint's component template
 *  and every instance spawned from it share one copy.
 */
UCLASS(BlueprintType)
class AEONIXNAVIGATION_API UAeonixNavDataAsset : public UObject
{
	GENERATED_BODY()

public:
	//~ Begin UObject
	void Serialize(FArchive& Ar) override;
	//~ End UObject

	/** Copy generated navigation data into the asset */
	void Store(const FAeonixData& Source);

	/** Copy the baked octree into a volume's navigation data, keeping that volume's non-baked parameters */
	void Restore(FAeonixData& Target, const FAeonixGenerationParameters& VolumeParameters) const;

	bool HasData() const { return NavData.OctreeData.LeafNodes.Num() > 0; }
	const FAeonixData& GetNavData() const { return NavData; }

	UPROPERTY(VisibleAnywhere, Category = "Aeonix")
	int32 OctreeDepth = 0;

	/** Half-extents of the generated volume, in nav space */
	UPROPERTY(VisibleAnywhere, Category = "Aeonix")
	FVector Extents = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Aeonix")
	int32 NumLeafNodes = 0;

	UPROPERTY(VisibleAnywhere, Category = "Aeonix")
	int32 NumBytes = 0;

private:
	FAeonixData NavData;
};
