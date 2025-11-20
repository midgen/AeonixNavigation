// Copyright 2024 Chris Ashworth

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AeonixSettings.generated.h"

/**
 * Aeonix Navigation Plugin Settings
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Aeonix Navigation"))
class AEONIXNAVIGATION_API UAeonixSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAeonixSettings();

	// Override to place settings in the correct category
	virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }

	//~ Pathfinding Settings

	/**
	 * Number of worker threads for async pathfinding operations.
	 * Recommended: 2-8 threads depending on CPU core count.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Pathfinding", meta = (ClampMin = "1", ClampMax = "16", UIMin = "1", UIMax = "8"))
	int32 PathfindingWorkerThreads = 2;

	/**
	 * Maximum number of concurrent pathfinding requests allowed in the queue.
	 * Prevents memory issues when overwhelmed with pathfinding requests.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Pathfinding", meta = (ClampMin = "4", ClampMax = "200", UIMin = "8", UIMax = "100",
		Tooltip = "Maximum pending pathfinding requests. Higher values allow more buffering but use more memory."))
	int32 MaxConcurrentPathfinds = 8;

	//~ Dynamic Regeneration Settings

	/**
	 * Time budget per frame for applying dynamic regeneration results (milliseconds).
	 * Lower values spread work across more frames, reducing spikes but increasing total time.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Dynamic Regeneration", meta = (ClampMin = "1.0", ClampMax = "50.0", UIMin = "1.0", UIMax = "20.0"))
	float DynamicRegenTimeBudgetMs = 5.0f;

	/**
	 * Number of leaves to process in each async chunk (for physics lock management).
	 * Higher values = fewer lock acquisitions but longer hold times.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Dynamic Regeneration", meta = (ClampMin = "10", ClampMax = "500", UIMin = "25", UIMax = "200"))
	int32 AsyncChunkSize = 75;

	/**
	 * Minimum time between dynamic region regenerations (seconds).
	 * Prevents excessive regeneration when obstacles move frequently.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Dynamic Regeneration", meta = (ClampMin = "0.0", ClampMax = "5.0", UIMin = "0.1", UIMax = "2.0"))
	float DynamicRegenCooldown = 0.5f;

	/**
	 * Delay after marking a region dirty before processing it at runtime (seconds).
	 * Allows physics to settle before regenerating navigation.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Dynamic Regeneration", meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "1.0"))
	float DirtyRegionProcessDelay = 0.25f;

	/**
	 * Delay after marking a region dirty before processing it in editor (seconds).
	 * Longer than runtime delay to allow for editor overhead.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Dynamic Regeneration", meta = (ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.5", UIMax = "5.0"))
	float EditorDirtyRegionProcessDelay = 1.0f;
};