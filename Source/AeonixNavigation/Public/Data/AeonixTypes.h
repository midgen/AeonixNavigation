#pragma once

#include "HAL/ThreadSafeCounter.h"
#include "Data/AeonixThreading.h"

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
	TWeakObjectPtr<UAeonixNavAgentComponent> RequestingAgent;
	TMap<FGuid, uint32> RegionVersionSnapshot; // For invalidation detection

	/** Check if request is stale (cancelled or agent destroyed) */
	bool IsStale() const
	{
		return bCancelled || !RequestingAgent.IsValid();
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
