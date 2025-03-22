#include <AeonixNavigation/Public/Pathfinding/AeonixPathFinder.h>
#include <AeonixNavigation/Public/Task/AeonixFindPathTask.h>

void FAeonixFindPathTask::DoWork()
{
	AeonixPathFinder PathFinder(NavigationData, Settings);

	StatusCounter.Increment();

	int Result = PathFinder.FindPath(Start, Goal, StartPos, TargetPos, Path);

	StatusCounter.Increment();
}
