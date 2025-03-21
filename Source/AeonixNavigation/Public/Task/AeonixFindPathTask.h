#pragma once

#include <AeonixNavigation/Public/Data/AeonixLink.h>
#include <AeonixNavigation/Public/Data/AeonixTypes.h">
#include <AeonixNavigation/Public/Pathfinding/AeonixPathFinder.h>

#include <Runtime/Core/Public/Async/AsyncWork.h>
#include <Runtime/Core/Public/HAL/ThreadSafeBool.h>

struct FAeonixData;
struct AeonixPathFinderSettings;

class FAeonixFindPathTask : public FNonAbandonableTask
{
	friend class FAutoDeleteAsyncTask<FAeonixFindPathTask>;

public:
	FAeonixFindPathTask(const FAeonixData& Data, FAeonixPathFinderSettings& aSettings, UWorld* aWorld, const AeonixLink aStart, const AeonixLink aTarget, const FVector& aStartPos, const FVector& aTargetPos, FAeonixNavigationPath& oPath, FThreadSafeBool& aCompleteFlag)
		: NavigationData(Data)
		, myWorld(aWorld)
		, myStart(aStart)
		, myTarget(aTarget)
		, myStartPos(aStartPos)
		, myTargetPos(aTargetPos)
		, myPath(oPath)
		, mySettings(aSettings)
		, myCompleteFlag(aCompleteFlag)
	{
	}

protected:
	const FAeonixData& NavigationData;
	UWorld* myWorld;

	AeonixLink myStart;
	AeonixLink myTarget;
	FVector myStartPos;
	FVector myTargetPos;
	FAeonixNavigationPath& myPath;

	// TODO: const this and have the debug data elsewhere
	FAeonixPathFinderSettings mySettings;

	FThreadSafeBool& myCompleteFlag;

	void DoWork();

	// This next section of code needs to be here.  Not important as to why.

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAeonixFindPathTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};