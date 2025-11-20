// Copyright 2024 Chris Ashworth

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Queue.h"
#include "Misc/ScopeLock.h"

#include "AeonixThreading.generated.h"

class UAeonixSubsystem;

/**
 * Priority levels for pathfinding requests
 */
UENUM()
enum class EAeonixRequestPriority : uint8
{
	Critical = 0 UMETA(DisplayName = "Critical"),  // Player character, high-priority AI
	High = 1 UMETA(DisplayName = "High"),           // Important AI
	Normal = 2 UMETA(DisplayName = "Normal"),       // Regular AI
	Low = 3 UMETA(DisplayName = "Low")              // Background/speculative paths
};

/**
 * Load metrics for pathfinding and regeneration systems
 */
struct AEONIXNAVIGATION_API FAeonixLoadMetrics
{
	std::atomic<int32> PendingPathfinds{0};
	std::atomic<int32> ActivePathfinds{0};
	std::atomic<int32> PendingRegenRegions{0};
	std::atomic<int32> ActiveWriteLocks{0};
	std::atomic<int32> CompletedPathfindsTotal{0};
	std::atomic<int32> FailedPathfindsTotal{0};
	std::atomic<int32> CancelledPathfindsTotal{0};

	TAtomic<float> AveragePathfindTimeMs{0.0f};
	TAtomic<float> AverageRegenTimeMs{0.0f};

	// Running average calculation helpers
	std::atomic<int32> PathfindSampleCount{0};
	std::atomic<int32> RegenSampleCount{0};

	/** Check if system is under heavy load */
	bool ShouldThrottleNewRequests() const
	{
		return PendingPathfinds > 100 || ActiveWriteLocks > 0;
	}

	/** Get recommended delay before processing new requests */
	float GetRecommendedDelay() const
	{
		if (PendingPathfinds > 50) return 0.1f;  // High load
		if (PendingPathfinds > 20) return 0.05f; // Medium load
		return 0.0f; // Normal
	}

	/** Update average pathfind time with exponential moving average */
	void UpdatePathfindTime(float NewTimeMs)
	{
		const float Alpha = 0.1f; // Smoothing factor
		float CurrentAvg = AveragePathfindTimeMs.Load();
		float NewAvg = (Alpha * NewTimeMs) + ((1.0f - Alpha) * CurrentAvg);
		AveragePathfindTimeMs.Store(NewAvg);
		PathfindSampleCount.fetch_add(1);
	}

	/** Update average regen time with exponential moving average */
	void UpdateRegenTime(float NewTimeMs)
	{
		const float Alpha = 0.1f; // Smoothing factor
		float CurrentAvg = AverageRegenTimeMs.Load();
		float NewAvg = (Alpha * NewTimeMs) + ((1.0f - Alpha) * CurrentAvg);
		AverageRegenTimeMs.Store(NewAvg);
		RegenSampleCount.fetch_add(1);
	}

	/** Reset all counters */
	void Reset()
	{
		PendingPathfinds = 0;
		ActivePathfinds = 0;
		PendingRegenRegions = 0;
		ActiveWriteLocks = 0;
		CompletedPathfindsTotal = 0;
		FailedPathfindsTotal = 0;
		CancelledPathfindsTotal = 0;
		AveragePathfindTimeMs = 0.0f;
		AverageRegenTimeMs = 0.0f;
		PathfindSampleCount = 0;
		RegenSampleCount = 0;
	}
};

/**
 * Pathfinding worker thread that processes work from a queue
 */
class AEONIXNAVIGATION_API FAeonixPathfindWorker : public FRunnable
{
public:
	FAeonixPathfindWorker(
		TQueue<TFunction<void()>, EQueueMode::Mpsc>* InWorkQueue,
		FEvent* InWorkAvailableEvent,
		FThreadSafeBool* InShuttingDown,
		int32 InWorkerIndex);

	virtual ~FAeonixPathfindWorker();

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

private:
	TQueue<TFunction<void()>, EQueueMode::Mpsc>* WorkQueue;
	FEvent* WorkAvailableEvent;
	FThreadSafeBool* bShuttingDown;
	int32 WorkerIndex;
	FThreadSafeBool bStopRequested;
};

/**
 * Worker pool for pathfinding tasks
 */
class AEONIXNAVIGATION_API FAeonixPathfindWorkerPool
{
public:
	FAeonixPathfindWorkerPool();
	~FAeonixPathfindWorkerPool();

	/** Initialize the worker pool with a specific number of threads */
	void Initialize(int32 NumThreads = 4);

	/** Enqueue work to be processed by worker threads */
	void EnqueueWork(TFunction<void()>&& Work);

	/** Shutdown the worker pool */
	void Shutdown();

	/** Check if pool is initialized */
	bool IsInitialized() const { return bInitialized; }

	/** Get number of worker threads */
	int32 GetNumWorkers() const { return WorkerThreads.Num(); }

private:
	TArray<TUniquePtr<FRunnableThread>> WorkerThreads;
	TQueue<TFunction<void()>, EQueueMode::Mpsc> WorkQueue;
	FThreadSafeBool bShuttingDown;
	FEvent* WorkAvailableEvent;
	bool bInitialized;
};
