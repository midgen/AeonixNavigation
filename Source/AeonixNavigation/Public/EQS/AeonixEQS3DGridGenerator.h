// Copyright 2024 Chris Ashworth

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "DataProviders/AIDataProvider.h"
#include "AeonixEQS3DGridGenerator.generated.h"

/**
 * Generates a uniform 3D grid of test points within a spherical radius
 * with fixed spacing between points, independent of the Aeonix voxel layout.
 *
 * This generator provides predictable, uniform point distribution for large levels
 * where the flood fill generator's voxel-dependent spacing is inefficient.
 *
 * Performance: O((2*Radius/Spacing)³) - depends only on spacing, not octree complexity
 */
UCLASS(EditInlineNew, Category = "Aeonix|EQS")
class AEONIXNAVIGATION_API UAeonixEQS3DGridGenerator : public UEnvQueryGenerator
{
	GENERATED_BODY()

public:
	UAeonixEQS3DGridGenerator();

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

public:
	/** Maximum distance from the origin to generate points (spherical bounds) */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	FAIDataProviderFloatValue GridRadius;

	/** Fixed distance between grid points in all axes */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	FAIDataProviderFloatValue GridSpacing;

	/** If true, only generate points that are in navigable space according to Aeonix navigation data */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	uint8 bOnlyNavigablePoints : 1;

	/** If true, snap generated points to the nearest Aeonix voxel center */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	uint8 bProjectToNavigation : 1;

	/** Context to use as the center point for grid generation */
	UPROPERTY(EditAnywhere, Category = Generator)
	TSubclassOf<UEnvQueryContext> GenerateAround;
};
