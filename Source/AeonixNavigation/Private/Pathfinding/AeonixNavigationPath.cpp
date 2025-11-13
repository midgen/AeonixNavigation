#include "Pathfinding/AeonixNavigationPath.h"

#include "Debug/DebugDrawService.h"
#include "Debug/AeonixDebugDrawManager.h"
#include "DrawDebugHelpers.h"
#include "NavigationData.h"
#include "Actor/AeonixBoundingVolume.h"
#include "AeonixNavigation.h"

void FAeonixNavigationPath::AddPoint(const FAeonixPathPoint& aPoint)
{
	myPoints.Add(aPoint);
}

void FAeonixNavigationPath::ResetForRepath()
{
	UE_LOG(LogTemp, Warning, TEXT("AeonixNavigationPath: ResetForRepath called, clearing %d points"), myPoints.Num());
	myPoints.Empty();
	myIsReady = false;
}

void FAeonixNavigationPath::DebugDraw(UWorld* World, const FAeonixData& Data)
{
	UAeonixDebugDrawManager* DebugManager = World->GetSubsystem<UAeonixDebugDrawManager>();
	if (!DebugManager)
	{
		return;
	}

	// Draw the final optimized path with blue spheres and connecting lines
	for (int i = 0; i < myPoints.Num(); i++)
	{
		FAeonixPathPoint& point = myPoints[i];

		FColor PointColor = FColor::Blue;
		if (i == 0)
		{
			PointColor = FColor::Green;
		}
		else if (i == myPoints.Num() - 1)
		{
			PointColor = FColor::Red;
		}

		// Draw sphere at the actual path point position (which may be smoothed)
		DebugManager->AddSphere(point.Position, 30.f, 20, PointColor, EAeonixDebugCategory::Paths);

		// Draw lines connecting path points
		if (i < myPoints.Num() - 1)
		{
			DebugManager->AddLine(point.Position, myPoints[i+1].Position, FColor::Cyan, 10.f, EAeonixDebugCategory::Paths);
		}
	}
	
#if WITH_EDITOR
	// Draw the original voxel positions from the debug array
	if (myDebugVoxelInfo.Num() > 0)
	{
		// Cache the array size to avoid issues if array changes during iteration
		const int32 NumVoxels = myDebugVoxelInfo.Num();

		for (int32 i = 0; i < NumVoxels; i++)
		{
			// Bounds check to prevent crashes
			if (!myDebugVoxelInfo.IsValidIndex(i))
			{
				UE_LOG(LogAeonixNavigation, Warning, TEXT("DebugDraw: Invalid index %d for debug voxel info array of size %d"), i, myDebugVoxelInfo.Num());
				break;
			}

			const FDebugVoxelInfo& voxelInfo = myDebugVoxelInfo[i];

			// Determine color based on position in the array
			FColor boxColor;
			if (i == 0)  // First voxel (target position)
			{
				boxColor = FColor::Yellow;
			}
			else if (i == NumVoxels - 1)  // Last voxel (start position)
			{
				boxColor = FColor::Green;
			}
			else  // Intermediate voxels
			{
				// Validate layer index before using it (myLinkColors has 8 elements)
				if (voxelInfo.Layer >= 0 && voxelInfo.Layer < 8)
				{
					boxColor = voxelInfo.Layer > 0 ? AeonixStatics::myLinkColors[voxelInfo.Layer] : FColor::Red;
				}
				else
				{
					boxColor = FColor::Red; // Default to red for invalid layers
				}
			}

			// Calculate size with bounds checking on layer
			float size = 50.0f; // Default size
			if (voxelInfo.Layer >= 0)
			{
				size = voxelInfo.Layer == 0 ?
					Data.GetVoxelSize(voxelInfo.Layer) * 0.125f :
					Data.GetVoxelSize(voxelInfo.Layer) * 0.25f;
			}

			// Draw a box representing the original voxel
			DebugManager->AddBox(voxelInfo.Position, FVector(size), FQuat::Identity, boxColor, EAeonixDebugCategory::Paths);
		}
	}
#endif
}

void FAeonixNavigationPath::DebugDrawLite(UWorld* World, const FColor& LineColor, float LifeTime) const
{
	if (!World || myPoints.Num() < 2)
	{
		return;
	}

	UAeonixDebugDrawManager* DebugManager = World->GetSubsystem<UAeonixDebugDrawManager>();
	if (!DebugManager)
	{
		return;
	}

	// Draw simple lines connecting all path points
	for (int32 i = 0; i < myPoints.Num() - 1; i++)
	{
		DebugManager->AddLine(myPoints[i].Position, myPoints[i + 1].Position, LineColor, 2.0f, EAeonixDebugCategory::Paths);
	}
}

void FAeonixNavigationPath::CreateNavPath(FNavigationPath& aOutPath)
{
	for (const FAeonixPathPoint& point : myPoints)
	{
		aOutPath.GetPathPoints().Add(point.Position);
	}
}
