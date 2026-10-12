// UAeonixVolumeComponent: navigation data generated in the component's own frame stays valid
// while the owner moves and rotates, paths re-project through the live transform, the baked
// asset round-trips, and the flying movement component rides a moving volume.

#include "Component/AeonixFlyingMovementComponent.h"
#include "Component/AeonixVolumeComponent.h"
#include "Data/AeonixLink.h"
#include "Data/AeonixNavDataAsset.h"
#include "Interface/AeonixCollisionQueryInterface.h"
#include "Pathfinding/AeonixNavigationPath.h"
#include "Pathfinding/AeonixPathFinder.h"
#include "Subsystem/AeonixSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ObjectWriter.h"
#include "UObject/Package.h"

namespace AeonixVolumeComponentTestPrivate
{
	/** A world-space sphere, tested exactly against oriented boxes */
	class FSphereCollision : public IAeonixCollisionQueryInterface
	{
	public:
		FVector Center = FVector::ZeroVector;
		float Radius = 150.f;

		bool IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
		{
			return IsBlockedOriented(Position, FQuat::Identity, VoxelSize, CollisionChannel, AgentRadius);
		}

		bool IsLeafBlocked(const FVector& Position, const float LeafSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
		{
			return IsBlockedOriented(Position, FQuat::Identity, LeafSize, CollisionChannel, AgentRadius);
		}

		bool IsBlockedOriented(const FVector& Position, const FQuat& Rotation, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
		{
			const float HalfSize = VoxelSize + AgentRadius;
			const FVector LocalCenter = Rotation.UnrotateVector(Center - Position);
			const FVector Closest = LocalCenter.BoundToBox(FVector(-HalfSize), FVector(HalfSize));
			return FVector::DistSquared(Closest, LocalCenter) <= FMath::Square(Radius);
		}

		bool IsLeafBlockedOriented(const FVector& Position, const FQuat& Rotation, const float LeafSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
		{
			return IsBlockedOriented(Position, Rotation, LeafSize, CollisionChannel, AgentRadius);
		}

		bool GetBlockingBounds(const FTransform& Frame, FBox& OutLocalBounds) const override
		{
			const FVector LocalCenter = Frame.InverseTransformPosition(Center);
			OutLocalBounds = FBox(LocalCenter - FVector(Radius), LocalCenter + FVector(Radius));
			return true;
		}
	};

	/** Nothing blocks */
	class FEmptyCollision : public IAeonixCollisionQueryInterface
	{
	public:
		bool IsBlocked(const FVector&, const float, ECollisionChannel, const float) const override { return false; }
		bool IsLeafBlocked(const FVector&, const float, ECollisionChannel, const float) const override { return false; }
	};

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

	template <typename ActorType>
	ActorType* SpawnWithRoot(UWorld& World, const FTransform& Transform)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;
		ActorType* Actor = World.SpawnActor<ActorType>(ActorType::StaticClass(), Transform, SpawnParams);
		if (Actor && !Actor->GetRootComponent())
		{
			USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
			Root->SetMobility(EComponentMobility::Movable);
			Actor->SetRootComponent(Root);
			Root->RegisterComponent();
			Actor->SetActorTransform(Transform);
		}
		return Actor;
	}

	UAeonixVolumeComponent* AddVolume(AActor& Actor, float HalfExtent, int32 OctreeDepth)
	{
		UAeonixVolumeComponent* Volume = NewObject<UAeonixVolumeComponent>(&Actor);
		Volume->HalfExtent = HalfExtent;
		Volume->GenerationParameters.OctreeDepth = OctreeDepth;
		Volume->GenerationParameters.AgentRadius = 0.f;
		Volume->GenerationParameters.GenerationStrategy = ESVOGenerationStrategy::GenerateOnBeginPlay;
		Volume->SetupAttachment(Actor.GetRootComponent());
		Volume->RegisterComponent();
		return Volume;
	}

	FAeonixPathFinderSettings MakePathSettings()
	{
		FAeonixPathFinderSettings Settings;
		Settings.MaxIterations = 10000;
		Settings.bUseUnitCost = false;
		Settings.bOptimizePath = true;
		Settings.bUseStringPulling = true;
		return Settings;
	}

	const FTransform StartTransform(FRotator(20.f, 37.f, -15.f), FVector(10000.f, -5000.f, 3000.f));
	const FTransform MovedTransform(FRotator(-60.f, 170.f, 45.f), FVector(-2500.f, 8000.f, -400.f));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixVolumeComponent_MovingFrameTest,
	"AeonixNavigation.VolumeComponent.MovingFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixVolumeComponent_MovingFrameTest::RunTest(const FString& Parameters)
{
	using namespace AeonixVolumeComponentTestPrivate;

	UWorld* World = FindEditorWorld();
	UAeonixSubsystem* Subsystem = World ? World->GetSubsystem<UAeonixSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("Editor world with Aeonix subsystem"), Subsystem))
	{
		return false;
	}

	AActor* Owner = SpawnWithRoot<AActor>(*World, StartTransform);
	UAeonixVolumeComponent* Volume = AddVolume(*Owner, 500.f, 4);

	FSphereCollision Sphere;
	Sphere.Center = Volume->NavToWorld(FVector::ZeroVector);
	TestTrue(TEXT("Generates against an explicit source"), Volume->GenerateWith(Sphere));
	TestTrue(TEXT("Has data"), Volume->HasData());

	AeonixLink Link;
	TestFalse(TEXT("Sphere centre is blocked"), Volume->GetNavData().GetLinkForPosition(FVector::ZeroVector, Link));
	TestTrue(TEXT("Clear corner is free"), Volume->GetNavData().GetLinkForPosition(FVector(-400.f, -400.f, -400.f), Link));

	const FVector NavStart(-400.f, 0.f, 0.f);
	const FVector NavEnd(400.f, 0.f, 0.f);
	FAeonixNavigationPath Path;
	TestTrue(TEXT("Paths around the sphere"), Subsystem->FindPathInVolume(*Volume, NavStart, NavEnd, MakePathSettings(), Path));
	TestTrue(TEXT("Path has points"), Path.GetNumPoints() >= 2);

	for (int32 i = 0; i < Path.GetNumPoints(); i++)
	{
		const FAeonixPathPoint& Point = Path.GetPathPoints()[i];
		TestTrue(FString::Printf(TEXT("Point %d is outside the sphere"), i), Point.NavPosition.Size() > Sphere.Radius * 0.9f);
		TestTrue(FString::Printf(TEXT("Point %d world position matches its nav position"), i),
			Point.Position.Equals(Volume->NavToWorld(Point.NavPosition), 0.1f));
	}

	// Move and rotate the owner without regenerating
	Owner->SetActorTransform(MovedTransform);
	Sphere.Center = Volume->NavToWorld(FVector::ZeroVector);

	TestTrue(TEXT("Moved sphere centre is inside the moved volume"), Volume->IsPointInside(Sphere.Center));
	TestFalse(TEXT("Moved sphere centre is still blocked"), Volume->GetNavData().GetLinkForPosition(Volume->WorldToNav(Sphere.Center), Link));
	TestFalse(TEXT("The old world position is outside the moved volume"), Volume->IsPointInside(StartTransform.GetLocation()));

	for (int32 i = 0; i < Path.GetNumPoints(); i++)
	{
		const FVector Expected = MovedTransform.TransformPosition(Path.GetPathPoints()[i].NavPosition);
		TestTrue(FString::Printf(TEXT("Point %d follows the moved volume"), i), Path.GetPointWorldPosition(i).Equals(Expected, 0.1f));
	}

	Owner->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixVolumeComponent_CubeAndFitTest,
	"AeonixNavigation.VolumeComponent.CubeAndFit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixVolumeComponent_CubeAndFitTest::RunTest(const FString& Parameters)
{
	using namespace AeonixVolumeComponentTestPrivate;

	UWorld* World = FindEditorWorld();
	if (!TestNotNull(TEXT("Editor world"), World))
	{
		return false;
	}

	AActor* Owner = SpawnWithRoot<AActor>(*World, StartTransform);
	UAeonixVolumeComponent* Volume = AddVolume(*Owner, 300.f, 3);

	TestEqual(TEXT("Box extent mirrors the cube"), Volume->GetUnscaledBoxExtent(), FVector(300.f));
	TestTrue(TEXT("Inside along a rotated axis"), Volume->IsPointInside(Volume->NavToWorld(FVector(0.f, 290.f, 0.f))));
	TestFalse(TEXT("Outside past the face"), Volume->IsPointInside(Volume->NavToWorld(FVector(0.f, 0.f, 310.f))));

	FSphereCollision Sphere;
	Sphere.Center = Volume->NavToWorld(FVector(1000.f, -200.f, 50.f));
	Sphere.Radius = 400.f;
	Volume->FitPadding = 100.f;
	Volume->TargetLeafVoxelSize = 30.f;
	TestTrue(TEXT("Fits to the source"), Volume->FitToSource(Sphere));

	TestTrue(TEXT("Centred on the source"), Volume->GetComponentLocation().Equals(Sphere.Center, 0.1f));
	TestEqual(TEXT("Half extent is radius plus padding"), Volume->HalfExtent, 500.f);
	TestTrue(TEXT("Leaf voxels meet the target"), Volume->GetLeafVoxelSize(Volume->GenerationParameters.OctreeDepth) <= 30.f);
	TestTrue(TEXT("Depth is the smallest that meets it"), Volume->GetLeafVoxelSize(Volume->GenerationParameters.OctreeDepth - 1) > 30.f);
	TestEqual(TEXT("Box extent follows the fit"), Volume->GetUnscaledBoxExtent(), FVector(500.f));

	Owner->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixVolumeComponent_NavDataAssetTest,
	"AeonixNavigation.VolumeComponent.NavDataAssetRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixVolumeComponent_NavDataAssetTest::RunTest(const FString& Parameters)
{
	using namespace AeonixVolumeComponentTestPrivate;

	UWorld* World = FindEditorWorld();
	if (!TestNotNull(TEXT("Editor world"), World))
	{
		return false;
	}

	AActor* Owner = SpawnWithRoot<AActor>(*World, StartTransform);
	UAeonixVolumeComponent* Volume = AddVolume(*Owner, 500.f, 4);
	FSphereCollision Sphere;
	Sphere.Center = Volume->NavToWorld(FVector(100.f, 0.f, 0.f));
	Volume->GenerateWith(Sphere);

	UAeonixNavDataAsset* Asset = NewObject<UAeonixNavDataAsset>(GetTransientPackage());
	Volume->StoreToAsset(Asset);
	TestEqual(TEXT("Asset records leaf count"), Asset->NumLeafNodes, Volume->GetNavData().OctreeData.LeafNodes.Num());
	TestEqual(TEXT("Component references the asset"), Volume->NavDataAsset.Get(), Asset);

	TArray<uint8> Bytes;
	FObjectWriter Writer(Asset, Bytes);
	UAeonixNavDataAsset* Loaded = NewObject<UAeonixNavDataAsset>(GetTransientPackage());
	FObjectReader Reader(Loaded, Bytes);
	TestEqual(TEXT("Loaded leaf count"), Loaded->GetNavData().OctreeData.LeafNodes.Num(), Asset->NumLeafNodes);
	TestEqual(TEXT("Loaded depth"), Loaded->GetNavData().GetParams().OctreeDepth, 4);

	UAeonixVolumeComponent* Second = AddVolume(*Owner, 500.f, 2);
	Second->GenerationParameters.GenerationStrategy = ESVOGenerationStrategy::UseBaked;
	Second->NavDataAsset = Loaded;
	TestTrue(TEXT("Loads from the asset"), Second->LoadFromAsset());
	TestTrue(TEXT("Ready after load"), Second->IsReadyForNavigation());
	TestEqual(TEXT("Baked depth overrides the component's"), Second->GetNavData().GetParams().OctreeDepth, 4);

	AeonixLink Link;
	TestFalse(TEXT("Baked sphere is blocked"), Second->GetNavData().GetLinkForPosition(FVector(100.f, 0.f, 0.f), Link));
	TestTrue(TEXT("Baked free space is free"), Second->GetNavData().GetLinkForPosition(FVector(-400.f, 0.f, 0.f), Link));

	Owner->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixVolumeComponent_RideTest,
	"AeonixNavigation.VolumeComponent.FlyingMovementRidesVolume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixVolumeComponent_RideTest::RunTest(const FString& Parameters)
{
	using namespace AeonixVolumeComponentTestPrivate;

	UWorld* World = FindEditorWorld();
	if (!TestNotNull(TEXT("Editor world"), World))
	{
		return false;
	}

	AActor* Owner = SpawnWithRoot<AActor>(*World, StartTransform);
	UAeonixVolumeComponent* Volume = AddVolume(*Owner, 500.f, 3);
	Volume->GenerateWith(FEmptyCollision());

	const FVector NavDronePosition(200.f, -100.f, 50.f);
	APawn* Drone = SpawnWithRoot<APawn>(*World, FTransform(Volume->NavToWorld(NavDronePosition)));
	UAeonixFlyingMovementComponent* Movement = NewObject<UAeonixFlyingMovementComponent>(Drone, TEXT("Flying"));
	Movement->RegisterComponent();
	Movement->SetUpdatedComponent(Drone->GetRootComponent());

	// The first tick finds the volume, the second carries the drone with it
	Movement->TickComponent(0.016f, LEVELTICK_All, nullptr);
	Owner->SetActorTransform(MovedTransform);
	Movement->TickComponent(0.016f, LEVELTICK_All, nullptr);

	const FVector Expected = Volume->NavToWorld(NavDronePosition);
	TestTrue(FString::Printf(TEXT("Drone rode the volume (at %s, expected %s)"), *Drone->GetActorLocation().ToCompactString(), *Expected.ToCompactString()),
		Drone->GetActorLocation().Equals(Expected, 0.5f));

	Drone->Destroy();
	Owner->Destroy();
	return true;
}
