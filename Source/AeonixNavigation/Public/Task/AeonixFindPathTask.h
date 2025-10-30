#pragma once

#include "Data/AeonixLink.h"
#include "Pathfinding/AeonixPathFinder.h"
#include "Data/AeonixTypes.h"

#include "Async/AsyncWork.h"
#include "HAL/ThreadSafeBool.h"

struct FAeonixData;
struct AeonixPathFinderSettings;

class FAeonixFindPathTask : public FNonAbandonableTask
{
	friend class FAutoDeleteAsyncTask<FAeonixFindPathTask>;

public:
	FAeonixFindPathTask(const FAeonixData& Data, const FAeonixPathFinderSettings& aSettings, const AeonixLink aStart, const AeonixLink aGoal, const FVector& aStartPos, const FVector& aTargetPos, FAeonixNavigationPath& oPath, FAeonixPathFindRequest& aRequest)
		: NavigationData(Data)
		, Start(aStart)
		, Goal(aGoal)
		, StartPos(aStartPos)
		, TargetPos(aTargetPos)
		, Path(oPath)
		, Settings(aSettings)
		, Request(aRequest)
	{
	}

protected:
	const FAeonixData& NavigationData;

	AeonixLink Start;
	AeonixLink Goal;
	FVector StartPos;
	FVector TargetPos;
	FAeonixNavigationPath& Path;
	const FAeonixPathFinderSettings Settings;
	FAeonixPathFindRequest& Request;

	void DoWork();
	
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAeonixFindPathTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};