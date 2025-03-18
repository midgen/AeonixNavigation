#include <AeonixNavigation/Public/Task/AeonixFindPathTask.h>
#include <AeonixNavigation/Public/Pathfinding/AeonixPathFinder.h>

void FAeonixFindPathTask::DoWork()
{
	AeonixPathFinder PathFinder(myWorld, NavigationData, mySettings);

	int Result = PathFinder.FindPath(myStart, myTarget, myStartPos, myTargetPos, myPath);

	myCompleteFlag = true;
}
