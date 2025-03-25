
#include <AeonixNavigation/Public/Pathfinding/AeonixPathFinder.h>

#include <AeonixNavigation/Public/AeonixNavigation.h>
#include <AeonixNavigation/Public/Data/AeonixData.h>
#include <AeonixNavigation/Public/Data/AeonixLink.h>
#include <AeonixNavigation/Public/Data/AeonixNode.h>
#include <AeonixNavigation/Public/Pathfinding/AeonixNavigationPath.h>

bool AeonixPathFinder::FindPath(const AeonixLink& Start, const AeonixLink& InGoal, const FVector& StartPos, const FVector& TargetPos, FAeonixNavigationPath& Path)
{
	OpenSet.Empty();
	ClosedSet.Empty();
	CameFrom.Empty();
	FScore.Empty();
	GScore.Empty();
	CurrentLink = AeonixLink();
	GoalLink = InGoal;
	StartLink = Start;

	OpenSet.Add(Start);
	CameFrom.Add(Start, Start);
	GScore.Add(Start, 0);
	FScore.Add(Start, HeuristicScore(Start, InGoal)); // Distance to target

	int numIterations = 0;

	while (OpenSet.Num() > 0)
	{

		float lowestScore = FLT_MAX;
		for (AeonixLink& link : OpenSet)
		{
			if (!FScore.Contains(link) || FScore[link] < lowestScore)
			{
				lowestScore = FScore[link];
				CurrentLink = link;
			}
		}

		OpenSet.Remove(CurrentLink);
		ClosedSet.Add(CurrentLink);

		if (CurrentLink == GoalLink)
		{
			BuildPath(CameFrom, CurrentLink, StartPos, TargetPos, Path);
			UE_LOG(AeonixNavigation, Display, TEXT("Pathfinding complete, iterations : %i"), numIterations);

			return true;
		}

		const AeonixNode& currentNode = NavigationData.OctreeData.GetNode(CurrentLink);

		TArray<AeonixLink> neighbours;

		if (CurrentLink.GetLayerIndex() == 0 && currentNode.FirstChild.IsValid())
		{
			NavigationData.OctreeData.GetLeafNeighbours(CurrentLink, neighbours);
		}
		else
		{
			NavigationData.OctreeData.GetNeighbours(CurrentLink, neighbours);
		}

		for (const AeonixLink& neighbour : neighbours)
		{
			ProcessLink(neighbour);
		}

		numIterations++;

		if (numIterations > Settings.MaxIterations)
		{
			UE_LOG(AeonixNavigation, Display, TEXT("Pathfinding aborted, hit iteration limit, iterations : %i"), numIterations);
			return false;
		}
	}

	UE_LOG(AeonixNavigation, Display, TEXT("Pathfinding failed, iterations : %i"), numIterations);
	return false;
}

float AeonixPathFinder::HeuristicScore(const AeonixLink& aStart, const AeonixLink& aTarget)
{
	float score = 0.f;

	FVector startPos, endPos;
	NavigationData.GetLinkPosition(aStart, startPos);
	NavigationData.GetLinkPosition(aTarget, endPos);
	switch (Settings.HeuristicType)
	{
	case EAeonixPathHeuristicType::MANHATTAN:
		score = FMath::Abs(endPos.X - startPos.X) + FMath::Abs(endPos.Y - startPos.Y) + FMath::Abs(endPos.Z - startPos.Z);
		break;
	case EAeonixPathHeuristicType::EUCLIDEAN:
	default:
		score = (startPos - endPos).Size();
		break;
	}

	// Layer size compensation, weights towards higher layers (larger voxels)
	score *= (1.0f - (static_cast<float>(aTarget.GetLayerIndex()) / static_cast<float>(NavigationData.OctreeData.GetNumLayers())) * Settings.NodeSizeHeuristic);

	return score;
}

float AeonixPathFinder::GetCost(const AeonixLink& aStart, const AeonixLink& aTarget)
{
	float cost = 0.f;

	// Unit cost implementation
	if (Settings.bUseUnitCost)
	{
		cost = Settings.UnitCost;
	}
	else
	{

		FVector startPos(0.f), endPos(0.f);
		const AeonixNode& startNode = NavigationData.OctreeData.GetNode(aStart);
		const AeonixNode& endNode = NavigationData.OctreeData.GetNode(aTarget);
		NavigationData.GetLinkPosition(aStart, startPos);
		NavigationData.GetLinkPosition(aTarget, endPos);
		cost = (startPos - endPos).Size();
	}

	cost *= (1.0f - (static_cast<float>(aTarget.GetLayerIndex()) / static_cast<float>(NavigationData.OctreeData.GetNumLayers())) * Settings.NodeSizeHeuristic);

	return cost;
}

void AeonixPathFinder::ProcessLink(const AeonixLink& aNeighbour)
{
	if (aNeighbour.IsValid())
	{
		if (ClosedSet.Contains(aNeighbour))
			return;

		if (!OpenSet.Contains(aNeighbour))
		{
			OpenSet.Add(aNeighbour);

			if (Settings.bDebugOpenNodes)
			{
				FVector pos;
				NavigationData.GetLinkPosition(aNeighbour, pos);
				Settings.DebugPoints.Add(pos);
			}
		}

		float t_gScore = FLT_MAX;
		if (GScore.Contains(CurrentLink))
			t_gScore = GScore[CurrentLink] + GetCost(CurrentLink, aNeighbour);
		else
			GScore.Add(CurrentLink, FLT_MAX);

		if (t_gScore >= (GScore.Contains(aNeighbour) ? GScore[aNeighbour] : FLT_MAX))
			return;

		CameFrom.Add(aNeighbour, CurrentLink);
		GScore.Add(aNeighbour, t_gScore);
		FScore.Add(aNeighbour, GScore[aNeighbour] + (Settings.HeuristicWeight * HeuristicScore(aNeighbour, GoalLink)));
	}
}

void AeonixPathFinder::BuildPath(TMap<AeonixLink, AeonixLink>& aCameFrom, AeonixLink aCurrent, const FVector& aStartPos, const FVector& aTargetPos, FAeonixNavigationPath& oPath)
{
	FAeonixPathPoint pos;

	TArray<FAeonixPathPoint> points;

	while (aCameFrom.Contains(aCurrent) && !(aCurrent == aCameFrom[aCurrent]))
	{
		aCurrent = aCameFrom[aCurrent];
		NavigationData.GetLinkPosition(aCurrent, pos.Position);
		points.Add(pos);
		const AeonixNode& node = NavigationData.OctreeData.GetNode(aCurrent);
		if (aCurrent.GetLayerIndex() == 0)
		{
			if (!node.HasChildren())
				points[points.Num() - 1].Layer = 1;
			else
				points[points.Num() - 1].Layer = 0;
		}
		else
		{
			points[points.Num() - 1].Layer = aCurrent.GetLayerIndex() + 1;
		}
	}

	if (points.Num() > 1)
	{
		points[0].Position = aTargetPos;
		points[points.Num() - 1].Position = aStartPos;
	}
	else // If start and end are in the same voxel, just use the start and target positions.
	{
		if (points.Num() == 0)
			points.Emplace();

		points[0].Position = aTargetPos;
		points.Emplace(aStartPos, StartLink.GetLayerIndex());
	}

	Smooth_Chaikin(points, Settings.SmoothingIterations);

#if WITH_EDITOR
	for (int i = points.Num() - 1; i >= 0; i--)
	{
		points[i].NodePosition = points[i].Position;
	}
#endif

	// For the intermediate type, for voxels on the same layer, we use the average of the two positions, this smooths out zigzags in diagonal paths.
	// a proper string pulling algorithm would do better, but this is quick and easy for now!
	if (Settings.PathPointType == EAeonixPathPointType::INTERMEDIATE)
	{
		for (int i = points.Num() - 1; i >= 0; i--)
		{
			if (i == 0 || i == points.Num() - 1)
			{
				continue;
			}
			
			if (points[i].Layer == points[i-1].Layer)
			{
				points[i].Position += (points[i-1].Position - points[i].Position) * 0.5f;
			}
		}
	}

	// This is a simple optimisation that removes redundant points, if the vector between the previous valid point and this point,
	// is within the tolerance angle of the vector from the previous valid point and the next point, we just mark it for culling
	if (Settings.bOptimizePath)
	{
		FAeonixPathPoint LastPoint = points[0];
		for (int i = 0; i < points.Num(); i++)
		{
			if ( i > 0 && i < points.Num() - 2)
			{
				FAeonixPathPoint& nextPoint = points[i + 1];
				FAeonixPathPoint& thisPoint = points[i];
				
				const double angle = FMath::RadiansToDegrees(acosf(FVector::DotProduct((thisPoint.Position - LastPoint.Position).GetSafeNormal(), (nextPoint.Position - LastPoint.Position).GetSafeNormal())));
				if (angle < ((Settings.OptimizeDotTolerance)))
				{
					thisPoint.bCullFlag = true;
				}
				else
				{
					LastPoint = thisPoint;
				}
			}
		}
	}
	
	// Now construct the final path, dropping the culled points.
	for (int i = points.Num() - 1; i >= 0; i--)
	{
		if (points[i].bCullFlag != true)
		{
			oPath.GetPathPoints().Add(points[i]);	
		}
	}
}

 void AeonixPathFinder::Smooth_Chaikin(TArray<FAeonixPathPoint>& somePoints, int aNumIterations)
{
	for (int i = 0; i < aNumIterations; i++)
	{
		for (int j = 0; j < somePoints.Num() - 1; j += 2)
		{
			FVector start = somePoints[j].Position;
			FVector end = somePoints[j + 1].Position;
			if (j > 0)
				somePoints[j].Position = FMath::Lerp(start, end, 0.25f);
			FVector secondVal = FMath::Lerp(start, end, 0.75f);
			somePoints.Insert(FAeonixPathPoint(secondVal, -2), j + 1);
		}
		somePoints.RemoveAt(somePoints.Num() - 1);
	}
 }
