
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

	// Initial path building from the A* results
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

#if WITH_EDITOR
	// Store the original path for debug visualization before any optimizations
	TArray<FDebugVoxelInfo> debugVoxelInfo;
	debugVoxelInfo.Reserve(points.Num() + 2); // Reserve space for potential start/end additions
	

	//debugVoxelInfo.Add(FDebugVoxelInfo(aTargetPos, points.Num() > 0 ? points[0].Layer : 0));
	
	// Add intermediate points
	for (const FAeonixPathPoint& point : points)
	{
		debugVoxelInfo.Add(FDebugVoxelInfo(point.Position, point.Layer));
	}

	// Add target position (beginning of path) with its layer
	//FVector StartVoxelPos, TargetVoxelPos;
	NavigationData.GetLinkPosition(GoalLink, debugVoxelInfo[0].Position);
	NavigationData.GetLinkPosition(StartLink, debugVoxelInfo[debugVoxelInfo.Num() - 1].Position);
	
	// Add start position (end of path) with its layer
	//debugVoxelInfo.Add(FDebugVoxelInfo(StartVoxelPos, points.Num() > 1 ? points[points.Num()-1].Layer : StartLink.GetLayerIndex()));
	
	oPath.SetDebugVoxelInfo(debugVoxelInfo);
#endif

	Smooth_Chaikin(points, Settings.SmoothingIterations);

	// Apply string pulling if enabled
	if (Settings.bUseStringPulling)
	{
		StringPullPath(points);
	}

	// Smooth the path by adjusting positions within voxel bounds
	if (Settings.bSmoothPositions)
	{
		SmoothPathPositions(points);
	}

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

void AeonixPathFinder::StringPullPath(TArray<FAeonixPathPoint>& pathPoints)
{
	if (pathPoints.Num() < 3)
	{
		// Not enough points to string-pull
		return;
	}

	// First, make sure no points are marked for culling initially
	for (FAeonixPathPoint& point : pathPoints)
	{
		point.bCullFlag = false;
	}

	// Create an array to mark points that should be kept (not culled)
	TArray<bool, TInlineAllocator<32>> keepPoint;
	keepPoint.SetNum(pathPoints.Num());
	
	// Always keep first and last points
	keepPoint[0] = true;
	keepPoint[pathPoints.Num() - 1] = true;
	
	// Current apex point for pathfinding
	int32 apexIdx = 0;
	FVector apexPos = pathPoints[apexIdx].Position;
	
	// Process all points starting from the apex
	while (apexIdx < pathPoints.Num() - 1)
	{
		// Try to find the furthest visible point from current apex
		int32 furthestVisible = -1;
		
		// Test points starting from the furthest possible
		for (int32 testIdx = pathPoints.Num() - 1; testIdx > apexIdx; testIdx--)
		{
			// Check if we can create a valid corridor between these points
			bool canConnect = true;
			
			// Test if any intermediate nodes need to be kept due to obstacles
			for (int32 intermediateIdx = apexIdx + 1; intermediateIdx < testIdx; intermediateIdx++)
			{
				FVector intermediatePos = pathPoints[intermediateIdx].Position;
				
				// Calculate perpendicular distance from intermediate point to the direct line
				FVector apexToTest = pathPoints[testIdx].Position - apexPos;
				apexToTest.Normalize();
				
				FVector apexToIntermediate = intermediatePos - apexPos;
				float alongLine = FVector::DotProduct(apexToIntermediate, apexToTest);
				FVector projectedPoint = apexPos + apexToTest * alongLine;
				
				float perpDistance = FVector::Dist(intermediatePos, projectedPoint);
				
				// Get voxel size for the intermediate point's layer
				float voxelSize = NavigationData.GetVoxelSize(pathPoints[intermediateIdx].Layer);
				
				// If point is too far from the direct line (more than threshold * voxel size),
				// we need to keep intermediate points
				if (perpDistance > voxelSize * Settings.StringPullingVoxelThreshold)
				{
					canConnect = false;
					break;
				}
			}
			
			if (canConnect)
			{
				furthestVisible = testIdx;
				break;
			}
		}
		
		// If we found a valid connection point
		if (furthestVisible != -1)
		{
			// Mark this furthest visible point to keep
			keepPoint[furthestVisible] = true;
			
			// Mark intermediate points for culling
			for (int32 i = apexIdx + 1; i < furthestVisible; i++)
			{
				// Don't mark points for culling if they're at a different layer
				if (pathPoints[i].Layer == pathPoints[apexIdx].Layer && 
				    pathPoints[i].Layer == pathPoints[furthestVisible].Layer)
				{
					pathPoints[i].bCullFlag = true;
				}
				else
				{
					// Keep points at layer transitions
					keepPoint[i] = true;
				}
			}
			
			// Move apex to the furthest visible point
			apexIdx = furthestVisible;
			apexPos = pathPoints[apexIdx].Position;
		}
		else
		{
			// If we couldn't find a valid connection, keep the next point and move apex forward
			keepPoint[apexIdx + 1] = true;
			apexIdx++;
			apexPos = pathPoints[apexIdx].Position;
		}
	}
	
	// Set culling flags based on our analysis
	for (int32 i = 0; i < pathPoints.Num(); i++)
	{
		if (!keepPoint[i])
		{
			pathPoints[i].bCullFlag = true;
		}
	}
	
	// Debug output
	UE_LOG(AeonixNavigation, Log, TEXT("String pulling: Original points: %d, Kept points: %d"), 
	       pathPoints.Num(), 
	       pathPoints.Num() - pathPoints.FilterByPredicate([](const FAeonixPathPoint& Point){ return Point.bCullFlag; }).Num());
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

void AeonixPathFinder::SmoothPathPositions(TArray<FAeonixPathPoint>& pathPoints)
{
	// Need at least 3 points to perform smoothing
	if (pathPoints.Num() < 3)
	{
		return;
	}
	
	// Note: We don't need to store original positions for debug here
	// because they were already stored when building the path
	
	// Create a copy of the original positions for reference
	TArray<FVector> originalPositions;
	for (const FAeonixPathPoint& point : pathPoints)
	{
		if (!point.bCullFlag)
		{
			originalPositions.Add(point.Position);
		}
	}
	
	// Skip if we don't have enough valid points after culling
	if (originalPositions.Num() < 3)
	{
		return;
	}

	// Get a filtered array of points that aren't being culled
	TArray<FAeonixPathPoint*> validPoints;
	for (int32 i = 0; i < pathPoints.Num(); i++)
	{
		if (!pathPoints[i].bCullFlag)
		{
			validPoints.Add(&pathPoints[i]);
		}
	}
	
	// Don't modify the first and last points
	for (int32 i = 1; i < validPoints.Num() - 1; i++)
	{
		FAeonixPathPoint* prevPoint = validPoints[i-1];
		FAeonixPathPoint* currentPoint = validPoints[i];
		FAeonixPathPoint* nextPoint = validPoints[i+1];
		
		// Get the voxel size for the current point's layer
		float halfVoxelSize = currentPoint->Layer == 0 ? 
	NavigationData.GetVoxelSize(currentPoint->Layer) * 0.125f : 
	NavigationData.GetVoxelSize(currentPoint->Layer) * 0.25f;
		//float voxelSize = NavigationData.GetVoxelSize(currentPoint->Layer);
		//float halfVoxelSize = voxelSize * 0.125f;
		
		// Create a vector pointing from previous to next point
		FVector direction = (nextPoint->Position - prevPoint->Position).GetSafeNormal();
		
		// Project the current point onto the line between prev and next
		FVector prevToCurrentVec = currentPoint->Position - prevPoint->Position;
		float dot = FVector::DotProduct(prevToCurrentVec, direction);
		FVector projectedPoint = prevPoint->Position + direction * dot;
		
		// Calculate how far the projected point is from the current position
		float distanceToProjected = FVector::Dist(currentPoint->Position, projectedPoint);
		
		// Ensure we never move more than half the voxel size
		float maxMoveDistance = halfVoxelSize;
		float actualMoveDistance = FMath::Min(distanceToProjected * Settings.SmoothingFactor, maxMoveDistance);
		
		// If we need to move, calculate the direction and apply the limited movement
		if (distanceToProjected > KINDA_SMALL_NUMBER)
		{
			FVector moveDirection = (projectedPoint - currentPoint->Position).GetSafeNormal();
			FVector newPosition = currentPoint->Position + (moveDirection * actualMoveDistance);
			
			// Apply the adjusted position
			currentPoint->Position = newPosition;
		}
	}
}
