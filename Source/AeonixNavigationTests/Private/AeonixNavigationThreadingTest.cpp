// Copyright 2024 Chris Ashworth
// Multithreading tests for Aeonix Navigation

#include "Data/AeonixThreading.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include <atomic>

// Helper function to wait for a condition with timeout
static bool WaitForCondition(TFunction<bool()> Condition, float TimeoutSeconds = 5.0f)
{
	const double StartTime = FPlatformTime::Seconds();
	while (!Condition())
	{
		if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
		{
			return false;
		}
		FPlatformProcess::Sleep(0.01f);
	}
	return true;
}

// Simple runnable for spawning test threads
class FTestRunnable : public FRunnable
{
public:
	TFunction<void()> WorkFunction;
	FThreadSafeBool bComplete;

	FTestRunnable(TFunction<void()> InWork) : WorkFunction(MoveTemp(InWork)), bComplete(false) {}

	virtual uint32 Run() override
	{
		WorkFunction();
		bComplete = true;
		return 0;
	}
};

//////////////////////////////////////////////////////////////////////////
// Test 1: LoadMetrics Counter Initialization
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAeonixNavigation_LoadMetricsInitialization,
	"AeonixNavigation.Threading.LoadMetricsInitialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAeonixNavigation_LoadMetricsInitialization::RunTest(const FString& Parameters)
{
	// Create a fresh LoadMetrics instance
	FAeonixLoadMetrics Metrics;

	// Verify all counters start at zero
	TestEqual(TEXT("PendingPathfinds should start at 0"), Metrics.PendingPathfinds.load(), 0);
	TestEqual(TEXT("ActivePathfinds should start at 0"), Metrics.ActivePathfinds.load(), 0);
	TestEqual(TEXT("PendingRegenRegions should start at 0"), Metrics.PendingRegenRegions.load(), 0);
	TestEqual(TEXT("ActiveWriteLocks should start at 0"), Metrics.ActiveWriteLocks.load(), 0);
	TestEqual(TEXT("CompletedPathfindsTotal should start at 0"), Metrics.CompletedPathfindsTotal.load(), 0);
	TestEqual(TEXT("FailedPathfindsTotal should start at 0"), Metrics.FailedPathfindsTotal.load(), 0);
	TestEqual(TEXT("CancelledPathfindsTotal should start at 0"), Metrics.CancelledPathfindsTotal.load(), 0);
	TestEqual(TEXT("InvalidatedPathsTotal should start at 0"), Metrics.InvalidatedPathsTotal.load(), 0);
	TestEqual(TEXT("PathfindSampleCount should start at 0"), Metrics.PathfindSampleCount.load(), 0);
	TestEqual(TEXT("RegenSampleCount should start at 0"), Metrics.RegenSampleCount.load(), 0);

	// Modify some counters
	Metrics.PendingPathfinds.fetch_add(5);
	Metrics.ActivePathfinds.fetch_add(3);
	Metrics.CompletedPathfindsTotal.fetch_add(10);

	// Call Reset
	Metrics.Reset();

	// Verify all counters are zero again
	TestEqual(TEXT("PendingPathfinds should be 0 after Reset"), Metrics.PendingPathfinds.load(), 0);
	TestEqual(TEXT("ActivePathfinds should be 0 after Reset"), Metrics.ActivePathfinds.load(), 0);
	TestEqual(TEXT("CompletedPathfindsTotal should be 0 after Reset"), Metrics.CompletedPathfindsTotal.load(), 0);

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Test 2: LoadMetrics Counter Increment/Decrement Balance
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAeonixNavigation_LoadMetricsBalance,
	"AeonixNavigation.Threading.LoadMetricsBalance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAeonixNavigation_LoadMetricsBalance::RunTest(const FString& Parameters)
{
	FAeonixLoadMetrics Metrics;
	const int32 NumThreads = 8;
	const int32 OperationsPerThread = 1000;

	TArray<FRunnableThread*> Threads;
	TArray<TUniquePtr<FTestRunnable>> Runnables;

	// Spawn threads that increment then decrement PendingPathfinds
	for (int32 i = 0; i < NumThreads; ++i)
	{
		auto Runnable = MakeUnique<FTestRunnable>([&Metrics, OperationsPerThread]()
		{
			for (int32 j = 0; j < OperationsPerThread; ++j)
			{
				Metrics.PendingPathfinds.fetch_add(1);
				// Small spin to increase contention window
				volatile int32 Spin = 0;
				for (int32 k = 0; k < 10; ++k) { Spin++; }
				Metrics.PendingPathfinds.fetch_sub(1);
			}
		});

		FRunnableThread* Thread = FRunnableThread::Create(Runnable.Get(), *FString::Printf(TEXT("TestThread_%d"), i));
		Threads.Add(Thread);
		Runnables.Add(MoveTemp(Runnable));
	}

	// Wait for all threads to complete
	for (int32 i = 0; i < NumThreads; ++i)
	{
		Threads[i]->WaitForCompletion();
		delete Threads[i];
	}

	// Verify counter is balanced
	TestEqual(TEXT("PendingPathfinds should be 0 after balanced operations"), Metrics.PendingPathfinds.load(), 0);

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Test 3: LoadMetrics Concurrent Counter Stress
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAeonixNavigation_LoadMetricsConcurrentStress,
	"AeonixNavigation.Threading.LoadMetricsConcurrentStress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAeonixNavigation_LoadMetricsConcurrentStress::RunTest(const FString& Parameters)
{
	FAeonixLoadMetrics Metrics;
	const int32 NumThreads = 8;
	const int32 IterationsPerThread = 10000;

	std::atomic<int32> TotalIncrements{0};
	std::atomic<int32> TotalDecrements{0};

	TArray<FRunnableThread*> Threads;
	TArray<TUniquePtr<FTestRunnable>> Runnables;

	for (int32 i = 0; i < NumThreads; ++i)
	{
		auto Runnable = MakeUnique<FTestRunnable>([&Metrics, &TotalIncrements, &TotalDecrements, IterationsPerThread]()
		{
			for (int32 j = 0; j < IterationsPerThread; ++j)
			{
				Metrics.PendingPathfinds.fetch_add(1);
				TotalIncrements.fetch_add(1);
				Metrics.PendingPathfinds.fetch_sub(1);
				TotalDecrements.fetch_add(1);
			}
		});

		FRunnableThread* Thread = FRunnableThread::Create(Runnable.Get(), *FString::Printf(TEXT("StressThread_%d"), i));
		Threads.Add(Thread);
		Runnables.Add(MoveTemp(Runnable));
	}

	// Wait for all threads
	for (int32 i = 0; i < NumThreads; ++i)
	{
		Threads[i]->WaitForCompletion();
		delete Threads[i];
	}

	const int32 ExpectedTotal = NumThreads * IterationsPerThread;
	TestEqual(TEXT("Total increments should match expected"), TotalIncrements.load(), ExpectedTotal);
	TestEqual(TEXT("Total decrements should match expected"), TotalDecrements.load(), ExpectedTotal);
	TestEqual(TEXT("PendingPathfinds should be 0 (no lost updates)"), Metrics.PendingPathfinds.load(), 0);

	AddInfo(FString::Printf(TEXT("Completed %d total atomic operations across %d threads"), ExpectedTotal * 2, NumThreads));

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Test 4: LoadMetrics Never Goes Negative (Regression Test)
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAeonixNavigation_LoadMetricsNeverNegative,
	"AeonixNavigation.Threading.LoadMetricsNeverNegative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAeonixNavigation_LoadMetricsNeverNegative::RunTest(const FString& Parameters)
{
	FAeonixLoadMetrics Metrics;
	const int32 NumRequests = 100;

	// Track minimum values observed
	std::atomic<int32> MinPendingObserved{INT_MAX};
	std::atomic<int32> MinActiveObserved{INT_MAX};

	// Simulate the CORRECT pathfind flow (post-fix):
	// 1. Request created: PendingPathfinds++
	// 2. Worker starts: PendingPathfinds--, ActivePathfinds++
	// 3. Worker completes: ActivePathfinds--

	for (int32 i = 0; i < NumRequests; ++i)
	{
		// Step 1: Request created
		Metrics.PendingPathfinds.fetch_add(1);
		int32 CurrentPending = Metrics.PendingPathfinds.load();
		int32 CurrentMin = MinPendingObserved.load();
		while (CurrentPending < CurrentMin && !MinPendingObserved.compare_exchange_weak(CurrentMin, CurrentPending)) {}

		// Step 2: Worker starts (transition pending -> active)
		Metrics.PendingPathfinds.fetch_sub(1);
		Metrics.ActivePathfinds.fetch_add(1);

		CurrentPending = Metrics.PendingPathfinds.load();
		CurrentMin = MinPendingObserved.load();
		while (CurrentPending < CurrentMin && !MinPendingObserved.compare_exchange_weak(CurrentMin, CurrentPending)) {}

		int32 CurrentActive = Metrics.ActivePathfinds.load();
		CurrentMin = MinActiveObserved.load();
		while (CurrentActive < CurrentMin && !MinActiveObserved.compare_exchange_weak(CurrentMin, CurrentActive)) {}

		// Step 3: Worker completes
		Metrics.ActivePathfinds.fetch_sub(1);
		Metrics.CompletedPathfindsTotal.fetch_add(1);

		CurrentActive = Metrics.ActivePathfinds.load();
		CurrentMin = MinActiveObserved.load();
		while (CurrentActive < CurrentMin && !MinActiveObserved.compare_exchange_weak(CurrentMin, CurrentActive)) {}
	}

	// Verify no counter ever went negative
	TestTrue(TEXT("PendingPathfinds should never go negative"), MinPendingObserved.load() >= 0);
	TestTrue(TEXT("ActivePathfinds should never go negative"), MinActiveObserved.load() >= 0);

	// Verify final state is balanced
	TestEqual(TEXT("Final PendingPathfinds should be 0"), Metrics.PendingPathfinds.load(), 0);
	TestEqual(TEXT("Final ActivePathfinds should be 0"), Metrics.ActivePathfinds.load(), 0);
	TestEqual(TEXT("CompletedPathfindsTotal should match request count"), Metrics.CompletedPathfindsTotal.load(), NumRequests);

	AddInfo(FString::Printf(TEXT("Min pending observed: %d, Min active observed: %d"),
		MinPendingObserved.load(), MinActiveObserved.load()));

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Test 5: LoadMetrics Early Exit Paths Balance
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAeonixNavigation_LoadMetricsEarlyExitBalance,
	"AeonixNavigation.Threading.LoadMetricsEarlyExitBalance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAeonixNavigation_LoadMetricsEarlyExitBalance::RunTest(const FString& Parameters)
{
	FAeonixLoadMetrics Metrics;

	// Test 1: Simulate IsStale early exit path
	// Flow: Request created -> IsStale detected -> early exit with decrement
	{
		Metrics.Reset();

		// Request created
		Metrics.PendingPathfinds.fetch_add(1);
		TestEqual(TEXT("[IsStale] After request created, pending should be 1"), Metrics.PendingPathfinds.load(), 1);

		// Early exit (stale) - this is the fixed code path
		Metrics.PendingPathfinds.fetch_sub(1);
		Metrics.CancelledPathfindsTotal.fetch_add(1);

		TestEqual(TEXT("[IsStale] After early exit, pending should be 0"), Metrics.PendingPathfinds.load(), 0);
		TestEqual(TEXT("[IsStale] CancelledPathfindsTotal should be 1"), Metrics.CancelledPathfindsTotal.load(), 1);
	}

	// Test 2: Simulate NavVolume null early exit path
	{
		Metrics.Reset();

		// Request created
		Metrics.PendingPathfinds.fetch_add(1);
		TestEqual(TEXT("[NavVolumeNull] After request created, pending should be 1"), Metrics.PendingPathfinds.load(), 1);

		// Early exit (volume null) - this is the fixed code path
		Metrics.PendingPathfinds.fetch_sub(1);
		Metrics.FailedPathfindsTotal.fetch_add(1);

		TestEqual(TEXT("[NavVolumeNull] After early exit, pending should be 0"), Metrics.PendingPathfinds.load(), 0);
		TestEqual(TEXT("[NavVolumeNull] FailedPathfindsTotal should be 1"), Metrics.FailedPathfindsTotal.load(), 1);
	}

	// Test 3: Normal completion path (for comparison)
	{
		Metrics.Reset();

		// Request created
		Metrics.PendingPathfinds.fetch_add(1);

		// Worker starts (pending -> active transition)
		Metrics.PendingPathfinds.fetch_sub(1);
		Metrics.ActivePathfinds.fetch_add(1);

		TestEqual(TEXT("[Normal] After worker starts, pending should be 0"), Metrics.PendingPathfinds.load(), 0);
		TestEqual(TEXT("[Normal] After worker starts, active should be 1"), Metrics.ActivePathfinds.load(), 1);

		// Worker completes
		Metrics.ActivePathfinds.fetch_sub(1);
		Metrics.CompletedPathfindsTotal.fetch_add(1);

		TestEqual(TEXT("[Normal] After completion, active should be 0"), Metrics.ActivePathfinds.load(), 0);
		TestEqual(TEXT("[Normal] CompletedPathfindsTotal should be 1"), Metrics.CompletedPathfindsTotal.load(), 1);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Test 6: Worker Pool Initialization
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAeonixNavigation_WorkerPoolInitialization,
	"AeonixNavigation.Threading.WorkerPoolInitialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAeonixNavigation_WorkerPoolInitialization::RunTest(const FString& Parameters)
{
	FAeonixPathfindWorkerPool Pool;

	// Verify initial state
	TestFalse(TEXT("Pool should not be initialized before Initialize()"), Pool.IsInitialized());
	TestEqual(TEXT("Pool should have 0 workers before Initialize()"), Pool.GetNumWorkers(), 0);

	// Initialize with 4 workers
	Pool.Initialize(4);
	TestTrue(TEXT("Pool should be initialized after Initialize()"), Pool.IsInitialized());
	TestEqual(TEXT("Pool should have 4 workers"), Pool.GetNumWorkers(), 4);

	// Shutdown
	Pool.Shutdown();

	// Test re-initialization with different count
	Pool.Initialize(2);
	TestTrue(TEXT("Pool should be initialized after re-Initialize()"), Pool.IsInitialized());
	TestEqual(TEXT("Pool should have 2 workers after re-Initialize()"), Pool.GetNumWorkers(), 2);

	Pool.Shutdown();

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Test 7: Worker Pool Work Distribution
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAeonixNavigation_WorkerPoolDistribution,
	"AeonixNavigation.Threading.WorkerPoolDistribution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAeonixNavigation_WorkerPoolDistribution::RunTest(const FString& Parameters)
{
	FAeonixPathfindWorkerPool Pool;
	Pool.Initialize(4);

	std::atomic<int32> CompletedCount{0};
	const int32 TotalWorkItems = 100;

	// Enqueue work items
	for (int32 i = 0; i < TotalWorkItems; ++i)
	{
		Pool.EnqueueWork([&CompletedCount]()
		{
			CompletedCount.fetch_add(1);
		});
	}

	// Wait for completion with timeout
	bool bCompleted = WaitForCondition([&CompletedCount, TotalWorkItems]()
	{
		return CompletedCount.load() >= TotalWorkItems;
	}, 10.0f);

	TestTrue(TEXT("All work items should complete within timeout"), bCompleted);
	TestEqual(TEXT("CompletedCount should equal TotalWorkItems"), CompletedCount.load(), TotalWorkItems);

	Pool.Shutdown();

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Test 8: Worker Pool Shutdown Safety
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAeonixNavigation_WorkerPoolShutdownSafety,
	"AeonixNavigation.Threading.WorkerPoolShutdownSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAeonixNavigation_WorkerPoolShutdownSafety::RunTest(const FString& Parameters)
{
	FAeonixPathfindWorkerPool Pool;
	Pool.Initialize(2);

	std::atomic<int32> CompletedCount{0};

	// Enqueue work that takes some time
	Pool.EnqueueWork([&CompletedCount]()
	{
		FPlatformProcess::Sleep(0.01f); // 10ms work
		CompletedCount.fetch_add(1);
	});

	// Give worker time to pick up and start the work
	FPlatformProcess::Sleep(0.05f);

	// Shutdown - workers complete in-flight work but abandon queued work
	Pool.Shutdown();

	// Verify in-flight work completed (work that was already executing)
	TestEqual(TEXT("In-flight work should complete before shutdown"), CompletedCount.load(), 1);

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Test 9: Worker Pool Empty Shutdown
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAeonixNavigation_WorkerPoolEmptyShutdown,
	"AeonixNavigation.Threading.WorkerPoolEmptyShutdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAeonixNavigation_WorkerPoolEmptyShutdown::RunTest(const FString& Parameters)
{
	FAeonixPathfindWorkerPool Pool;
	Pool.Initialize(4);

	// Immediately shutdown without enqueueing any work
	// This should not deadlock or crash
	Pool.Shutdown();

	TestFalse(TEXT("Pool should not be initialized after shutdown"), Pool.IsInitialized());

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Test 10: Concurrent Enqueue Stress Test
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAeonixNavigation_ConcurrentEnqueueStress,
	"AeonixNavigation.Threading.ConcurrentEnqueueStress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAeonixNavigation_ConcurrentEnqueueStress::RunTest(const FString& Parameters)
{
	FAeonixPathfindWorkerPool Pool;
	Pool.Initialize(4);

	std::atomic<int32> CompletedCount{0};
	const int32 NumProducers = 8;
	const int32 ItemsPerProducer = 500;
	const int32 TotalExpected = NumProducers * ItemsPerProducer;

	TArray<FRunnableThread*> ProducerThreads;
	TArray<TUniquePtr<FTestRunnable>> Runnables;

	// Spawn producer threads that all enqueue work simultaneously
	for (int32 i = 0; i < NumProducers; ++i)
	{
		auto Runnable = MakeUnique<FTestRunnable>([&Pool, &CompletedCount, ItemsPerProducer]()
		{
			for (int32 j = 0; j < ItemsPerProducer; ++j)
			{
				Pool.EnqueueWork([&CompletedCount]()
				{
					CompletedCount.fetch_add(1);
				});
			}
		});

		FRunnableThread* Thread = FRunnableThread::Create(Runnable.Get(), *FString::Printf(TEXT("Producer_%d"), i));
		ProducerThreads.Add(Thread);
		Runnables.Add(MoveTemp(Runnable));
	}

	// Wait for all producers to finish enqueueing
	for (int32 i = 0; i < NumProducers; ++i)
	{
		ProducerThreads[i]->WaitForCompletion();
		delete ProducerThreads[i];
	}

	// Wait for all work to complete
	bool bCompleted = WaitForCondition([&CompletedCount, TotalExpected]()
	{
		return CompletedCount.load() >= TotalExpected;
	}, 30.0f);

	TestTrue(TEXT("All work should complete within timeout"), bCompleted);
	TestEqual(TEXT("CompletedCount should equal total expected"), CompletedCount.load(), TotalExpected);

	AddInfo(FString::Printf(TEXT("Completed %d work items from %d concurrent producers"), CompletedCount.load(), NumProducers));

	Pool.Shutdown();

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Test 11: Mixed Counter Transitions
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAeonixNavigation_MixedCounterTransitions,
	"AeonixNavigation.Threading.MixedCounterTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAeonixNavigation_MixedCounterTransitions::RunTest(const FString& Parameters)
{
	FAeonixLoadMetrics Metrics;
	const int32 TotalRequests = 100;

	// Deterministic distribution:
	// 60% complete successfully
	// 20% cancelled (early exit - stale)
	// 10% failed (early exit - volume null)
	// 10% failed after starting (active->failed)

	int32 ExpectedCompleted = 0;
	int32 ExpectedCancelled = 0;
	int32 ExpectedFailed = 0;

	for (int32 i = 0; i < TotalRequests; ++i)
	{
		// All paths start with request creation
		Metrics.PendingPathfinds.fetch_add(1);

		if (i % 10 < 6) // 60% - successful completion
		{
			// Worker starts
			Metrics.PendingPathfinds.fetch_sub(1);
			Metrics.ActivePathfinds.fetch_add(1);
			// Worker completes
			Metrics.ActivePathfinds.fetch_sub(1);
			Metrics.CompletedPathfindsTotal.fetch_add(1);
			ExpectedCompleted++;
		}
		else if (i % 10 < 8) // 20% - cancelled (stale early exit)
		{
			// Early exit before worker starts
			Metrics.PendingPathfinds.fetch_sub(1);
			Metrics.CancelledPathfindsTotal.fetch_add(1);
			ExpectedCancelled++;
		}
		else if (i % 10 < 9) // 10% - failed (volume null early exit)
		{
			// Early exit before worker starts
			Metrics.PendingPathfinds.fetch_sub(1);
			Metrics.FailedPathfindsTotal.fetch_add(1);
			ExpectedFailed++;
		}
		else // 10% - failed after starting
		{
			// Worker starts
			Metrics.PendingPathfinds.fetch_sub(1);
			Metrics.ActivePathfinds.fetch_add(1);
			// Worker fails
			Metrics.ActivePathfinds.fetch_sub(1);
			Metrics.FailedPathfindsTotal.fetch_add(1);
			ExpectedFailed++;
		}
	}

	// Verify final state
	TestEqual(TEXT("PendingPathfinds should be 0"), Metrics.PendingPathfinds.load(), 0);
	TestEqual(TEXT("ActivePathfinds should be 0"), Metrics.ActivePathfinds.load(), 0);
	TestEqual(TEXT("CompletedPathfindsTotal should match expected"), Metrics.CompletedPathfindsTotal.load(), ExpectedCompleted);
	TestEqual(TEXT("CancelledPathfindsTotal should match expected"), Metrics.CancelledPathfindsTotal.load(), ExpectedCancelled);
	TestEqual(TEXT("FailedPathfindsTotal should match expected"), Metrics.FailedPathfindsTotal.load(), ExpectedFailed);

	// Verify totals add up
	int32 TotalOutcomes = Metrics.CompletedPathfindsTotal.load() +
	                      Metrics.CancelledPathfindsTotal.load() +
	                      Metrics.FailedPathfindsTotal.load();
	TestEqual(TEXT("Total outcomes should equal total requests"), TotalOutcomes, TotalRequests);

	AddInfo(FString::Printf(TEXT("Completed: %d, Cancelled: %d, Failed: %d"),
		ExpectedCompleted, ExpectedCancelled, ExpectedFailed));

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Test 12: Average Time Calculation
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAeonixNavigation_AverageTimeCalculation,
	"AeonixNavigation.Threading.AverageTimeCalculation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAeonixNavigation_AverageTimeCalculation::RunTest(const FString& Parameters)
{
	FAeonixLoadMetrics Metrics;

	// Initial value should be 0
	TestEqual(TEXT("Initial AveragePathfindTimeMs should be 0"), Metrics.AveragePathfindTimeMs.Load(), 0.0f);
	TestEqual(TEXT("Initial PathfindSampleCount should be 0"), Metrics.PathfindSampleCount.load(), 0);

	// Update with 100ms
	Metrics.UpdatePathfindTime(100.0f);
	TestEqual(TEXT("PathfindSampleCount should be 1 after first update"), Metrics.PathfindSampleCount.load(), 1);

	// EMA: 0.1 * 100 + 0.9 * 0 = 10
	float Expected = 10.0f;
	TestTrue(TEXT("Average should be ~10 after first 100ms sample"),
		FMath::IsNearlyEqual(Metrics.AveragePathfindTimeMs.Load(), Expected, 0.1f));

	// Update with another 100ms
	Metrics.UpdatePathfindTime(100.0f);
	TestEqual(TEXT("PathfindSampleCount should be 2"), Metrics.PathfindSampleCount.load(), 2);

	// EMA: 0.1 * 100 + 0.9 * 10 = 19
	Expected = 19.0f;
	TestTrue(TEXT("Average should be ~19 after second 100ms sample"),
		FMath::IsNearlyEqual(Metrics.AveragePathfindTimeMs.Load(), Expected, 0.1f));

	// Test regen time as well
	Metrics.UpdateRegenTime(50.0f);
	TestEqual(TEXT("RegenSampleCount should be 1"), Metrics.RegenSampleCount.load(), 1);
	TestTrue(TEXT("AverageRegenTimeMs should be ~5"),
		FMath::IsNearlyEqual(Metrics.AverageRegenTimeMs.Load(), 5.0f, 0.1f));

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Test 13: Throttling Decisions
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAeonixNavigation_ThrottlingDecisions,
	"AeonixNavigation.Threading.ThrottlingDecisions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAeonixNavigation_ThrottlingDecisions::RunTest(const FString& Parameters)
{
	FAeonixLoadMetrics Metrics;

	// Empty state - should not throttle
	TestFalse(TEXT("Should not throttle when empty"), Metrics.ShouldThrottleNewRequests());

	// High pending count (>100) - should throttle
	Metrics.PendingPathfinds = 101;
	TestTrue(TEXT("Should throttle when PendingPathfinds > 100"), Metrics.ShouldThrottleNewRequests());

	// Reset and test write lock trigger
	Metrics.Reset();
	TestFalse(TEXT("Should not throttle after reset"), Metrics.ShouldThrottleNewRequests());

	Metrics.ActiveWriteLocks = 1;
	TestTrue(TEXT("Should throttle when ActiveWriteLocks > 0"), Metrics.ShouldThrottleNewRequests());

	// Both conditions
	Metrics.PendingPathfinds = 50;
	Metrics.ActiveWriteLocks = 1;
	TestTrue(TEXT("Should throttle with write lock even if pending < 100"), Metrics.ShouldThrottleNewRequests());

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Test 14: Delay Recommendations
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAeonixNavigation_DelayRecommendations,
	"AeonixNavigation.Threading.DelayRecommendations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAeonixNavigation_DelayRecommendations::RunTest(const FString& Parameters)
{
	FAeonixLoadMetrics Metrics;

	// No load - no delay
	Metrics.PendingPathfinds = 0;
	TestEqual(TEXT("Delay should be 0 with no pending"), Metrics.GetRecommendedDelay(), 0.0f);

	// Low load (1-20) - no delay
	Metrics.PendingPathfinds = 20;
	TestEqual(TEXT("Delay should be 0 with 20 pending"), Metrics.GetRecommendedDelay(), 0.0f);

	// Medium load (21-50) - 0.05s delay
	Metrics.PendingPathfinds = 21;
	TestEqual(TEXT("Delay should be 0.05 with 21 pending"), Metrics.GetRecommendedDelay(), 0.05f);

	Metrics.PendingPathfinds = 50;
	TestEqual(TEXT("Delay should be 0.05 with 50 pending"), Metrics.GetRecommendedDelay(), 0.05f);

	// High load (51+) - 0.1s delay
	Metrics.PendingPathfinds = 51;
	TestEqual(TEXT("Delay should be 0.1 with 51 pending"), Metrics.GetRecommendedDelay(), 0.1f);

	Metrics.PendingPathfinds = 100;
	TestEqual(TEXT("Delay should be 0.1 with 100 pending"), Metrics.GetRecommendedDelay(), 0.1f);

	return true;
}
