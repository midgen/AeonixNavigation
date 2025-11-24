#include "Data/AeonixData.h"
#include "Data/AeonixGenerationParameters.h"
#include "Data/AeonixLink.h"
#include "Data/AeonixNode.h"
#include "Data/AeonixOctreeData.h"
#include "Data/AeonixLeafNode.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Interface/AeonixCollisionQueryInterface.h"
#include "Interface/AeonixDebugDrawInterface.h"
#include "Pathfinding/AeonixPathFinder.h"
#include "Pathfinding/AeonixNavigationPath.h"
#include "Misc/AutomationTest.h"
#include "AeonixNavigation.h"

// Mock collision interface with a half-volume blocked region
class FDynamicRegionMockCollision : public IAeonixCollisionQueryInterface
{
public:
	FBox DynamicRegion;

	FDynamicRegionMockCollision(const FBox& InDynamicRegion)
		: DynamicRegion(InDynamicRegion)
	{
	}

	virtual bool IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
	{
		// Create some obstacles to make pathfinding more interesting
		// Add a few scattered blocked regions
		if (FMath::Abs(Position.X) < 500.0f && FMath::Abs(Position.Y) < 500.0f && Position.Z < 0.0f)
		{
			return true; // Blocked floor in center
		}

		// Add some vertical pillars
		if ((FMath::Abs(Position.X - 1000.0f) < 200.0f && FMath::Abs(Position.Y - 1000.0f) < 200.0f) ||
		    (FMath::Abs(Position.X + 1000.0f) < 200.0f && FMath::Abs(Position.Y + 1000.0f) < 200.0f))
		{
			return true;
		}

		return false;
	}

	virtual bool IsLeafBlocked(const FVector& Position, const float LeafSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
	{
		return IsBlocked(Position, LeafSize, CollisionChannel, AgentRadius);
	}
};

// Mock debug interface (silent)
class FSilentDebugDrawInterface : public IAeonixDebugDrawInterface
{
public:
	virtual void AeonixDrawDebugString(const FVector& Position, const FString& String, const FColor& Color) const override {}
	virtual void AeonixDrawDebugBox(const FVector& Position, const float Size, const FColor& Color) const override {}
	virtual void AeonixDrawDebugLine(const FVector& Start, const FVector& End, const FColor& Color, float Thickness = 0.0f) const override {}
	virtual void AeonixDrawDebugDirectionalArrow(const FVector& Start, const FVector& End, const FColor& Color, float ArrowSize = 0.0f) const override {}
};

// Helper function to get a link from a position (simplified from AeonixMediator::GetLinkFromPosition)
static bool GetLinkFromPosition(const FVector& Position, const FAeonixData& NavData, AeonixLink& OutLink)
{
	const FAeonixGenerationParameters& Params = NavData.GetParams();
	const FVector& Origin = Params.Origin;
	const FVector& Extent = Params.Extents;

	// Check if position is within bounds
	const FBox Bounds(Origin - Extent, Origin + Extent);
	if (!Bounds.IsInside(Position))
	{
		return false;
	}

	// Z-order origin (where code == 0)
	const FVector ZOrigin = Origin - Extent;
	const FVector LocalPos = Position - ZOrigin;

	// Start from top layer and descend
	int32 LayerIndex = NavData.OctreeData.GetNumLayers() - 1;
	nodeindex_t NodeIndex = 0;

	while (LayerIndex >= 0 && LayerIndex < NavData.OctreeData.GetNumLayers())
	{
		const TArray<AeonixNode>& Layer = NavData.OctreeData.GetLayer(LayerIndex);
		const float VoxelSize = NavData.GetVoxelSize(LayerIndex);

		// Calculate XYZ coordinates
		const int32 X = FMath::FloorToInt(LocalPos.X / VoxelSize);
		const int32 Y = FMath::FloorToInt(LocalPos.Y / VoxelSize);
		const int32 Z = FMath::FloorToInt(LocalPos.Z / VoxelSize);
		const mortoncode_t Code = morton3D_64_encode(X, Y, Z);

		// Find node with this code
		for (nodeindex_t j = NodeIndex; j < Layer.Num(); j++)
		{
			const AeonixNode& Node = Layer[j];
			if (Node.Code == Code)
			{
				// No children - this is the link
				if (!Node.FirstChild.IsValid())
				{
					OutLink.LayerIndex = LayerIndex;
					OutLink.NodeIndex = j;
					OutLink.SubnodeIndex = 0;
					return true;
				}

				// Leaf node - find subnode
				if (LayerIndex == 0)
				{
					const AeonixLeafNode& Leaf = NavData.OctreeData.GetLeafNode(Node.FirstChild.NodeIndex);

					// Get node world position
					FVector NodePosition;
					NavData.GetNodePosition(LayerIndex, Node.Code, NodePosition);
					const FVector NodeOrigin = NodePosition - FVector(VoxelSize * 0.5f);
					const FVector NodeLocalPos = Position - NodeOrigin;

					// Calculate leaf voxel coordinates
					const int32 LeafX = FMath::FloorToInt(NodeLocalPos.X / (VoxelSize * 0.25f));
					const int32 LeafY = FMath::FloorToInt(NodeLocalPos.Y / (VoxelSize * 0.25f));
					const int32 LeafZ = FMath::FloorToInt(NodeLocalPos.Z / (VoxelSize * 0.25f));
					const mortoncode_t LeafIndex = morton3D_64_encode(LeafX, LeafY, LeafZ);

					// Check if blocked
					if (Leaf.GetNode(LeafIndex))
					{
						return false; // Blocked
					}

					OutLink.LayerIndex = 0;
					OutLink.NodeIndex = j;
					OutLink.SubnodeIndex = LeafIndex;
					return true;
				}

				// Has children - descend
				LayerIndex = Layer[j].FirstChild.GetLayerIndex();
				NodeIndex = Layer[j].FirstChild.GetNodeIndex();
				break;
			}
		}
	}

	return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAeonixNavigation_DynamicRegionStressTest,
	"AeonixNavigation.DynamicRegion.StressTest1000Paths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAeonixNavigation_DynamicRegionStressTest::RunTest(const FString& Parameters)
{
	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("Starting Dynamic Region Stress Test"));
	UE_LOG(LogTemp, Display, TEXT("========================================"));

	// Setup test environment
	const FVector TestOrigin = FVector::ZeroVector;
	const FVector TestExtents = FVector(5000.0f, 5000.0f, 5000.0f);
	const int32 VoxelPower = 5; 

	// Dynamic region covers half the volume (left half)
	const FBox DynamicRegion(
		TestOrigin - TestExtents,
		TestOrigin + FVector(0.0f, TestExtents.Y, TestExtents.Z)
	);

	UE_LOG(LogTemp, Display, TEXT("Test Setup:"));
	UE_LOG(LogTemp, Display, TEXT("  Origin: %s"), *TestOrigin.ToString());
	UE_LOG(LogTemp, Display, TEXT("  Extents: %s"), *TestExtents.ToString());
	UE_LOG(LogTemp, Display, TEXT("  VoxelPower: %d"), VoxelPower);
	UE_LOG(LogTemp, Display, TEXT("  Dynamic Region: Min=%s, Max=%s"), *DynamicRegion.Min.ToString(), *DynamicRegion.Max.ToString());

	// Create mock interfaces
	FDynamicRegionMockCollision MockCollision(DynamicRegion);
	FSilentDebugDrawInterface MockDebug;

	// Setup navigation data
	FAeonixData NavData;
	FAeonixGenerationParameters Params;
	Params.Origin = TestOrigin;
	Params.Extents = TestExtents;
	Params.OctreeDepth = VoxelPower;
	Params.CollisionChannel = ECollisionChannel::ECC_WorldStatic;
	Params.AgentRadius = 34.0f;

	// Add dynamic region
	FGuid DynamicRegionGuid = FGuid::NewGuid();
	Params.DynamicRegionBoxes.Add(DynamicRegionGuid, DynamicRegion);

	NavData.UpdateGenerationParameters(Params);

	// Generate navigation
	UE_LOG(LogTemp, Display, TEXT("Generating initial navigation data..."));
	UWorld* DummyWorld = nullptr;
	NavData.Generate(*DummyWorld, MockCollision, MockDebug);

	UE_LOG(LogTemp, Display, TEXT("Navigation data generated. Total leaf nodes: %d"), NavData.OctreeData.LeafNodes.Num());

	// Regenerate dynamic region to simulate the problem scenario
	UE_LOG(LogTemp, Display, TEXT("Regenerating dynamic region..."));
	NavData.RegenerateDynamicSubregions(MockCollision, MockDebug);
	UE_LOG(LogTemp, Display, TEXT("Dynamic region regenerated."));

	// Setup pathfinder
	FAeonixPathFinderSettings PathSettings;
	PathSettings.MaxIterations = 5001;
	PathSettings.bUseUnitCost = false;

	AeonixPathFinder PathFinder(NavData, PathSettings);

	// Statistics tracking
	int32 TotalTests = 1000;
	int32 SuccessCount = 0;
	int32 FailureCount = 0;
	int32 TotalIterations = 0;
	int32 MaxIterations = 0;
	int32 MinIterations = INT_MAX;
	TArray<int32> FailureIterations;
	TArray<float> FailureDistances;
	TArray<float> SuccessDistances;

	UE_LOG(LogTemp, Display, TEXT(""));
	UE_LOG(LogTemp, Display, TEXT("Running %d random pathfinding tests..."), TotalTests);
	UE_LOG(LogTemp, Display, TEXT("========================================"));

	// Run random pathfinding tests
	for (int32 TestIndex = 0; TestIndex < TotalTests; ++TestIndex)
	{
		// Generate random start and end positions within the navigation bounds
		// Retry until we find valid positions (avoids skipping tests)
		FVector StartPos, EndPos;
		AeonixLink StartLink, EndLink;
		bool bFoundValidPositions = false;

		const int32 MaxRetries = 50;
		for (int32 RetryCount = 0; RetryCount < MaxRetries && !bFoundValidPositions; ++RetryCount)
		{
			// 50% of tests should cross the boundary (start in static, end in dynamic or vice versa)
			if (TestIndex % 2 == 0)
			{
				// Start in static region (right half), end in dynamic region (left half)
				StartPos = FVector(
					FMath::FRandRange(0.0f, TestExtents.X * 0.9f),
					FMath::FRandRange(-TestExtents.Y * 0.9f, TestExtents.Y * 0.9f),
					FMath::FRandRange(-TestExtents.Z * 0.9f, TestExtents.Z * 0.9f)
				);
				EndPos = FVector(
					FMath::FRandRange(-TestExtents.X * 0.9f, 0.0f),
					FMath::FRandRange(-TestExtents.Y * 0.9f, TestExtents.Y * 0.9f),
					FMath::FRandRange(-TestExtents.Z * 0.9f, TestExtents.Z * 0.9f)
				);
			}
			else
			{
				// Start in dynamic region (left half), end in static region (right half)
				StartPos = FVector(
					FMath::FRandRange(-TestExtents.X * 0.9f, 0.0f),
					FMath::FRandRange(-TestExtents.Y * 0.9f, TestExtents.Y * 0.9f),
					FMath::FRandRange(-TestExtents.Z * 0.9f, TestExtents.Z * 0.9f)
				);
				EndPos = FVector(
					FMath::FRandRange(0.0f, TestExtents.X * 0.9f),
					FMath::FRandRange(-TestExtents.Y * 0.9f, TestExtents.Y * 0.9f),
					FMath::FRandRange(-TestExtents.Z * 0.9f, TestExtents.Z * 0.9f)
				);
			}

			// Get links for start and end positions
			bool bStartValid = GetLinkFromPosition(StartPos, NavData, StartLink);
			bool bEndValid = GetLinkFromPosition(EndPos, NavData, EndLink);

			if (bStartValid && bEndValid)
			{
				bFoundValidPositions = true;
			}
		}

		// If we couldn't find valid positions after max retries, skip this test
		if (!bFoundValidPositions)
		{
			UE_LOG(LogTemp, Warning, TEXT("Test %d: Could not find valid positions after %d retries, skipping"), TestIndex, MaxRetries);
			continue;
		}

		// Run pathfinding
		FAeonixNavigationPath Path;
		bool bPathFound = PathFinder.FindPath(StartLink, EndLink, StartPos, EndPos, Path, nullptr);

		const int32 Iterations = PathFinder.GetLastIterationCount();
		const float Distance = FVector::Dist(StartPos, EndPos);

		if (bPathFound)
		{
			SuccessCount++;
			TotalIterations += Iterations;
			MaxIterations = FMath::Max(MaxIterations, Iterations);
			MinIterations = FMath::Min(MinIterations, Iterations);
			SuccessDistances.Add(Distance);

			UE_LOG(LogTemp, Display, TEXT("Test %4d: SUCCESS - Distance: %7.2f units, Iterations: %4d"),
				TestIndex, Distance, Iterations);
		}
		else
		{
			FailureCount++;
			FailureIterations.Add(Iterations);
			FailureDistances.Add(Distance);

			UE_LOG(LogTemp, Error, TEXT("Test %4d: FAILURE - Distance: %7.2f units, Iterations: %4d, Start: %s, End: %s"),
				TestIndex, Distance, Iterations, *StartPos.ToString(), *EndPos.ToString());
		}
	}

	// Print summary statistics
	UE_LOG(LogTemp, Display, TEXT(""));
	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("Test Results Summary"));
	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("Total Tests:     %d"), TotalTests);
	UE_LOG(LogTemp, Display, TEXT("Successes:       %d (%.1f%%)"), SuccessCount, (SuccessCount * 100.0f) / TotalTests);
	UE_LOG(LogTemp, Display, TEXT("Failures:        %d (%.1f%%)"), FailureCount, (FailureCount * 100.0f) / TotalTests);

	if (SuccessCount > 0)
	{
		const float AvgIterations = static_cast<float>(TotalIterations) / SuccessCount;

		float AvgSuccessDistance = 0.0f;
		float MinSuccessDistance = FLT_MAX;
		float MaxSuccessDistance = 0.0f;
		for (float Dist : SuccessDistances)
		{
			AvgSuccessDistance += Dist;
			MinSuccessDistance = FMath::Min(MinSuccessDistance, Dist);
			MaxSuccessDistance = FMath::Max(MaxSuccessDistance, Dist);
		}
		AvgSuccessDistance /= SuccessCount;

		UE_LOG(LogTemp, Display, TEXT(""));
		UE_LOG(LogTemp, Display, TEXT("Success Statistics:"));
		UE_LOG(LogTemp, Display, TEXT("  Avg Iterations:  %.1f"), AvgIterations);
		UE_LOG(LogTemp, Display, TEXT("  Min Iterations:  %d"), MinIterations);
		UE_LOG(LogTemp, Display, TEXT("  Max Iterations:  %d"), MaxIterations);
		UE_LOG(LogTemp, Display, TEXT("  Avg Distance:    %.2f units"), AvgSuccessDistance);
		UE_LOG(LogTemp, Display, TEXT("  Min Distance:    %.2f units"), MinSuccessDistance);
		UE_LOG(LogTemp, Display, TEXT("  Max Distance:    %.2f units"), MaxSuccessDistance);
		UE_LOG(LogTemp, Display, TEXT("  Total Iterations: %d"), TotalIterations);
	}

	if (FailureCount > 0)
	{
		UE_LOG(LogTemp, Display, TEXT(""));
		UE_LOG(LogTemp, Display, TEXT("Failure Statistics:"));

		float AvgFailureDistance = 0.0f;
		for (float Dist : FailureDistances)
		{
			AvgFailureDistance += Dist;
		}
		AvgFailureDistance /= FailureCount;

		UE_LOG(LogTemp, Display, TEXT("  Avg Distance:    %.2f units"), AvgFailureDistance);
		UE_LOG(LogTemp, Display, TEXT("  Failures hit iteration limit: %d"),
			FailureIterations.FilterByPredicate([&](int32 Iter) { return Iter >= PathSettings.MaxIterations; }).Num());
	}

	UE_LOG(LogTemp, Display, TEXT("========================================"));

	// Output statistics to test results (visible in test summary)
	AddInfo(FString::Printf(TEXT("Ran %d pathfinding tests: %d successes (%.1f%%), %d failures (%.1f%%)"),
		TotalTests, SuccessCount, (SuccessCount * 100.0f) / TotalTests, FailureCount, (FailureCount * 100.0f) / TotalTests));

	if (SuccessCount > 0)
	{
		const float AvgIterations = static_cast<float>(TotalIterations) / SuccessCount;
		AddInfo(FString::Printf(TEXT("Success stats: Avg=%.1f, Min=%d, Max=%d iterations, Total=%d iterations"),
			AvgIterations, MinIterations, MaxIterations, TotalIterations));

		float AvgSuccessDistance = 0.0f;
		for (float Dist : SuccessDistances)
		{
			AvgSuccessDistance += Dist;
		}
		AvgSuccessDistance /= SuccessCount;

		AddInfo(FString::Printf(TEXT("Distance stats: Avg=%.1f units across all successful paths"), AvgSuccessDistance));
	}

	// Test passes if we have at least some successes and can identify the pattern
	const bool bTestPassed = (SuccessCount > 0);

	if (!bTestPassed)
	{
		AddError(FString::Printf(TEXT("All %d pathfinding tests failed! Dynamic region pathfinding is completely broken."), TotalTests));
	}
	else if (FailureCount > TotalTests * 0.1f) // More than 10% failure rate
	{
		AddWarning(FString::Printf(TEXT("High failure rate: %d/%d (%.1f%%) tests failed"),
			FailureCount, TotalTests, (FailureCount * 100.0f) / TotalTests));
	}

	return bTestPassed;
}
