#pragma once

#include <AeonixNavigation/Public/Pathfinding/AeonixNavigationPath.h>
#include <AeonixNavigation/Public/Data/AeonixTypes.h>
#include <AeonixNavigation/Public/Data/AeonixLink.h>

#include "AeonixPathFinder.generated.h"

class AAeonixBoundingVolume;

struct FNavigationPath;
struct FAeonixNavigationPath;
struct AeonixLink;

USTRUCT(BlueprintType)
struct FAeonixPathFinderSettings
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	bool bDebugOpenNodes{false};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	bool bUseUnitCost{false};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	float UnitCost{1.0f};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	float EstimateWeight{10.0f};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	float NodeSizeCompensation{1.0f};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	int SmoothingIterations{0};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	int32 MaxIterations{5000};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	EAeonixPathCostType PathCostType{EAeonixPathCostType::EUCLIDEAN};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	EAeonixPathPointType PathPointType{EAeonixPathPointType::NODE_CENTER};
	
	TArray<FVector> DebugPoints;
};

class AEONIXNAVIGATION_API AeonixPathFinder
{
public:
	AeonixPathFinder(UWorld* World, const FAeonixData& Data, FAeonixPathFinderSettings& Settings)
		: NavigationData(Data)
		, Settings(Settings)
		, World(World){};
	~AeonixPathFinder(){};

	/* Performs an A* search from start to target navlink */
	int FindPath(const AeonixLink& aStart, const AeonixLink& aTarget, const FVector& aStartPos, const FVector& aTargetPos, FAeonixNavigationPath& oPath);

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

	FAeonixPathFinderSettings& Settings;

	UWorld* World;

	/* A* heuristic calculation */
	float HeuristicScore(const AeonixLink& aStart, const AeonixLink& aTarget);

	/* Distance between two links */
	float GetCost(const AeonixLink& aStart, const AeonixLink& aTarget);

	void ProcessLink(const AeonixLink& aNeighbour);

	/* Constructs the path by navigating back through our CameFrom map */
	void BuildPath(TMap<AeonixLink, AeonixLink>& aCameFrom, AeonixLink aCurrent, const FVector& aStartPos, const FVector& aTargetPos, FAeonixNavigationPath& oPath);

	void Smooth_Chaikin(TArray<FAeonixPathPoint>& somePoints, int aNumIterations);
};