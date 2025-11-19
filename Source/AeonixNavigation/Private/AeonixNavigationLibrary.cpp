#include "AeonixNavigationLibrary.h"

TArray<FAeonixPathPoint> UAeonixNavigationLibrary::GetPathPoints(const FAeonixNavigationPath& Path)
{
	return Path.GetPathPoints();
}

int32 UAeonixNavigationLibrary::GetNumPathPoints(const FAeonixNavigationPath& Path)
{
	return Path.GetNumPoints();
}

bool UAeonixNavigationLibrary::IsPathReady(const FAeonixNavigationPath& Path)
{
	return Path.IsReady();
}
