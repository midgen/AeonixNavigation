#pragma once

#include <AeonixNavigation/Public/Pathfinding/AeonixNavigationPath.h>
#include <AeonixNavigation/Public/Data/AeonixTypes.h>
#include <AeonixNavigation/Public/Data/AeonixLink.h>

class AAeonixBoundingVolume;

struct FNavigationPath;
struct FAeonixNavigationPath;
struct AeonixLink;

struct AeonixPathFinderSettings
{
	bool DebugOpenNodes;
	bool UseUnitCost;
	float UnitCost;
	float EstimateWeight;
	float NodeSizeCompensation;
	int SmoothingIterations;
	EAeonixPathCostType PathCostType;
	TArray<FVector> DebugPoints;

	AeonixPathFinderSettings()
		: DebugOpenNodes(false)
		, UseUnitCost(false)
		, UnitCost(1.0f)
		, EstimateWeight(1.0f)
		, NodeSizeCompensation(1.0f)
		, SmoothingIterations(0.f)
		, PathCostType(EAeonixPathCostType::EUCLIDEAN)
	{
	}
};

class AEONIXNAVIGATION_API AeonixPathFinder
{
public:
	AeonixPathFinder(UWorld* World, const FAeonixData& Data, AeonixPathFinderSettings& Settings)
		: NavigationData(Data)
		, Settings(Settings)
		, World(World){};
	~AeonixPathFinder(){};

	/* Performs an A* search from start to target navlink */
	int FindPath(const AeonixLink& aStart, const AeonixLink& aTarget, const FVector& aStartPos, const FVector& aTargetPos, FAeonixNavPathSharedPtr* oPath);

private:
	TArray<AeonixLink> OpenSet;
	TSet<AeonixLink> ClosedSet;

	TMap<AeonixLink, AeonixLink> CameFrom;

	TMap<AeonixLink, float> GScore;
	TMap<AeonixLink, float> FScore;

	AeonixLink Start;
	AeonixLink Current;
	AeonixLink Goal;

	const FAeonixData& NavigationData;

	AeonixPathFinderSettings& Settings;

	UWorld* World;

	/* A* heuristic calculation */
	float HeuristicScore(const AeonixLink& aStart, const AeonixLink& aTarget);

	/* Distance between two links */
	float GetCost(const AeonixLink& aStart, const AeonixLink& aTarget);

	void ProcessLink(const AeonixLink& aNeighbour);

	/* Constructs the path by navigating back through our CameFrom map */
	void BuildPath(TMap<AeonixLink, AeonixLink>& aCameFrom, AeonixLink aCurrent, const FVector& aStartPos, const FVector& aTargetPos, FAeonixNavPathSharedPtr* oPath);

	void Smooth_Chaikin(TArray<FAeonixPathPoint>& somePoints, int aNumIterations);
};