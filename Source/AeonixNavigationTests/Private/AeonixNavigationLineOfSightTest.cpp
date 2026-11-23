// Line of sight where a segment meets the leaf grid exactly.
//
// A segment that passes through an edge of the grid touches every cell meeting at that edge, not only
// the two it travels through, so one blocked cell among them blocks it. The cells beyond the volume's
// own faces do not exist and do not count: a segment may run along a face.

#include "Data/AeonixData.h"
#include "Data/AeonixLink.h"
#include "Engine/EngineTypes.h"
#include "Misc/AutomationTest.h"
#include "../Public/AeonixNavigationTestMocks.h"

namespace
{
	// Blocks the quarter of space with X and Y both positive. Its faces lie on the octree's centre
	// planes, so every voxel at every layer is wholly inside or wholly outside it.
	class FTestQuadrantCollisionQueryInterface : public IAeonixCollisionQueryInterface
	{
	public:
		virtual bool IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
		{
			return Position.X > 0.0 && Position.Y > 0.0;
		}

		virtual bool IsLeafBlocked(const FVector& Position, const float LeafSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
		{
			return IsBlocked(Position, LeafSize, CollisionChannel, AgentRadius);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixNavigation_LineOfSightGridEdgeTest,
	"AeonixNavigation.Data.LineOfSightGridEdges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixNavigation_LineOfSightGridEdgeTest::RunTest(const FString& Parameters)
{
	FTestQuadrantCollisionQueryInterface Collision;
	FSilentDebugDrawInterface DebugDraw;
	FAeonixData NavData;

	FAeonixGenerationParameters Params;
	Params.Origin = FVector::ZeroVector;
	Params.Extents = FVector(500, 500, 500);
	Params.OctreeDepth = 4;
	Params.CollisionChannel = ECollisionChannel::ECC_WorldStatic;
	Params.AgentRadius = 34.f;
	Params.ShowLeafVoxels = false;
	Params.ShowMortonCodes = false;
	NavData.UpdateGenerationParameters(Params);

	UWorld* DummyWorld = nullptr;
	NavData.Generate(*DummyWorld, Collision, DebugDraw);

	// A height that is not on a grid plane, so the segments below meet edges, not corners.
	const double Z = 10.0;

	AeonixLink Link;
	TestFalse(TEXT("The blocked quadrant is blocked"), NavData.GetLinkForPosition(FVector(40, 40, Z), Link));
	TestTrue(TEXT("The quadrant beside it is free"), NavData.GetLinkForPosition(FVector(-40, 40, Z), Link));
	TestTrue(TEXT("The quadrant on its other side is free"), NavData.GetLinkForPosition(FVector(40, -40, Z), Link));
	TestTrue(TEXT("The quadrant opposite it is free"), NavData.GetLinkForPosition(FVector(-40, -40, Z), Link));

	// Between the two free quadrants beside the blocked one, straight through the edge all four share.
	TestFalse(TEXT("A segment through the blocked quadrant's edge has no line of sight"),
		NavData.HasLineOfSight(FVector(-40, 40, Z), FVector(40, -40, Z)));
	TestFalse(TEXT("Nor does the same segment the other way"),
		NavData.HasLineOfSight(FVector(40, -40, Z), FVector(-40, 40, Z)));

	// The same segment moved clear of the edge, through the free quadrant opposite.
	TestTrue(TEXT("A segment passing clear of the edge has line of sight"),
		NavData.HasLineOfSight(FVector(-45, 35, Z), FVector(35, -45, Z)));

	// Away from the blocked quadrant, through the edge where four free cells meet (X = 0 and Z = 0).
	TestTrue(TEXT("A segment through an edge between free cells has line of sight"),
		NavData.HasLineOfSight(FVector(-40, -300, -40), FVector(40, -300, 40)));

	// Along the volume's bottom face from one free cell into the next: where it crosses between
	// them it is on an edge of the grid with nothing below it.
	const double Floor = Params.Origin.Z - Params.Extents.Z;
	TestTrue(TEXT("A segment along the volume's face has line of sight"),
		NavData.HasLineOfSight(FVector(-100, -300, Floor), FVector(100, -300, Floor)));
	TestFalse(TEXT("A segment along the face through the blocked quadrant's edge does not"),
		NavData.HasLineOfSight(FVector(-40, 40, Floor), FVector(40, -40, Floor)));

	return true;
}
