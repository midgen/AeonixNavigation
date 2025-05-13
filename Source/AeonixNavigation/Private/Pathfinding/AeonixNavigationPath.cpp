#include <AeonixNavigation/Public/Pathfinding/AeonixNavigationPath.h>

#include <Runtime/Engine/Classes/Debug/DebugDrawService.h>
#include <Runtime/Engine/Public/DrawDebugHelpers.h>
#include <Runtime/NavigationSystem/Public/NavigationData.h>
#include <AeonixNavigation/Public/Actor/AeonixBoundingVolume.h>
#include <AeonixNavigation/Public/AeonixNavigation.h>

void FAeonixNavigationPath::AddPoint(const FAeonixPathPoint& aPoint)
{
	myPoints.Add(aPoint);
}

void FAeonixNavigationPath::ResetForRepath()
{
	myPoints.Empty();
}

void FAeonixNavigationPath::DebugDraw(UWorld* World, const FAeonixData& Data)
{
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
		DrawDebugSphere(World, point.Position, 30.f, 20, PointColor, true, 0.1f, 0, 2.f);
		
		// Draw lines connecting path points
		if (i < myPoints.Num() - 1)
		{
			DrawDebugLine(World, point.Position, myPoints[i+1].Position, FColor::Cyan, true, 0.1f, 0, 10.f);	
		}
	}
	
#if WITH_EDITOR
	// Draw the original voxel positions from the debug array
	if (myDebugVoxelInfo.Num() > 0)
	{
		for (int i = 0; i < myDebugVoxelInfo.Num(); i++)
		{
			const FDebugVoxelInfo& voxelInfo = myDebugVoxelInfo[i];
			
			// Determine color based on position in the array
			FColor boxColor;
			if (i == 0)  // First voxel (target position)
			{
				boxColor = FColor::Green;
			}
			else if (i == myDebugVoxelInfo.Num() - 1)  // Last voxel (start position)
			{
				boxColor = FColor::Red;
			}
			else  // Intermediate voxels
			{
				boxColor = voxelInfo.Layer > 0 ? AeonixStatics::myLinkColors[voxelInfo.Layer] : FColor::Red;
			}
			
			float size = voxelInfo.Layer == 0 ? 
				Data.GetVoxelSize(voxelInfo.Layer) * 0.125f : 
				Data.GetVoxelSize(voxelInfo.Layer) * 0.25f;
				
			// Draw a box representing the original voxel
			DrawDebugBox(
				World, 
				voxelInfo.Position, 
				FVector(size), 
				boxColor, 
				true, 0.1f, 0, 2.f
			);
		}
	}
#endif
}

void FAeonixNavigationPath::CreateNavPath(FNavigationPath& aOutPath)
{
	for (const FAeonixPathPoint& point : myPoints)
	{
		aOutPath.GetPathPoints().Add(point.Position);
	}
}
