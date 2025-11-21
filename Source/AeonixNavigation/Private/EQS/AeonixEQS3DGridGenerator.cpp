// Copyright 2024 Chris Ashworth

#include "EQS/AeonixEQS3DGridGenerator.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "Subsystem/AeonixSubsystem.h"
#include "Actor/AeonixBoundingVolume.h"
#include "Util/AeonixMediator.h"
#include "Data/AeonixData.h"
#include "AeonixNavigation.h"

UAeonixEQS3DGridGenerator::UAeonixEQS3DGridGenerator()
{
	GenerateAround = UEnvQueryContext_Querier::StaticClass();
	GridRadius.DefaultValue = 1000.0f;
	GridSpacing.DefaultValue = 200.0f;
	bOnlyNavigablePoints = true;
	bProjectToNavigation = false;

	ItemType = UEnvQueryItemType_Point::StaticClass();
}

void UAeonixEQS3DGridGenerator::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	// Get origin from context
	TArray<FVector> ContextLocations;
	if (!QueryInstance.PrepareContext(GenerateAround, ContextLocations))
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("AeonixEQS3DGridGenerator: Failed to get context locations"));
		return;
	}

	if (ContextLocations.Num() == 0)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("AeonixEQS3DGridGenerator: No context locations provided"));
		return;
	}

	const FVector Origin = ContextLocations[0];

	// Get parameters
	const float Radius = GridRadius.GetValue();
	const float Spacing = GridSpacing.GetValue();

	if (Spacing <= 0.0f)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("AeonixEQS3DGridGenerator: Invalid spacing %.2f"), Spacing);
		return;
	}

	if (Radius <= 0.0f)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("AeonixEQS3DGridGenerator: Invalid radius %.2f"), Radius);
		return;
	}

	// Get Aeonix subsystem for navigation validation
	UWorld* World = QueryInstance.World;
	UAeonixSubsystem* AeonixSubsystem = nullptr;
	AAeonixBoundingVolume* NavVolume = nullptr;

	if (bOnlyNavigablePoints || bProjectToNavigation)
	{
		if (!World)
		{
			UE_LOG(LogAeonixNavigation, Warning, TEXT("AeonixEQS3DGridGenerator: No world context for navigation validation"));
			return;
		}

		AeonixSubsystem = World->GetSubsystem<UAeonixSubsystem>();
		if (!AeonixSubsystem)
		{
			UE_LOG(LogAeonixNavigation, Warning, TEXT("AeonixEQS3DGridGenerator: No AeonixSubsystem found"));
			return;
		}

		// Find the bounding volume containing the origin
		NavVolume = const_cast<AAeonixBoundingVolume*>(AeonixSubsystem->GetVolumeForPosition(Origin));
		if (!NavVolume)
		{
			UE_LOG(LogAeonixNavigation, Warning, TEXT("AeonixEQS3DGridGenerator: Origin not inside any navigation volume"));
			return;
		}
	}

	// Calculate grid bounds
	const float RadiusSquared = Radius * Radius;
	const int32 NumSteps = FMath::CeilToInt(2.0f * Radius / Spacing);

	// Pre-allocate space for worst-case point count (entire cube)
	// Actual count will be less due to spherical culling
	const int32 EstimatedPoints = NumSteps * NumSteps * NumSteps;
	TArray<FNavLocation> GeneratedPoints;
	GeneratedPoints.Reserve(EstimatedPoints / 2); // Sphere is ~52% of cube volume

	// Generate 3D grid with fixed spacing
	for (float X = -Radius; X <= Radius; X += Spacing)
	{
		for (float Y = -Radius; Y <= Radius; Y += Spacing)
		{
			for (float Z = -Radius; Z <= Radius; Z += Spacing)
			{
				const FVector Offset(X, Y, Z);
				const FVector TestPoint = Origin + Offset;

				// Spherical bounds culling
				if (Offset.SizeSquared() > RadiusSquared)
				{
					continue;
				}

				// Validate against navigation data if requested
				if (bOnlyNavigablePoints && NavVolume)
				{
					AeonixLink Link;
					if (!AeonixMediator::GetLinkFromPosition(TestPoint, *NavVolume, Link))
					{
						// Point is not in navigable space
						continue;
					}

					// Optionally project to navigation voxel center
					if (bProjectToNavigation)
					{
						FVector ProjectedPos;
						if (NavVolume->GetNavData().GetLinkPosition(Link, ProjectedPos))
						{
							GeneratedPoints.Add(FNavLocation(ProjectedPos));
						}
						else
						{
							// Fallback to original position if projection fails
							GeneratedPoints.Add(FNavLocation(TestPoint));
						}
					}
					else
					{
						GeneratedPoints.Add(FNavLocation(TestPoint));
					}
				}
				else
				{
					// No validation - add all grid points
					GeneratedPoints.Add(FNavLocation(TestPoint));
				}
			}
		}
	}

	// Add generated points to query instance
	for (const FNavLocation& NavLoc : GeneratedPoints)
	{
		QueryInstance.AddItemData<UEnvQueryItemType_Point>(NavLoc.Location);
	}

	UE_LOG(LogAeonixNavigation, Verbose, TEXT("AeonixEQS3DGridGenerator: Generated %d points (Origin: %s, Radius: %.1f, Spacing: %.1f)"),
		GeneratedPoints.Num(), *Origin.ToString(), Radius, Spacing);
}

FText UAeonixEQS3DGridGenerator::GetDescriptionTitle() const
{
	return FText::Format(
		NSLOCTEXT("EnvQueryGenerator", "Aeonix3DGridDescription", "Aeonix 3D Grid: radius {0}, spacing {1}"),
		FText::FromString(GridRadius.ToString()),
		FText::FromString(GridSpacing.ToString())
	);
}

FText UAeonixEQS3DGridGenerator::GetDescriptionDetails() const
{
	FText Details = NSLOCTEXT("EnvQueryGenerator", "Aeonix3DGridDetails",
		"Generates uniform 3D grid of points with fixed spacing, independent of voxel layout.");

	if (bOnlyNavigablePoints)
	{
		Details = FText::Format(
			NSLOCTEXT("EnvQueryGenerator", "Aeonix3DGridNavigableDetails", "{0}\nFiltered to navigable points only."),
			Details
		);
	}

	if (bProjectToNavigation)
	{
		Details = FText::Format(
			NSLOCTEXT("EnvQueryGenerator", "Aeonix3DGridProjectDetails", "{0}\nProjected to navigation voxel centers."),
			Details
		);
	}

	return Details;
}
