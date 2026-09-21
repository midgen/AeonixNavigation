// Regression test for GitHub issue #53: path shortcuts through blocked geometry.
//
// The A* search only ever steps between neighbouring free voxels, so the raw came-from
// chain never crosses blocked space. The post-processing passes that follow it do not
// share that guarantee: string pulling decides whether two points can be joined purely
// from how far the intermediate points sit from the straight line (scaled by voxel size),
// the optimise pass culls points purely on segment angle, and position smoothing slides
// points toward the line between their neighbours. None of them ask the octree whether
// the resulting segment passes through a blocked voxel.
//
// This test sweeps a seeded set of start/target pairs through obstacle worlds with the
// default post-processing enabled and asserts that every segment of every returned path
// stays inside free voxels. The first clipping pair is reported in full so it can be
// turned into a focused case.

#include "Data/AeonixData.h"
#include "Data/AeonixLink.h"
#include "Pathfinding/AeonixPathFinder.h"
#include "Pathfinding/AeonixNavigationPath.h"
#include "Engine/EngineTypes.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "../Public/AeonixNavigationTestMocks.h"

namespace
{
	struct FClipSweepResult
	{
		int32 PathsTested = 0;
		int32 PathsClipped = 0;
		FString FirstClipReport;
	};

	// Builds navigation data for the given collision world and runs NumPairs seeded queries through it.
	FClipSweepResult SweepWorld(const TCHAR* WorldName, IAeonixCollisionQueryInterface& Collision,
		const FAeonixPathFinderSettings& PathSettings, int32 Seed, int32 NumPairs)
	{
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

		AeonixPathFinder PathFinder(NavData, PathSettings);
		FRandomStream Rng(Seed);
		FClipSweepResult Result;

		// Keep queries inside the volume with a margin so the endpoints resolve to real voxels.
		const float Range = 450.f;
		int32 Attempts = 0;
		while (Result.PathsTested < NumPairs && Attempts < NumPairs * 10)
		{
			Attempts++;
			const FVector StartPos(Rng.FRandRange(-Range, Range), Rng.FRandRange(-Range, Range), Rng.FRandRange(-Range, Range));
			const FVector TargetPos(Rng.FRandRange(-Range, Range), Rng.FRandRange(-Range, Range), Rng.FRandRange(-Range, Range));

			AeonixLink StartLink, GoalLink;
			if (!GetLinkFromPosition(StartPos, NavData, StartLink) || !GetLinkFromPosition(TargetPos, NavData, GoalLink))
			{
				continue; // Endpoint inside geometry, not a useful query.
			}
			if (StartLink == GoalLink)
			{
				continue;
			}

			FAeonixNavigationPath Path;
			if (!PathFinder.FindPath(StartLink, GoalLink, StartPos, TargetPos, Path))
			{
				continue; // Unreachable pairs are not what this test is about.
			}

			Result.PathsTested++;

			const TArray<FAeonixPathPoint>& Points = Path.GetPathPoints();
			int32 BlockedSegmentEnd = -1;
			FVector BlockedSample;
			if (FindBlockedPathSegment(NavData, Points, BlockedSegmentEnd, BlockedSample))
			{
				Result.PathsClipped++;
				if (Result.FirstClipReport.IsEmpty())
				{
					Result.FirstClipReport = FString::Printf(
						TEXT("World '%s', seed %d, query #%d: start %s -> target %s\n    segment %d (%s, layer %d) -> %d (%s, layer %d) passes through blocked space at %s\n    full path (%d points):"),
						WorldName, Seed, Attempts, *StartPos.ToString(), *TargetPos.ToString(),
						BlockedSegmentEnd - 1, *Points[BlockedSegmentEnd - 1].Position.ToString(), Points[BlockedSegmentEnd - 1].Layer,
						BlockedSegmentEnd, *Points[BlockedSegmentEnd].Position.ToString(), Points[BlockedSegmentEnd].Layer,
						*BlockedSample.ToString(), Points.Num());
					for (int32 i = 0; i < Points.Num(); ++i)
					{
						Result.FirstClipReport += FString::Printf(TEXT("\n      %d: %s (layer %d)"), i, *Points[i].Position.ToString(), Points[i].Layer);
					}
				}
			}
		}

		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixNavigation_StringPullClippingTest,
	"AeonixNavigation.Pathfinding.PostProcessClipping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixNavigation_StringPullClippingTest::RunTest(const FString& Parameters)
{
	// Default post-processing: optimise, string pulling and position smoothing all on.
	FAeonixPathFinderSettings PathSettings;
	PathSettings.MaxIterations = 10000;
	PathSettings.bUseUnitCost = false;
	PathSettings.HeuristicSettings.EuclideanWeight = 1.0f;
	PathSettings.HeuristicSettings.GlobalWeight = 10.0f;

	const int32 Seed = 63053;
	const int32 PairsPerWorld = 150;

	// World A: two walls on the X=0 plane with a gap between them.
	FTestPartialObstacleCollisionQueryInterface GapWorld;

	// World B: a wall on the Y=0 plane that stops short of the +X edge and the top of the volume,
	// so routes go around its end or over its top.
	FTestWallCollisionQueryInterface CornerWorld;
	CornerWorld.WallXMin = -1000.f;
	CornerWorld.WallXMax = 250.f;
	CornerWorld.WallZMin = -1000.f;
	CornerWorld.WallZMax = 250.f;

	const FClipSweepResult GapResult = SweepWorld(TEXT("gap"), GapWorld, PathSettings, Seed, PairsPerWorld);
	const FClipSweepResult CornerResult = SweepWorld(TEXT("corner"), CornerWorld, PathSettings, Seed, PairsPerWorld);

	UE_LOG(LogTemp, Display, TEXT("Post-process clipping sweep: gap world %d/%d clipped, corner world %d/%d clipped"),
		GapResult.PathsClipped, GapResult.PathsTested, CornerResult.PathsClipped, CornerResult.PathsTested);

	TestTrue(TEXT("Sweep found enough reachable pairs in the gap world"), GapResult.PathsTested >= PairsPerWorld / 2);
	TestTrue(TEXT("Sweep found enough reachable pairs in the corner world"), CornerResult.PathsTested >= PairsPerWorld / 2);

	for (const FClipSweepResult* Result : { &GapResult, &CornerResult })
	{
		if (Result->PathsClipped > 0)
		{
			AddError(FString::Printf(TEXT("Issue #53: %d of %d post-processed paths pass through blocked voxels. First case:\n  %s"),
				Result->PathsClipped, Result->PathsTested, *Result->FirstClipReport));
		}
	}

	TestEqual(TEXT("No post-processed path in the gap world passes through a blocked voxel"), GapResult.PathsClipped, 0);
	TestEqual(TEXT("No post-processed path in the corner world passes through a blocked voxel"), CornerResult.PathsClipped, 0);

	return true;
}
