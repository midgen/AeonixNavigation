// Copyright 2024 Aeonix Navigation. All Rights Reserved.

#pragma once

#include "Stats/Stats.h"
#include "Stats/Stats2.h"

/**
 * Aeonix Navigation Performance Stats
 *
 * Use 'stat Aeonix' in console to view these stats in real-time.
 * Use 'stat startfile' / 'stat stopfile' for CSV profiling.
 */

// Declare the main Aeonix stats group
DECLARE_STATS_GROUP(TEXT("Aeonix"), STATGROUP_Aeonix, STATCAT_Advanced);

// Octree Generation Stats
DECLARE_CYCLE_STAT(TEXT("Full Octree Generation"), STAT_AeonixFullOctreeGen, STATGROUP_Aeonix);
DECLARE_CYCLE_STAT(TEXT("Dynamic Subregion Sync"), STAT_AeonixDynamicSync, STATGROUP_Aeonix);

// Async Dynamic Subregion Stats (3 levels of granularity)
DECLARE_CYCLE_STAT(TEXT("Dynamic Subregion Async"), STAT_AeonixDynamicAsync, STATGROUP_Aeonix);
DECLARE_CYCLE_STAT(TEXT("Dynamic Async Chunk"), STAT_AeonixDynamicAsyncChunk, STATGROUP_Aeonix);
DECLARE_CYCLE_STAT(TEXT("Dynamic Async Leaf"), STAT_AeonixDynamicAsyncLeaf, STATGROUP_Aeonix);

// Pathfinding Stats
DECLARE_CYCLE_STAT(TEXT("Pathfinding Sync"), STAT_AeonixPathfindingSync, STATGROUP_Aeonix);
DECLARE_CYCLE_STAT(TEXT("Pathfinding Async"), STAT_AeonixPathfindingAsync, STATGROUP_Aeonix);
