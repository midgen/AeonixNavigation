#pragma once

#include <AeonixNavigation/Public/Data/AeonixLink.h>
#include <AeonixNavigation/Public/Pathfinding/AeonixPathFinder.h>

#include <Runtime/Core/Public/Async/AsyncWork.h>
#include <Runtime/Core/Public/HAL/ThreadSafeBool.h>

struct FAeonixData;
struct AeonixPathFinderSettings;

class FAeonixFindPathTask : public FNonAbandonableTask
{
	friend class FAutoDeleteAsyncTask<FAeonixFindPathTask>;

public:
	FAeonixFindPathTask(const FAeonixData& Data, const FAeonixPathFinderSettings& aSettings, const AeonixLink aStart, const AeonixLink aGoal, const FVector& aStartPos, const FVector& aTargetPos, FAeonixNavigationPath& oPath, FThreadSafeCounter& aStatusCounter)
		: NavigationData(Data)
		, Start(aStart)
		, Goal(aGoal)
		, StartPos(aStartPos)
		, TargetPos(aTargetPos)
		, Path(oPath)
		, Settings(aSettings)
		, StatusCounter(aStatusCounter)
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
	FThreadSafeCounter& StatusCounter;

	void DoWork();
	
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAeonixFindPathTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};