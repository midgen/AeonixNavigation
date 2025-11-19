#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Pathfinding/AeonixNavigationPath.h"
#include "AeonixNavigationLibrary.generated.h"

UCLASS()
class AEONIXNAVIGATION_API UAeonixNavigationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Get the array of path points from a navigation path */
	UFUNCTION(BlueprintPure, Category = "Aeonix|Path")
	static TArray<FAeonixPathPoint> GetPathPoints(const FAeonixNavigationPath& Path);

	/** Get the number of points in the path */
	UFUNCTION(BlueprintPure, Category = "Aeonix|Path")
	static int32 GetNumPathPoints(const FAeonixNavigationPath& Path);

	/** Check if the path is ready/valid */
	UFUNCTION(BlueprintPure, Category = "Aeonix|Path")
	static bool IsPathReady(const FAeonixNavigationPath& Path);
};
