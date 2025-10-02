#pragma once

#include <AeonixNavigation/Public/Pathfinding/AeonixNavigationPath.h>
#include <AeonixNavigation/Public/Data/AeonixLink.h>

#include "AeonixPathFinder.generated.h"

class AAeonixBoundingVolume;

struct FNavigationPath;
struct FAeonixNavigationPath;
struct AeonixLink;

USTRUCT(BlueprintType)
struct FAeonixHeuristicSettings
{
	GENERATED_BODY()

	/** Weight factor for euclidean distance heuristic (distance to goal) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heuristics", meta=(ClampMin="0.0"))
	float EuclideanWeight{1.0f};

	/** Weight factor for velocity heuristic (favors maintaining direction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heuristics", meta=(ClampMin="0.0"))
	float VelocityWeight{0.0f};

	/** Controls how much the velocity heuristic favors maintaining direction (0.0-1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heuristics", meta=(EditCondition="VelocityWeight > 0.0", ClampMin="0.0", ClampMax="1.0"))
	float VelocityBias{0.5f};

	/** Weight factor that increases score for higher layer nodes (e.g. larger voxels). Can reduce iterations considerably */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heuristics", meta=(ClampMin="0.0"))
	float NodeSizeWeight{1.0f};

	/** Global multiplier applied to the final combined heuristic score */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heuristics", meta=(ClampMin="0.0"))
	float GlobalWeight{10.0f};
};

USTRUCT(BlueprintType)
struct FAeonixPathFinderSettings
{
	GENERATED_BODY()

	/** Stores the nodes that are opened in the search, expensive, but looks cool! */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	bool bDebugOpenNodes{false};
	/** Uses a unit cost for traversing a voxel, instead of the actual distance for pathfinding */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	bool bUseUnitCost{false};
	/** The unit cost to apply */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix", meta=(EditCondition="bUseUnitCost"))
	float UnitCost{1.0f};
	
	/** Max iterations for the A* pathfinding algorithm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	int32 MaxIterations{5000};
	/** Heuristic calculation settings for pathfinding algorithm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	FAeonixHeuristicSettings HeuristicSettings;
	/** The method to use for calcuting the path position for a given node */ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	EAeonixPathPointType PathPointType{EAeonixPathPointType::NODE_CENTER};
	/** Apply path optmisation to prune uneccessary points */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	bool bOptimizePath{true};
	/** Tolerance for dot product when checking if two points are aligned enough to the previous point, to be consider for trimming*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix",meta=(EditCondition="bOptimizePath"))
	double OptimizeDotTolerance{FLT_EPSILON};
	/** Apply string-pulling algorithm for path smoothing (provides better results than simple optimization) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	bool bUseStringPulling{true};
	/** Controls how much a path can deviate as a fraction of voxel size (lower values create tighter paths) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix", meta=(EditCondition="bUseStringPulling", ClampMin="0.1", ClampMax="1.0"))
	float StringPullingVoxelThreshold{0.16841f};
	/** Adjust point positions within voxel bounds for smoother paths */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	bool bSmoothPositions{true};
	/** Controls how much to adjust positions when smoothing (0.0-1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix", meta=(EditCondition="bSmoothPositions", ClampMin="0.0", ClampMax="1.0"))
	float SmoothingFactor{0.7f};
	/** A crude Chaikan smoothing operation, multiplies points, not reccomended */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	int SmoothingIterations{0};

	mutable TArray<FVector> DebugPoints;
};

class AEONIXNAVIGATION_API AeonixPathFinder
{
public:
	AeonixPathFinder(const FAeonixData& Data, const FAeonixPathFinderSettings& Settings)
		: NavigationData(Data)
		, Settings(Settings){};

	/* Performs an A* search from start to target navlink */
	bool FindPath(const AeonixLink& aStart, const AeonixLink& aTarget, const FVector& aStartPos, const FVector& aTargetPos, FAeonixNavigationPath& oPath);

private:
	TArray<AeonixLink> OpenSet;
	TSet<AeonixLink> ClosedSet;

	TMap<AeonixLink, AeonixLink> CameFrom;

	TMap<AeonixLink, float> GScore;
	TMap<AeonixLink, float> FScore;

	AeonixLink StartLink;
	AeonixLink CurrentLink;
	AeonixLink GoalLink;

	const FAeonixData& NavigationData;

	const FAeonixPathFinderSettings& Settings;

	/* Unified A* heuristic calculation combining euclidean, velocity, and node size components */
	float CalculateHeuristic(const AeonixLink& aStart, const AeonixLink& aTarget, const AeonixLink& aParent = AeonixLink());

	/* Distance between two links */
	float GetCost(const AeonixLink& aStart, const AeonixLink& aTarget);

	/* Calculate normalized direction vector between two links */
	FVector GetDirectionVector(const AeonixLink& aStart, const AeonixLink& aTarget);

	void ProcessLink(const AeonixLink& aNeighbour);

	/* Constructs the path by navigating back through our CameFrom map */
	void BuildPath(TMap<AeonixLink, AeonixLink>& aCameFrom, AeonixLink aCurrent, const FVector& aStartPos, const FVector& aTargetPos, FAeonixNavigationPath& oPath);

	/* Implements corridor-based string pulling algorithm to smooth path by removing unnecessary waypoints */
	void StringPullPath(TArray<FAeonixPathPoint>& pathPoints);

	/* Adjusts path point positions within voxel bounds to create smoother paths */
	void SmoothPathPositions(TArray<FAeonixPathPoint>& pathPoints);

	void Smooth_Chaikin(TArray<FAeonixPathPoint>& somePoints, int aNumIterations);
};