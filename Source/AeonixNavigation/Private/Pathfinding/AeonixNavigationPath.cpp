#include <AeonixNavigation/Public/Pathfinding/AeonixNavigationPath.h>

#include <Runtime/Engine/Classes/Debug/DebugDrawService.h>
#include <Runtime/Engine/Public/DrawDebugHelpers.h>
#include <Runtime/NavigationSystem/Public/NavigationData.h>
#include <AeonixNavigation/Public/Actor/AeonixBoundingVolume.h>

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
	FlushPersistentDebugLines(World);
	for (int i = 0; i < myPoints.Num(); i++)
	{
		FAeonixPathPoint& point = myPoints[i];
		
		float Size = point.Layer == 0 ? Data.GetVoxelSize(point.Layer) * 0.125f : Data.GetVoxelSize(point.Layer) * 0.25f;

		FColor PointColor = FColor::Blue;
		if (i == 0)
		{
			PointColor = FColor::Green;
		}
		else if (i == myPoints.Num() - 1)
		{
			PointColor = FColor::Red;
		}
		DrawDebugSphere(World, point.Position, 30.f, 20, PointColor, true, -1.f, 0, 2.f);
		if (i < myPoints.Num() - 1)
		{
#if WITH_EDITOR
			if (i > 0)
			{
				DrawDebugBox(World, point.NodePosition, FVector(Size), point.Layer > 0 ? AeonixStatics::myLinkColors[point.Layer] : FColor::Red, true, -1.f, 0, 2.f);	
			}
#endif
			DrawDebugLine(World, point.Position, myPoints[i+1].Position, FColor::Cyan, true, -1.f, 0, 10.f);	
		}
		
	}
}

void FAeonixNavigationPath::CreateNavPath(FNavigationPath& aOutPath)
{
	for (const FAeonixPathPoint& point : myPoints)
	{
		aOutPath.GetPathPoints().Add(point.Position);
	}
}
