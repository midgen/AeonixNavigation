#include "AeonixBatchTestActor.h"
#include "../Public/AeonixPerformanceTypes.h"
#include "AeonixNavigation/Public/Actor/AeonixBoundingVolume.h"
#include "AeonixNavigation/Public/Component/AeonixNavAgentComponent.h"
#include "AeonixNavigation/Public/Subsystem/AeonixSubsystem.h"
#include "AeonixNavigation/Public/Interface/AeonixSubsystemInterface.h"
#include "AeonixNavigation/Public/Util/AeonixMediator.h"
#include "AenoixEditorDebugSubsystem.h"
#include "Runtime/Engine/Classes/Engine/World.h"
#include "Runtime/Engine/Public/DrawDebugHelpers.h"
#include "Runtime/Engine/Public/EngineUtils.h"
#include "Runtime/Core/Public/Misc/DateTime.h"
#include "Runtime/Core/Public/HAL/PlatformFilemanager.h"
#include "Runtime/Core/Public/Misc/FileHelper.h"

AAeonixBatchTestActor::AAeonixBatchTestActor()
{
	PrimaryActorTick.bCanEverTick = true;
#if WITH_EDITOR
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
#endif

	// Create root component
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	NavAgentComponent = CreateDefaultSubobject<UAeonixNavAgentComponent>(TEXT("NavAgentComponent"));

#if WITH_EDITOR
	bIsEditorOnlyActor = true;
#endif
}

void AAeonixBatchTestActor::BeginPlay()
{
	Super::BeginPlay();

	AeonixSubsystem = GetWorld()->GetSubsystem<UAeonixSubsystem>();
	RandomStream.Initialize(TestSettings.RandomSeed);
}

void AAeonixBatchTestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelBatchTest();
	ClearVisualization();

	// Unregister nav component from subsystem
	if (AeonixSubsystem && NavAgentComponent)
	{
		AeonixSubsystem->UnRegisterNavComponent(NavAgentComponent, EAeonixMassEntityFlag::Disabled);
	}

	Super::EndPlay(EndPlayReason);
}

void AAeonixBatchTestActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FAeonixPerformanceTestSettings, RandomSeed))
	{
		RandomStream.Initialize(TestSettings.RandomSeed);
	}
}

#if WITH_EDITOR
void AAeonixBatchTestActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (bFinished && TestSettings.bVisualizeResults)
	{
		VisualizeTestResults();
	}
}

void AAeonixBatchTestActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	RandomStream.Initialize(TestSettings.RandomSeed);

	// Skip registration during cooking/commandlet execution to avoid crashes
	UWorld* World = GetWorld();
	if (!World || World->WorldType == EWorldType::Inactive || IsRunningCommandlet())
	{
		return;
	}

	// Always register the nav component in OnConstruction (like the debug actor)
	if (NavAgentComponent)
	{
		UAeonixSubsystem* Subsystem = World->GetSubsystem<UAeonixSubsystem>();
		if (Subsystem)
		{
			Subsystem->RegisterNavComponent(NavAgentComponent, EAeonixMassEntityFlag::Enabled);
			UE_LOG(LogTemp, Log, TEXT("Registered NavAgentComponent in OnConstruction"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to get AeonixSubsystem in OnConstruction"));
		}
	}
}
#endif

void AAeonixBatchTestActor::StartBatchTest()
{
	if (CurrentStatus == EAeonixPerformanceTestStatus::Running)
	{
		UE_LOG(LogTemp, Warning, TEXT("Performance test is already running"));
		return;
	}

	if (!AeonixSubsystem)
	{
		AeonixSubsystem = GetWorld()->GetSubsystem<UAeonixSubsystem>();
	}

	AAeonixBoundingVolume* TargetVolume = GetTargetVolume();
	if (!TargetVolume)
	{
		UE_LOG(LogTemp, Error, TEXT("No valid target volume found for performance testing"));
		return;
	}

	if (!TargetVolume->HasData())
	{
		UE_LOG(LogTemp, Error, TEXT("Target volume has no navigation data"));
		return;
	}

	CurrentStatus = EAeonixPerformanceTestStatus::Running;
	CurrentTestIndex = 0;
	CurrentResults.Empty();
	CurrentResults.Reserve(TestSettings.NumberOfTests);
	TestStartTime = FPlatformTime::Seconds();

	ClearVisualization();

	UE_LOG(LogTemp, Log, TEXT("Starting performance test with %d iterations"), TestSettings.NumberOfTests);

	RunSynchronousTests();
}

void AAeonixBatchTestActor::CancelBatchTest()
{
	if (CurrentStatus != EAeonixPerformanceTestStatus::Running)
	{
		return;
	}

	CurrentStatus = EAeonixPerformanceTestStatus::Cancelled;

	UE_LOG(LogTemp, Log, TEXT("Performance test cancelled"));
}

void AAeonixBatchTestActor::ClearResults()
{
	CurrentResults.Empty();
	LastTestSummary = FAeonixPerformanceTestSummary();
	CurrentStatus = EAeonixPerformanceTestStatus::NotStarted;
	ClearVisualization();
}


void AAeonixBatchTestActor::RunSynchronousTests()
{
	for (CurrentTestIndex = 0; CurrentTestIndex < TestSettings.NumberOfTests; CurrentTestIndex++)
	{
		FVector EndPos;
		if (!GenerateRandomEndPoint(EndPos))
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to generate valid end point %d"), CurrentTestIndex);
			continue;
		}

		FAeonixPerformanceTestResult TestResult;
		ExecuteSingleTest(GetActorLocation(), EndPos, TestResult);
		CurrentResults.Add(TestResult);
	}

	OnTestCompleted();
}



bool AAeonixBatchTestActor::GenerateRandomNavigablePoint(const AAeonixBoundingVolume* Volume, FVector& OutPoint)
{
	if (!Volume || !Volume->HasData())
	{
		return false;
	}

	FBox VolumeBounds = Volume->GetComponentsBoundingBox(true);
	const int32 MaxAttempts = 50;

	for (int32 Attempt = 0; Attempt < MaxAttempts; Attempt++)
	{
		FVector RandomPoint = FVector(
			RandomStream.FRandRange(VolumeBounds.Min.X, VolumeBounds.Max.X),
			RandomStream.FRandRange(VolumeBounds.Min.Y, VolumeBounds.Max.Y),
			RandomStream.FRandRange(VolumeBounds.Min.Z, VolumeBounds.Max.Z)
		);

		AeonixLink TestLink;
		if (AeonixMediator::GetLinkFromPosition(RandomPoint, *Volume, TestLink))
		{
			OutPoint = RandomPoint;
			return true;
		}
	}

	return false;
}

bool AAeonixBatchTestActor::GenerateRandomEndPoint(FVector& OutEnd)
{
	AAeonixBoundingVolume* TargetVolume = GetTargetVolume();
	if (!TargetVolume)
	{
		return false;
	}

	FVector ActorPosition = GetActorLocation();
	const int32 MaxAttempts = 50;

	for (int32 Attempt = 0; Attempt < MaxAttempts; Attempt++)
	{
		if (!GenerateRandomNavigablePoint(TargetVolume, OutEnd))
		{
			continue;
		}

		float Distance = FVector::Dist(ActorPosition, OutEnd);
		if (Distance >= TestSettings.MinPathDistance && Distance <= TestSettings.MaxPathDistance)
		{
			return true;
		}
	}

	return false;
}

void AAeonixBatchTestActor::ExecuteSingleTest(const FVector& Start, const FVector& End, FAeonixPerformanceTestResult& OutResult)
{
	// Use actor's current position as start point
	FVector ActorPosition = GetActorLocation();

	OutResult.StartPosition = ActorPosition;
	OutResult.EndPosition = End;
	OutResult.bPathFound = false;
	OutResult.PathfindingTime = 0.0f;
	OutResult.PathLength = 0.0f;
	OutResult.NodesExplored = 0;
	OutResult.PathPoints = 0;

	if (!AeonixSubsystem || !NavAgentComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("ExecuteSingleTest failed: AeonixSubsystem=%s, NavAgentComponent=%s"),
			AeonixSubsystem ? TEXT("Valid") : TEXT("NULL"),
			NavAgentComponent ? TEXT("Valid") : TEXT("NULL"));
		return;
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("ExecuteSingleTest: From (%.1f,%.1f,%.1f) to (%.1f,%.1f,%.1f)"),
		ActorPosition.X, ActorPosition.Y, ActorPosition.Z, End.X, End.Y, End.Z);

	// No need to move actor - use current position

	FAeonixNavigationPath NavigationPath;

	double StartTime = FPlatformTime::Seconds();
	bool bPathFound = AeonixSubsystem->FindPathImmediateAgent(NavAgentComponent, End, NavigationPath);
	double EndTime = FPlatformTime::Seconds();

	OutResult.bPathFound = bPathFound;
	OutResult.PathfindingTime = (float)(EndTime - StartTime);

	UE_LOG(LogTemp, VeryVerbose, TEXT("Pathfinding result: %s, Time: %.6f seconds"),
		bPathFound ? TEXT("SUCCESS") : TEXT("FAILED"), OutResult.PathfindingTime);

	if (bPathFound)
	{
		// Calculate path length manually
		const TArray<FAeonixPathPoint>& PathPoints = NavigationPath.GetPathPoints();
		float PathLength = 0.0f;
		for (int32 i = 0; i < PathPoints.Num() - 1; i++)
		{
			PathLength += FVector::Dist(PathPoints[i].Position, PathPoints[i + 1].Position);
		}
		OutResult.PathLength = PathLength;
		OutResult.PathPoints = NavigationPath.GetPathPoints().Num();

		if (TestSettings.bVisualizeResults)
		{
			VisualizationPaths.Add(NavigationPath);
			VisualizationPoints.Add(ActorPosition);
			VisualizationPoints.Add(End);
			UE_LOG(LogTemp, VeryVerbose, TEXT("Added path for visualization: %d points, total stored: %d"),
				PathPoints.Num(), VisualizationPaths.Num());
		}
	}
	else
	{
		// Store failed path for visualization
		if (TestSettings.bVisualizeResults)
		{
			FailedPathVisualizationPoints.Add(TPair<FVector, FVector>(ActorPosition, End));
			UE_LOG(LogTemp, VeryVerbose, TEXT("Added failed path for visualization from (%.1f,%.1f,%.1f) to (%.1f,%.1f,%.1f)"),
				ActorPosition.X, ActorPosition.Y, ActorPosition.Z, End.X, End.Y, End.Z);
		}
	}
}

void AAeonixBatchTestActor::OnTestCompleted()
{
	CurrentStatus = EAeonixPerformanceTestStatus::Completed;

	LastTestSummary.Results = CurrentResults;
	LastTestSummary.TotalTestTime = FPlatformTime::Seconds() - TestStartTime;
	LastTestSummary.CalculateSummary();

	UE_LOG(LogTemp, Log, TEXT("Performance test completed. Success rate: %.1f%%, Average time: %.6f seconds"),
		LastTestSummary.SuccessRate, LastTestSummary.AveragePathfindingTime);

	if (TestSettings.bVisualizeResults)
	{
		VisualizeTestResults();
	}

}


void AAeonixBatchTestActor::VisualizeTestResults()
{
	UE_LOG(LogTemp, Log, TEXT("Visualizing %d successful paths and %d failed paths from performance test"),
		VisualizationPaths.Num(), FailedPathVisualizationPoints.Num());

	// Push paths to debug subsystem for visualization
	if (GEditor)
	{
		UAenoixEditorDebugSubsystem* DebugSubsystem = GEditor->GetEditorSubsystem<UAenoixEditorDebugSubsystem>();
		if (DebugSubsystem)
		{
			// Send successful paths to debug subsystem
			DebugSubsystem->SetBatchRunPaths(VisualizationPaths);

			// Send failed paths to debug subsystem
			DebugSubsystem->SetFailedBatchRunPaths(FailedPathVisualizationPoints);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to get debug subsystem for path visualization"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GEditor not available for path visualization"));
	}
}

void AAeonixBatchTestActor::ClearVisualization()
{
	VisualizationPoints.Empty();
	VisualizationPaths.Empty();
	FailedPathVisualizationPoints.Empty();

	// Clear paths from debug subsystem
	if (GEditor)
	{
		UAenoixEditorDebugSubsystem* DebugSubsystem = GEditor->GetEditorSubsystem<UAenoixEditorDebugSubsystem>();
		if (DebugSubsystem)
		{
			DebugSubsystem->ClearBatchRunPaths();
			DebugSubsystem->ClearFailedBatchRunPaths();
		}
	}

	if (GetWorld())
	{
		UE_LOG(LogTemp, Log, TEXT("FlushPersistentDebugLines called from AAeonixBatchTestActor (clearing batch test visualization)"));
		FlushDebugStrings(GetWorld());
		FlushPersistentDebugLines(GetWorld());
	}
}


AAeonixBoundingVolume* AAeonixBatchTestActor::GetTargetVolume() const
{
	TArray<AAeonixBoundingVolume*> AllVolumes = GetAllVolumes();
	if (AllVolumes.Num() > 0)
	{
		return AllVolumes[0];
	}

	return nullptr;
}

TArray<AAeonixBoundingVolume*> AAeonixBatchTestActor::GetAllVolumes() const
{
	TArray<AAeonixBoundingVolume*> Volumes;

	if (GetWorld())
	{
		for (TActorIterator<AAeonixBoundingVolume> ActorItr(GetWorld()); ActorItr; ++ActorItr)
		{
			AAeonixBoundingVolume* Volume = *ActorItr;
			if (Volume && Volume->HasData())
			{
				Volumes.Add(Volume);
			}
		}
	}

	return Volumes;
}


void AAeonixBatchTestActor::RunTestInEditor()
{
	InitializeSubsystemIfNeeded();

	if (CurrentStatus == EAeonixPerformanceTestStatus::Running)
	{
		UE_LOG(LogTemp, Warning, TEXT("Performance test is already running"));
		return;
	}

	if (!AeonixSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("AeonixSubsystem not available in editor context"));
		return;
	}

	AAeonixBoundingVolume* TargetVolume = GetTargetVolume();
	if (!TargetVolume)
	{
		UE_LOG(LogTemp, Error, TEXT("No valid target volume found for performance testing"));
		return;
	}

	if (!TargetVolume->HasData())
	{
		UE_LOG(LogTemp, Error, TEXT("Target volume has no navigation data"));
		return;
	}

	CurrentStatus = EAeonixPerformanceTestStatus::Running;
	CurrentTestIndex = 0;
	CurrentResults.Empty();
	CurrentResults.Reserve(TestSettings.NumberOfTests);
	TestStartTime = FPlatformTime::Seconds();

	ClearVisualization();

	UE_LOG(LogTemp, Log, TEXT("Starting editor performance test with %d iterations"), TestSettings.NumberOfTests);

	RunSynchronousTests();
}

void AAeonixBatchTestActor::ClearResultsInEditor()
{
	ClearResults();
	UE_LOG(LogTemp, Log, TEXT("Editor performance test results cleared"));
}


void AAeonixBatchTestActor::InitializeSubsystemIfNeeded()
{
	if (!AeonixSubsystem && GetWorld())
	{
		AeonixSubsystem = GetWorld()->GetSubsystem<UAeonixSubsystem>();
		if (!AeonixSubsystem)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to get AeonixSubsystem in editor context"));
		}
		else if (NavAgentComponent)
		{
			// Register the nav component so it has a valid nav volume
			AeonixSubsystem->RegisterNavComponent(NavAgentComponent, EAeonixMassEntityFlag::Enabled);
		}
	}
}