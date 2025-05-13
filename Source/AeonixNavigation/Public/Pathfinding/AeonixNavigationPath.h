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
enum class EAeonixPathHeuristicType : uint8
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

#if WITH_EDITOR
// Structure to store debug information about the original path nodes
struct FDebugVoxelInfo
{
	FVector Position; // Original position of the voxel
	int32 Layer;     // Layer of the voxel in the octree

	FDebugVoxelInfo() : Position(FVector::ZeroVector), Layer(-1) {}
	
	FDebugVoxelInfo(const FVector& InPosition, int32 InLayer)
		: Position(InPosition), Layer(InLayer)
	{
	}
};
#endif

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

#if WITH_EDITOR
	// Sets the debug voxel information, to be called before any path optimizations
	void SetDebugVoxelInfo(const TArray<FDebugVoxelInfo>& InVoxelInfo)
	{
		myDebugVoxelInfo = InVoxelInfo;
	}
	
	// Creates debug voxel information from the current path points
	void StoreOriginalPathForDebug()
	{
		myDebugVoxelInfo.Empty(myPoints.Num());
		for (const FAeonixPathPoint& Point : myPoints)
		{
			myDebugVoxelInfo.Add(FDebugVoxelInfo(Point.Position, Point.Layer));
		}
	}
#endif

	bool IsReady() const { return myIsReady; };
	void SetIsReady(bool aIsReady) { myIsReady = aIsReady; }

	// Copy the path positions into a standard navigation path
	void CreateNavPath(FNavigationPath& aOutPath);

protected:
	bool myIsReady;
	TArray<FAeonixPathPoint> myPoints;
#if WITH_EDITOR
	TArray<FDebugVoxelInfo> myDebugVoxelInfo;
#endif
};