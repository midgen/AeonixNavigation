#pragma once

#include "HAL/ThreadSafeCounter.h"
#include "Data/AeonixThreading.h"
#include "Pathfinding/AeonixNavigationPath.h"

#include "AeonixTypes.generated.h"

class AAeonixBoundingVolume;
class UAeonixNavAgentComponent;

UENUM()
enum class EAeonixPathFindStatus : uint8
{
	Idle = 0,
	Initialized = 1,
	InProgress = 2,
	Complete = 3,
	Consumed = 4,
	Failed = 5,
	Cancelled = 6,
	Invalidated = 7
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FAeonixPathFindRequestCompleteDelegate, EAeonixPathFindStatus, PathFindStatus);

struct FAeonixPathFindRequest
{
	TPromise<EAeonixPathFindStatus> PathFindPromise;
	TFuture<EAeonixPathFindStatus> PathFindFuture = PathFindPromise.GetFuture();

	FAeonixPathFindRequestCompleteDelegate OnPathFindRequestComplete{};

	// Threading enhancements
	EAeonixRequestPriority Priority = EAeonixRequestPriority::Normal;
	double SubmitTime = 0.0;
	std::atomic<bool> bCancelled{false};
	std::atomic<bool> bAgentInvalidated{false};  // Set by game thread when agent is unregistered
	TWeakObjectPtr<UAeonixNavAgentComponent> RequestingAgent;
	TMap<FGuid, uint32> RegionVersionSnapshot; // For invalidation detection

	// Deferred delivery: workers write to WorkerPath, game thread moves to DestinationPath
	FAeonixNavigationPath WorkerPath;
	FAeonixNavigationPath* DestinationPath = nullptr;
	std::atomic<bool> bPathReady{false};  // Set by worker when path is complete

	/** Thread-safe staleness check (NO UObject access - safe from worker threads) */
	bool IsStale() const
	{
		return bCancelled.load(std::memory_order_acquire) ||
		       bAgentInvalidated.load(std::memory_order_acquire);
	}

	/** Comparison operator for priority queue sorting */
	bool operator<(const FAeonixPathFindRequest& Other) const
	{
		// Higher priority first (lower enum value = higher priority)
		if (Priority != Other.Priority)
		{
			return Priority < Other.Priority;
		}
		// Within same priority, FIFO (earlier submit time first)
		return SubmitTime < Other.SubmitTime;
	}
};
