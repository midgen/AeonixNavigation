
#include "Pathfinding/AeonixPathFinder.h"

#include "AeonixNavigation.h"
#include "Data/AeonixData.h"
#include "Data/AeonixLeafNode.h"
#include "Data/AeonixLink.h"
#include "Data/AeonixNode.h"
#include "Data/AeonixStats.h"
#include "Pathfinding/AeonixNavigationPath.h"

bool AeonixPathFinder::FindPath(const AeonixLink& Start, const AeonixLink& InGoal, const FVector& StartPos, const FVector& TargetPos, FAeonixNavigationPath& Path, FAeonixPathFailureInfo* OutFailureInfo)
{
	OpenHeap.Empty();
	OpenSetLookup.Empty();
	ClosedSet.Empty();
	CameFrom.Empty();
	FScore.Empty();
	GScore.Empty();
	CurrentLink = AeonixLink();
	GoalLink = InGoal;
	StartLink = Start;

	CameFrom.Add(Start, Start);
	GScore.Add(Start, 0);
	FScore.Add(Start, CalculateHeuristic(Start, InGoal)); // Distance to target

	// Add start to open set using heap
	OpenHeap.Add(Start);
	OpenSetLookup.Add(Start);

	int numIterations = 0;
	FScoreHeapPredicate HeapPredicate(FScore);

	// Diagnostic tracking for iteration explosion debugging
	TSet<AeonixLink> UniqueNodesProcessed;
	int32 DuplicatePopCount = 0;
	int32 TotalNeighborsGenerated = 0;
	int32 MaxNeighborsInSingleIteration = 0;
	int32 EmptyLeafNeighbourCount = 0;
	int32 NonEmptyLeafNeighbourCount = 0;
	int32 HigherLayerNeighbourCount = 0;

	while (OpenHeap.Num() > 0)
	{
		// Pop the node with lowest FScore from the heap - O(log n) instead of O(n)
		OpenHeap.HeapPop(CurrentLink, HeapPredicate);
		OpenSetLookup.Remove(CurrentLink);

		// Skip if already processed (can happen with lazy deletion approach)
		if (ClosedSet.Contains(CurrentLink))
		{
			DuplicatePopCount++;
			continue;
		}

		// Track unique nodes processed
		UniqueNodesProcessed.Add(CurrentLink);
		ClosedSet.Add(CurrentLink);

		if (CurrentLink == GoalLink)
		{
			BuildPath(CameFrom, CurrentLink, StartPos, TargetPos, Path);
			UE_LOG(LogAeonixNavigation, Display, TEXT("Pathfinding complete, iterations : %i"), numIterations);

			LastIterationCount = numIterations;
			return true;
		}

		const AeonixNode& currentNode = NavigationData.OctreeData.GetNode(CurrentLink);

		TArray<AeonixLink> neighbours;

		if (CurrentLink.GetLayerIndex() == 0 && currentNode.FirstChild.IsValid())
		{
			// Layer 0 node with leaf subdivision - use GetLeafNeighbours
			// This returns ~6 neighbors (one per direction) instead of up to 96
			// The previous "empty leaf optimization" was causing neighbor explosion
			NavigationData.OctreeData.GetLeafNeighbours(CurrentLink, neighbours);
			NonEmptyLeafNeighbourCount++;

			// Early filtering: skip neighbors already in closed set
			for (const AeonixLink& neighbour : neighbours)
			{
				if (!ClosedSet.Contains(neighbour))
				{
					ProcessLink(neighbour);
				}
			}
		}
		else
		{
			NavigationData.OctreeData.GetNeighbours(CurrentLink, neighbours);
			HigherLayerNeighbourCount++;

			// Early filtering: skip neighbors already in closed set
			for (const AeonixLink& neighbour : neighbours)
			{
				if (!ClosedSet.Contains(neighbour))
				{
					ProcessLink(neighbour);
				}
			}
		}

		// Track neighbor generation statistics
		TotalNeighborsGenerated += neighbours.Num();
		MaxNeighborsInSingleIteration = FMath::Max(MaxNeighborsInSingleIteration, neighbours.Num());

		// Periodic diagnostic logging every 100 iterations
		if (numIterations > 0 && numIterations % 100 == 0)
		{
			FVector CurrentPos;
			NavigationData.GetLinkPosition(CurrentLink, CurrentPos);
			const float DistToGoal = FVector::Dist(CurrentPos, TargetPos);

			UE_LOG(LogAeonixNavigation, Verbose, TEXT("Iteration %d: Heap=%d, Unique=%d, Dups=%d, Neighbors=%d, MaxNeighbors=%d, DistToGoal=%.1f"),
				numIterations, OpenHeap.Num(), UniqueNodesProcessed.Num(), DuplicatePopCount,
				TotalNeighborsGenerated, MaxNeighborsInSingleIteration, DistToGoal);
		}

		numIterations++;

		if (numIterations > Settings.MaxIterations)
		{
			const float Distance = FVector::Dist(StartPos, TargetPos);
			FVector CurrentPos;
			NavigationData.GetLinkPosition(CurrentLink, CurrentPos);
			const float DistToGoal = FVector::Dist(CurrentPos, TargetPos);

			UE_LOG(LogAeonixNavigation, Warning, TEXT("Pathfinding aborted - hit iteration limit %i. Distance: %.2f units. Start: %s, Target: %s, StartLink: (L:%d N:%d S:%d), GoalLink: (L:%d N:%d S:%d), CurrentLink: (L:%d N:%d S:%d)"),
				numIterations,
				Distance,
				*StartPos.ToCompactString(),
				*TargetPos.ToCompactString(),
				StartLink.GetLayerIndex(), StartLink.GetNodeIndex(), StartLink.GetSubnodeIndex(),
				InGoal.GetLayerIndex(), InGoal.GetNodeIndex(), InGoal.GetSubnodeIndex(),
				CurrentLink.GetLayerIndex(), CurrentLink.GetNodeIndex(), CurrentLink.GetSubnodeIndex());

			// Detailed diagnostic information
			UE_LOG(LogAeonixNavigation, Warning, TEXT("  Diagnostics: HeapSize=%d, UniqueNodes=%d, DuplicatePops=%d, TotalNeighbors=%d, MaxNeighbors=%d, DistToGoal=%.1f"),
				OpenHeap.Num(), UniqueNodesProcessed.Num(), DuplicatePopCount,
				TotalNeighborsGenerated, MaxNeighborsInSingleIteration, DistToGoal);

			// Calculate average neighbors per iteration
			const float AvgNeighbors = numIterations > 0 ? static_cast<float>(TotalNeighborsGenerated) / numIterations : 0.0f;
			UE_LOG(LogAeonixNavigation, Warning, TEXT("  AvgNeighborsPerIteration=%.1f, DuplicatePopRate=%.1f%%"),
				AvgNeighbors, numIterations > 0 ? (DuplicatePopCount * 100.0f) / (numIterations + DuplicatePopCount) : 0.0f);

			// Report which neighbor generation paths were taken
			UE_LOG(LogAeonixNavigation, Warning, TEXT("  NeighborGenPaths: EmptyLeaf=%d, NonEmptyLeaf=%d, HigherLayer=%d"),
				EmptyLeafNeighbourCount, NonEmptyLeafNeighbourCount, HigherLayerNeighbourCount);

			// Populate failure info if requested
			if (OutFailureInfo)
			{
				OutFailureInfo->bFailedDueToMaxIterations = true;
				OutFailureInfo->StartPosition = StartPos;
				OutFailureInfo->TargetPosition = TargetPos;
				OutFailureInfo->StartLink = StartLink;
				OutFailureInfo->GoalLink = InGoal;
				OutFailureInfo->LastProcessedLink = CurrentLink;
				OutFailureInfo->IterationCount = numIterations;
				OutFailureInfo->StraightLineDistance = Distance;
			}

			LastIterationCount = numIterations;
			return false;
		}
	}

	UE_LOG(LogAeonixNavigation, Display, TEXT("Pathfinding failed, iterations : %i"), numIterations);
	LastIterationCount = numIterations;
	return false;
}

float AeonixPathFinder::CalculateHeuristic(const AeonixLink& aStart, const AeonixLink& aTarget, const AeonixLink& aParent)
{
	float totalScore = 0.0f;

	FVector startPos, targetPos;
	NavigationData.GetLinkPosition(aStart, startPos);
	NavigationData.GetLinkPosition(aTarget, targetPos);

	// 1. Euclidean distance component
	if (Settings.HeuristicSettings.EuclideanWeight > 0.0f)
	{
		float euclideanDistance = (startPos - targetPos).Size();
		totalScore += euclideanDistance * Settings.HeuristicSettings.EuclideanWeight;
	}

	// 2. Velocity component (requires valid parent)
	if (Settings.HeuristicSettings.VelocityWeight > 0.0f && aParent.IsValid() && !(aParent == aStart))
	{
		// Get direction from parent to current node (incoming direction)
		FVector incomingDirection = GetDirectionVector(aParent, aStart);

		// Get direction from current node to target (desired direction)
		FVector outgoingDirection = GetDirectionVector(aStart, aTarget);

		// Calculate alignment using dot product (-1 to 1, where 1 = same direction)
		float alignment = FVector::DotProduct(incomingDirection, outgoingDirection);

		// Convert alignment to penalty (0 = same direction, 2 = opposite direction)
		float directionPenalty = (1.0f - alignment);

		// Apply velocity bias and weight
		float baseDistance = (startPos - targetPos).Size();
		float velocityPenalty = directionPenalty * Settings.HeuristicSettings.VelocityBias * baseDistance;
		totalScore += velocityPenalty * Settings.HeuristicSettings.VelocityWeight;
	}

	// 3. Node size component (applies to all components)
	if (Settings.HeuristicSettings.NodeSizeWeight > 0.0f)
	{
		// Higher layer index = larger voxel = should have lower score to be preferred
		float nodeSizeMultiplier = (1.0f - (static_cast<float>(aTarget.GetLayerIndex()) / static_cast<float>(NavigationData.OctreeData.GetNumLayers())) * Settings.HeuristicSettings.NodeSizeWeight);
		totalScore *= nodeSizeMultiplier;
	}

	// Apply global weight
	totalScore *= Settings.HeuristicSettings.GlobalWeight;

	return totalScore;
}


FVector AeonixPathFinder::GetDirectionVector(const AeonixLink& aStart, const AeonixLink& aTarget)
{
	FVector startPos, targetPos;
	NavigationData.GetLinkPosition(aStart, startPos);
	NavigationData.GetLinkPosition(aTarget, targetPos);

	FVector direction = targetPos - startPos;
	return direction.GetSafeNormal();
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

		// Validate distance for leaf-to-leaf transitions
		if (aStart.GetLayerIndex() == 0 && aTarget.GetLayerIndex() == 0)
		{
			// Both are leaf nodes - check if they're navigating between what should be adjacent nodes
			if (startNode.FirstChild.IsValid() && endNode.FirstChild.IsValid())
			{
				// Normal leaf-to-leaf: navigating at sub-voxel level
				// Leaf voxel size is 1/4 of the layer 0 voxel size
				float leafVoxelSize = NavigationData.GetVoxelSize(0) * 0.25f;
				// Maximum distance would be diagonal between adjacent leaf voxels
				float maxExpectedDistance = leafVoxelSize * 2.0f; // Allow for diagonal neighbors

				if (cost > maxExpectedDistance)
				{
					UE_LOG(LogAeonixNavigation, Error, TEXT("WARNING: Pathfinder attempting to navigate between distant leaf nodes! Distance: %.2f, Max Expected: %.2f"),
						cost, maxExpectedDistance);
					UE_LOG(LogAeonixNavigation, Error, TEXT("  Start Position: %s (Layer: %d, Node: %d, Subnode: %d)"),
						*startPos.ToString(), aStart.GetLayerIndex(), aStart.GetNodeIndex(), aStart.GetSubnodeIndex());
					UE_LOG(LogAeonixNavigation, Error, TEXT("  End Position: %s (Layer: %d, Node: %d, Subnode: %d)"),
						*endPos.ToString(), aTarget.GetLayerIndex(), aTarget.GetNodeIndex(), aTarget.GetSubnodeIndex());
				}
			}
		}
	}

	return cost;
}

void AeonixPathFinder::ProcessLink(const AeonixLink& aNeighbour)
{
	if (aNeighbour.IsValid())
	{
		if (ClosedSet.Contains(aNeighbour))
			return;

		float t_gScore = FLT_MAX;
		if (GScore.Contains(CurrentLink))
			t_gScore = GScore[CurrentLink] + GetCost(CurrentLink, aNeighbour);
		else
			GScore.Add(CurrentLink, FLT_MAX);

		if (t_gScore >= (GScore.Contains(aNeighbour) ? GScore[aNeighbour] : FLT_MAX))
			return;

		CameFrom.Add(aNeighbour, CurrentLink);
		GScore.Add(aNeighbour, t_gScore);

		// Calculate heuristic using unified function with parent information when available
		AeonixLink parentLink = CameFrom.Contains(CurrentLink) ? CameFrom[CurrentLink] : AeonixLink();
		float heuristicScore = CalculateHeuristic(aNeighbour, GoalLink, parentLink);

		FScore.Add(aNeighbour, GScore[aNeighbour] + heuristicScore);

		// Only add to heap if this is a NEW node (prevents 80%+ duplicate waste)
		// This eliminates the massive duplicate pop problem observed in stress testing
		if (!OpenSetLookup.Contains(aNeighbour))
		{
			OpenSetLookup.Add(aNeighbour);

			// Push to heap with calculated FScore
			FScoreHeapPredicate HeapPredicate(FScore);
			OpenHeap.HeapPush(aNeighbour, HeapPredicate);

			if (Settings.bDebugOpenNodes)
			{
				FVector pos;
				NavigationData.GetLinkPosition(aNeighbour, pos);
				Settings.DebugPoints.Add(pos);
			}
		}
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
			{
				points[points.Num() - 1].Layer = 1;
			}
			else
			{
				// Layer 0 node with leaf subdivision - use actual sub-voxel position
				points[points.Num() - 1].Layer = 0;
			}
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

	// Add intermediate points
	for (int32 i = 0; i < points.Num(); i++)
	{
		const FAeonixPathPoint& point = points[i];
		debugVoxelInfo.Add(FDebugVoxelInfo(point.Position, point.Layer, false));
	}

	// Set actual voxel positions for debug visualization
	NavigationData.GetLinkPosition(GoalLink, debugVoxelInfo[0].Position);
	NavigationData.GetLinkPosition(StartLink, debugVoxelInfo[debugVoxelInfo.Num() - 1].Position);

	oPath.SetDebugVoxelInfo(debugVoxelInfo);
#endif

	{
		SCOPE_CYCLE_COUNTER(STAT_AeonixPathChaikinSmoothing);
		Smooth_Chaikin(points, Settings.SmoothingIterations);
	}

	// Apply string pulling if enabled
	if (Settings.bUseStringPulling)
	{
		SCOPE_CYCLE_COUNTER(STAT_AeonixPathStringPulling);
		StringPullPath(points);
	}

	// Smooth the path by adjusting positions within voxel bounds
	if (Settings.bSmoothPositions)
	{
		SCOPE_CYCLE_COUNTER(STAT_AeonixPathPositionSmoothing);
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
	UE_LOG(LogAeonixNavigation, Log, TEXT("String pulling: Original points: %d, Kept points: %d"), 
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
