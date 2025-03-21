
#include <AeonixNavigation/Public/Pathfinding/AeonixPathFinder.h>

#include <AeonixNavigation/Public/Data/AeonixLink.h>
#include <AeonixNavigation/Public/Data/AeonixNode.h>
#include <AeonixNavigation/Public/Pathfinding/AeonixNavigationPath.h>
#include <AeonixNavigation/Public/AeonixNavigation.h>
#include <AeonixNavigation/Public/Data/AeonixData.h>

int AeonixPathFinder::FindPath(const AeonixLink& InStart, const AeonixLink& InGoal, const FVector& StartPos, const FVector& TargetPos, FAeonixNavigationPath& Path)
{
	OpenSet.Empty();
	ClosedSet.Empty();
	CameFrom.Empty();
	FScore.Empty();
	GScore.Empty();
	Current = AeonixLink();
	Goal = InGoal;
	Start = InStart;

	OpenSet.Add(InStart);
	CameFrom.Add(InStart, InStart);
	GScore.Add(InStart, 0);
	FScore.Add(InStart, HeuristicScore(InStart, InGoal)); // Distance to target

	int numIterations = 0;

	while (OpenSet.Num() > 0)
	{

		float lowestScore = FLT_MAX;
		for (AeonixLink& link : OpenSet)
		{
			if (!FScore.Contains(link) || FScore[link] < lowestScore)
			{
				lowestScore = FScore[link];
				Current = link;
			}
		}

		OpenSet.Remove(Current);
		ClosedSet.Add(Current);

		if (Current == Goal)
		{
			BuildPath(CameFrom, Current, StartPos, TargetPos, Path);
			UE_LOG(AeonixNavigation, Display, TEXT("Pathfinding complete, iterations : %i"), numIterations);

			return 1;
		}

		const AeonixNode& currentNode = NavigationData.OctreeData.GetNode(Current);

		TArray<AeonixLink> neighbours;

		if (Current.GetLayerIndex() == 0 && currentNode.FirstChild.IsValid())
		{

			NavigationData.OctreeData.GetLeafNeighbours(Current, neighbours);
		}
		else
		{
			NavigationData.OctreeData.GetNeighbours(Current, neighbours);
		}

		for (const AeonixLink& neighbour : neighbours)
		{
			ProcessLink(neighbour);
		}

		numIterations++;

		if (numIterations > Settings.MaxIterations)
		{
			UE_LOG(AeonixNavigation, Display, TEXT("Pathfinding aborted, hit iteration limit, iterations : %i"), numIterations);
			return 0;
		}
	}

	UE_LOG(AeonixNavigation, Display, TEXT("Pathfinding failed, iterations : %i"), numIterations);
	return 0;
}

float AeonixPathFinder::HeuristicScore(const AeonixLink& aStart, const AeonixLink& aTarget)
{
	/* Just using manhattan distance for now */
	float score = 0.f;

	FVector startPos, endPos;
	NavigationData.GetLinkPosition(aStart, startPos);
	NavigationData.GetLinkPosition(aTarget, endPos);
	switch (Settings.PathCostType)
	{
	case EAeonixPathCostType::MANHATTAN:
		score = FMath::Abs(endPos.X - startPos.X) + FMath::Abs(endPos.Y - startPos.Y) + FMath::Abs(endPos.Z - startPos.Z);
		break;
	case EAeonixPathCostType::EUCLIDEAN:
	default:
		score = (startPos - endPos).Size();
		break;
	}

	score *= (1.0f - (static_cast<float>(aTarget.GetLayerIndex()) / static_cast<float>(NavigationData.OctreeData.GetNumLayers())) * Settings.NodeSizeCompensation);

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

	cost *= (1.0f - (static_cast<float>(aTarget.GetLayerIndex()) / static_cast<float>(NavigationData.OctreeData.GetNumLayers())) * Settings.NodeSizeCompensation);

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
		if (GScore.Contains(Current))
			t_gScore = GScore[Current] + GetCost(Current, aNeighbour);
		else
			GScore.Add(Current, FLT_MAX);

		if (t_gScore >= (GScore.Contains(aNeighbour) ? GScore[aNeighbour] : FLT_MAX))
			return;

		CameFrom.Add(aNeighbour, Current);
		GScore.Add(aNeighbour, t_gScore);
		FScore.Add(aNeighbour, GScore[aNeighbour] + (Settings.EstimateWeight * HeuristicScore(aNeighbour, Goal)));
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
		// This is rank. I really should sort the layers out
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
		points.Emplace(aStartPos, Start.GetLayerIndex());
	}

	Smooth_Chaikin(points, Settings.SmoothingIterations);

	for (int i = points.Num() - 1; i >= 0; i--)
	{
		oPath.GetPathPoints().Add(points[i]);
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
