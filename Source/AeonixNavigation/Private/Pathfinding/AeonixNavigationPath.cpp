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
	for (int i = 0; i < myPoints.Num(); i++)
	{
		FAeonixPathPoint& point = myPoints[i];

		if (i < myPoints.Num() - 1)
		{
			FVector offSet(0.f);
			// if (i == 0)
			// offSet.Z -= 300.f;
			float Size = point.Layer == 0 ? Data.GetVoxelSize(point.Layer) * 0.25f : Data.GetVoxelSize(point.Layer) * 0.5f;

			DrawDebugBox(World, point.Position, FVector(Size), AeonixStatics::myLinkColors[point.Layer], true, -1.f, 0, 30.f);

			DrawDebugSphere(World, point.Position + offSet, 30.f, 20, FColor::Cyan, true, -1.f, 0, 100.f);

			// DrawDebugLine(aWorld, point.myPosition + offSet, myPoints[i+1].myPosition, FColor::Cyan, true, -1.f, 0, 100.f);
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
