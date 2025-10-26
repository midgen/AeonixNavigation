#include "Task/AeonixFindPathTask.h"

#include "Pathfinding/AeonixPathFinder.h"
#include "Data/AeonixTypes.h"

void FAeonixFindPathTask::DoWork()
{
	AeonixPathFinder PathFinder(NavigationData, Settings);

	//Request.SetPathRequestStatusLocked(EAeonixPathFindStatus::InProgress);	
	
	if (PathFinder.FindPath(Start, Goal, StartPos, TargetPos, Path))
	{
		//Request.SetPathRequestStatusLocked(EAeonixPathFindStatus::Complete);	
	}
	else
	{
		//Request.SetPathRequestStatusLocked(EAeonixPathFindStatus::Failed);	
	}
}
