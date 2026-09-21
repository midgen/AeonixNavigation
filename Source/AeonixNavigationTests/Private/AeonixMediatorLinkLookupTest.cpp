// Regression tests for https://github.com/midgen/AeonixNavigation/issues/54
//
// AeonixMediator::GetLinkFromPosition loops forever when a position passes the
// actor's IsPointInside check but maps to a morton code that does not exist in the
// octree. The outer while loop only advances on a successful match, so a miss on
// any layer never terminates.
//
// Two ways that can happen with a fully generated volume:
//   1. A point exactly on the max face of the bounds. IsInsideOrOn accepts it, but
//      FloorToInt puts the voxel coordinate one past the grid.
//   2. The actor's live bounds no longer match the Origin/Extents cached in the
//      generation parameters (actor moved after baking, brush edited, etc).
//
// Each lookup runs on a worker thread with a timeout so a hang surfaces as a test
// failure rather than freezing the editor. On timeout the worker is abandoned and
// the actor is intentionally leaked so the spinning thread never touches freed data.

#include "Actor/AeonixBoundingVolume.h"
#include "Data/AeonixData.h"
#include "Data/AeonixLink.h"
#include "Util/AeonixMediator.h"
#include "../Public/AeonixNavigationTestMocks.h"

#include "Async/Async.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

namespace AeonixMediatorLinkLookupTestPrivate
{
	constexpr double LookupTimeoutSeconds = 5.0;

	UWorld* FindEditorWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Editor && Context.World())
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	// Spawns a bounding volume whose live bounds are driven by a box component (the
	// spawned AVolume has no brush, so its own bounds would be degenerate), then
	// generates octree data on it with the given collision mock.
	AAeonixBoundingVolume* SpawnGeneratedVolume(UWorld& World, const FVector& Origin, const FVector& Extents, int32 OctreeDepth,
		const IAeonixCollisionQueryInterface& Collision, const IAeonixDebugDrawInterface& DebugDraw)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;
		AAeonixBoundingVolume* Volume = World.SpawnActor<AAeonixBoundingVolume>(Origin, FRotator::ZeroRotator, SpawnParams);
		if (!Volume)
		{
			return nullptr;
		}

		UBoxComponent* Box = NewObject<UBoxComponent>(Volume, TEXT("TestBounds"));
		Box->SetBoxExtent(Extents);
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Box->SetupAttachment(Volume->GetRootComponent());
		Box->RegisterComponent();
		Volume->AddInstanceComponent(Box);

		FAeonixGenerationParameters Params = Volume->GenerationParameters;
		Params.Origin = Origin;
		Params.Extents = Extents;
		Params.OctreeDepth = OctreeDepth;
		Params.CollisionChannel = ECollisionChannel::ECC_WorldStatic;
		Params.AgentRadius = 34.f;
		Params.ShowLeafVoxels = false;
		Params.ShowMortonCodes = false;
		Params.ShowVoxels = false;

		FAeonixData& NavData = Volume->GetMutableNavData();
		NavData.UpdateGenerationParameters(Params);
		UWorld* DummyWorld = nullptr;
		NavData.Generate(*DummyWorld, Collision, DebugDraw);
		Volume->bIsReadyForNavigation = true;

		return Volume;
	}

	// Runs the lookup on a worker thread. Returns true if it completed in time.
	bool RunLookupWithTimeout(const FVector& Position, const AAeonixBoundingVolume& Volume, AeonixLink& OutLink, bool& bOutResult)
	{
		const AAeonixBoundingVolume* VolumePtr = &Volume;
		TSharedPtr<AeonixLink, ESPMode::ThreadSafe> SharedLink = MakeShared<AeonixLink, ESPMode::ThreadSafe>();

		TFuture<bool> Future = Async(EAsyncExecution::Thread, [Position, VolumePtr, SharedLink]()
		{
			return AeonixMediator::GetLinkFromPosition(Position, *VolumePtr, *SharedLink);
		});

		if (!Future.WaitFor(FTimespan::FromSeconds(LookupTimeoutSeconds)))
		{
			return false;
		}

		bOutResult = Future.Get();
		OutLink = *SharedLink;
		return true;
	}
}

using namespace AeonixMediatorLinkLookupTestPrivate;

// Case 1: a point exactly on the max face of the volume.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixMediator_LinkLookupOnMaxFaceTest,
	"AeonixNavigation.Mediator.GetLinkFromPosition.PointOnMaxFaceDoesNotHang",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixMediator_LinkLookupOnMaxFaceTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindEditorWorld();
	if (!TestNotNull(TEXT("Editor world available"), World))
	{
		return false;
	}

	FTestWallCollisionQueryInterface WallCollision;
	FSilentDebugDrawInterface DebugDraw;

	const FVector Origin = FVector::ZeroVector;
	const FVector Extents(500, 500, 500);
	AAeonixBoundingVolume* Volume = SpawnGeneratedVolume(*World, Origin, Extents, 4, WallCollision, DebugDraw);
	if (!TestNotNull(TEXT("Spawned bounding volume"), Volume))
	{
		return false;
	}
	TestTrue(TEXT("Volume has generated data"), Volume->HasData());

	// Sanity check: a point comfortably inside the volume and away from the wall resolves.
	{
		const FVector InsidePos(300, 300, 0);
		TestTrue(TEXT("Interior point is inside live bounds"), Volume->IsPointInside(InsidePos));
		AeonixLink Link;
		bool bResult = false;
		const bool bCompleted = RunLookupWithTimeout(InsidePos, *Volume, Link, bResult);
		TestTrue(TEXT("Interior lookup completes"), bCompleted);
		if (bCompleted)
		{
			TestTrue(TEXT("Interior lookup finds a link"), bResult);
		}
	}

	// The actual case: on the max X face, away from the wall at Y=0.
	const FVector MaxFacePos(Origin.X + Extents.X, 300, 0);
	TestTrue(TEXT("Max face point passes IsPointInside (IsInsideOrOn)"), Volume->IsPointInside(MaxFacePos));

	AeonixLink Link;
	bool bResult = false;
	const bool bCompleted = RunLookupWithTimeout(MaxFacePos, *Volume, Link, bResult);
	TestTrue(FString::Printf(TEXT("GetLinkFromPosition on max face point %s completes within %.0fs (issue #54)"),
		*MaxFacePos.ToString(), LookupTimeoutSeconds), bCompleted);

	if (bCompleted)
	{
		// Either answer is acceptable once it terminates; a clamped hit or a clean miss.
		UE_LOG(LogTemp, Display, TEXT("Max face lookup returned %s"), bResult ? TEXT("true") : TEXT("false"));
		World->DestroyActor(Volume);
	}
	else
	{
		AddWarning(TEXT("Lookup thread abandoned; volume actor leaked on purpose so the spinning thread stays valid."));
	}

	return true;
}

// Case 2: the actor's live bounds no longer match the cached generation bounds.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixMediator_LinkLookupStaleBoundsTest,
	"AeonixNavigation.Mediator.GetLinkFromPosition.StaleCachedBoundsDoesNotHang",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixMediator_LinkLookupStaleBoundsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindEditorWorld();
	if (!TestNotNull(TEXT("Editor world available"), World))
	{
		return false;
	}

	FTestWallCollisionQueryInterface WallCollision;
	FSilentDebugDrawInterface DebugDraw;

	const FVector Origin = FVector::ZeroVector;
	const FVector Extents(500, 500, 500);
	AAeonixBoundingVolume* Volume = SpawnGeneratedVolume(*World, Origin, Extents, 4, WallCollision, DebugDraw);
	if (!TestNotNull(TEXT("Spawned bounding volume"), Volume))
	{
		return false;
	}
	TestTrue(TEXT("Volume has generated data"), Volume->HasData());

	// Move the actor after generation. Baked data deliberately keeps the generation-time
	// Origin/Extents, so the cached grid now sits 2000 units away from the live bounds.
	const FVector Offset(2000, 0, 0);
	Volume->SetActorLocation(Origin + Offset);
	TestEqual(TEXT("Cached origin is unchanged after moving the actor"), Volume->GetNavData().GetParams().Origin, Origin);

	// Inside the live bounds, but entirely outside the cached grid.
	const FVector QueryPos = Origin + Offset + FVector(0, 300, 0);
	TestTrue(TEXT("Query point is inside the moved live bounds"), Volume->IsPointInside(QueryPos));

	AeonixLink Link;
	bool bResult = false;
	const bool bCompleted = RunLookupWithTimeout(QueryPos, *Volume, Link, bResult);
	TestTrue(FString::Printf(TEXT("GetLinkFromPosition with stale cached bounds at %s completes within %.0fs (issue #54)"),
		*QueryPos.ToString(), LookupTimeoutSeconds), bCompleted);

	if (bCompleted)
	{
		TestFalse(TEXT("Point outside the cached grid reports no link"), bResult);
		World->DestroyActor(Volume);
	}
	else
	{
		AddWarning(TEXT("Lookup thread abandoned; volume actor leaked on purpose so the spinning thread stays valid."));
	}

	return true;
}
