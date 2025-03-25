#pragma once

#include <Runtime/Core/Public/HAL/ThreadSafeCounter.h>

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
	Failed = 5
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FAeonixPathFindRequestCompleteDelegate, EAeonixPathFindStatus, PathFindStatus);

struct FAeonixPathFindRequest
{
	TPromise<EAeonixPathFindStatus> PathFindPromise;
	TFuture<EAeonixPathFindStatus> PathFindFuture = PathFindPromise.GetFuture();
	
	FAeonixPathFindRequestCompleteDelegate OnPathFindRequestComplete{};

};
