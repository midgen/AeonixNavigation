#pragma once

#include "AeonixNavigationPath.generated.h"

struct FAeonixData;

UENUM(BlueprintType)
enum class EAeonixPathPointType : uint8
{
	NODE_CENTER UMETA(ToolTip="Path points are in the center of each traversed node"),
	INTERMEDIATE UMETA(ToolTip="Path points are halfway between the center of each traversed node edge"),
};

UENUM(BlueprintType)
enum class EAeonixPathCostType : uint8
{
	MANHATTAN,
	EUCLIDEAN
};

struct FNavigationPath;
class AAeonixBoundingVolume;

USTRUCT(BlueprintType)
struct AEONIXNAVIGATION_API FAeonixPathPoint
{
	GENERATED_BODY()
	FAeonixPathPoint(){}
	
	FAeonixPathPoint(const FVector& aPosition, int aLayer)
		: Position(aPosition)
		, Layer(aLayer)
		, bCullFlag(false)

	{
	}

	FVector Position{}; // Position of the point
	int Layer{-1};			// Layer that the point came from (so we can infer it's volume)
	bool bCullFlag{false};
#if WITH_EDITOR
	FVector NodePosition; // Position of the node that this point came from, for debug display
#endif
	
};

USTRUCT(BlueprintType)
struct AEONIXNAVIGATION_API FAeonixNavigationPath
{
	GENERATED_BODY()
public:
	void AddPoint(const FAeonixPathPoint& aPoint);
	void ResetForRepath();

	void DebugDraw(UWorld* World, const FAeonixData& Data);

	const TArray<FAeonixPathPoint>& GetPathPoints() const
	{
		return myPoints;
	};

	TArray<FAeonixPathPoint>& GetPathPoints() { return myPoints; }

	bool IsReady() const { return myIsReady; };
	void SetIsReady(bool aIsReady) { myIsReady = aIsReady; }

	// Copy the path positions into a standard navigation path
	void CreateNavPath(FNavigationPath& aOutPath);

protected:
	bool myIsReady;
	TArray<FAeonixPathPoint> myPoints;
};