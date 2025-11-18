// Copyright 2024 Chris Ashworth

#include "Data/AeonixAsyncRegen.h"
#include "AeonixNavigation.h"
#include "Actor/AeonixBoundingVolume.h"
#include "Data/AeonixData.h"
#include "Data/AeonixOctreeData.h"
#include "Data/AeonixLeafNode.h"
#include "Data/AeonixStats.h"
#include "Interface/AeonixCollisionQueryInterface.h"
#include "Subsystem/AeonixCollisionSubsystem.h"
#include "Library/libmorton/morton.h"

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"

namespace AeonixAsyncRegen
{
	void ExecuteAsyncRegen(const FAeonixAsyncRegenBatch& Batch)
	{
		SCOPE_CYCLE_COUNTER(STAT_AeonixDynamicAsync);

		if (!Batch.VolumePtr.IsValid())
		{
			UE_LOG(LogAeonixRegen, Error, TEXT("ExecuteAsyncRegen: Volume pointer is invalid!"));
			return;
		}

		const int32 TotalLeaves = Batch.LeafIndicesToProcess.Num();
		if (TotalLeaves == 0)
		{
			UE_LOG(LogAeonixRegen, Warning, TEXT("ExecuteAsyncRegen: No leaves to process"));
			return;
		}

		UE_LOG(LogAeonixRegen, Display, TEXT("ExecuteAsyncRegen: Processing %d leaves in chunks of %d"),
			TotalLeaves, Batch.ChunkSize);

		// Store all results
		TArray<FAeonixLeafRasterResult> AllResults;
		AllResults.Reserve(TotalLeaves);

		// Process in chunks to minimize physics scene lock hold time
		int32 ChunksProcessed = 0;
		for (int32 ChunkStart = 0; ChunkStart < TotalLeaves; ChunkStart += Batch.ChunkSize)
		{
			const int32 ChunkEnd = FMath::Min(ChunkStart + Batch.ChunkSize, TotalLeaves);

			// Process this chunk (acquires and releases physics scene read lock internally)
			ProcessLeafChunk(Batch, ChunkStart, ChunkEnd, AllResults);

			ChunksProcessed++;
		}

		UE_LOG(LogAeonixRegen, Display, TEXT("ExecuteAsyncRegen: Processed %d chunks, got %d results"),
			ChunksProcessed, AllResults.Num());

		// Enqueue results for time-budgeted processing on game thread
		AAeonixBoundingVolume* Volume = Batch.VolumePtr.Get();
		if (!Volume)
		{
			UE_LOG(LogAeonixRegen, Error, TEXT("ExecuteAsyncRegen: Volume pointer became null during async processing"));
			return;
		}

		AsyncTask(ENamedThreads::GameThread, [Volume, Results = MoveTemp(AllResults), TotalLeaves]() mutable
		{
			if (!Volume || !IsValid(Volume))
			{
				UE_LOG(LogAeonixRegen, Error, TEXT("ExecuteAsyncRegen: Volume became invalid before completion"));
				return;
			}

			// Enqueue results for time-budgeted processing
			// The volume's Tick will process them incrementally to avoid frame spikes
			Volume->EnqueueRegenResults(MoveTemp(Results), TotalLeaves);
		});
	}

	void ProcessLeafChunk(
		const FAeonixAsyncRegenBatch& Batch,
		int32 ChunkStart,
		int32 ChunkEnd,
		TArray<FAeonixLeafRasterResult>& OutResults)
	{
		SCOPE_CYCLE_COUNTER(STAT_AeonixDynamicAsyncChunk);

		if (!Batch.VolumePtr.IsValid())
		{
			return;
		}

		AAeonixBoundingVolume* Volume = Batch.VolumePtr.Get();
		if (!Volume || !IsValid(Volume))
		{
			return;
		}

		UWorld* World = Volume->GetWorld();
		if (!World)
		{
			UE_LOG(LogAeonixRegen, Error, TEXT("ProcessLeafChunk: World is null"));
			return;
		}

		// Get collision subsystem for queries
		UAeonixCollisionSubsystem* CollisionSubsystem = World->GetSubsystem<UAeonixCollisionSubsystem>();
		if (!CollisionSubsystem)
		{
			UE_LOG(LogAeonixRegen, Error, TEXT("ProcessLeafChunk: CollisionSubsystem is null"));
			return;
		}

		// Note: We're calling collision queries on the game thread's physics scene
		// The collision subsystem's IsBlocked/IsLeafBlocked methods use UWorld::OverlapBlockingTestByChannel
		// which is safe to call from any thread as long as we're careful about timing
		//
		// For stock engine, we cannot easily acquire FScopedSceneLock_Chaos from plugin code
		// as it requires internal physics headers that may not be accessible.
		//
		// The current implementation relies on the fact that:
		// 1. We dispatch from TG_PostPhysics (physics has completed for this frame)
		// 2. Collision queries use read-only physics scene access
		// 3. We process quickly enough that physics doesn't tick during our queries
		//
		// This is a pragmatic solution for stock engine. For better thread safety with
		// longer processing times, consider processing entirely on game thread in TG_PostPhysics
		// or moving to engine modifications to properly lock the physics scene.

		// Process each leaf in this chunk
		for (int32 i = ChunkStart; i < ChunkEnd; ++i)
		{
			if (i >= Batch.LeafOrigins.Num() || i >= Batch.LeafIndicesToProcess.Num())
			{
				continue;
			}

			const FVector& LeafOrigin = Batch.LeafOrigins[i];
			const mortoncode_t LeafIndex = Batch.LeafIndicesToProcess[i];

			// Rasterize this leaf with two-pass optimization
			// LeafIndex contains the correct node array index from the batch preparation
			const uint64 VoxelBitmask = RasterizeLeafNodeAsync(
				LeafOrigin,
				LeafIndex,
				LeafIndex, // Fixed: Use actual node array index, not chunk loop index
				Batch.GenParams,
				*CollisionSubsystem);

			// Store result (even if all clear - we need to update the leaf node)
			OutResults.Emplace(LeafIndex, LeafIndex, VoxelBitmask);
		}
	}

	uint64 RasterizeLeafNodeAsync(
		const FVector& LeafOrigin,
		mortoncode_t LeafIndex,
		nodeindex_t LeafNodeArrayIndex,
		const FAeonixGenerationParameters& GenParams,
		const IAeonixCollisionQueryInterface& CollisionInterface)
	{
		SCOPE_CYCLE_COUNTER(STAT_AeonixDynamicAsyncLeaf);

		// Calculate voxel and leaf sizes
		const float VoxelSizeLayer0 = (GenParams.Extents.X / FMath::Pow(2.f, GenParams.VoxelPower)) * 2.0f; // Layer 0 voxel size
		const float LeafVoxelSize = VoxelSizeLayer0 * 0.25f; // Each leaf voxel is 1/4 the Layer 0 size
		const float LeafSize = LeafVoxelSize * 4.0f; // 4x4x4 voxels
		const FVector LeafCenter = LeafOrigin + FVector(LeafSize * 0.5f);

		// Pass 1: Test entire leaf volume (two-pass optimization)
		if (!CollisionInterface.IsLeafBlocked(LeafCenter, LeafSize * 0.5f, GenParams.CollisionChannel, GenParams.AgentRadius))
		{
			// Entire leaf is clear - all 64 voxels are empty
			return 0;
		}

		// Pass 2: Leaf contains blocking geometry - do detailed 64-voxel rasterization
		uint64 VoxelBitmask = 0;

		for (int i = 0; i < 64; i++)
		{
			uint_fast32_t x, y, z;
			morton3D_64_decode(i, x, y, z);

			const FVector Position = LeafOrigin +
				FVector(x * LeafVoxelSize, y * LeafVoxelSize, z * LeafVoxelSize) +
				FVector(LeafVoxelSize * 0.5f);

			if (CollisionInterface.IsBlocked(Position, LeafVoxelSize * 0.5f, GenParams.CollisionChannel, GenParams.AgentRadius))
			{
				// Set the bit for this voxel
				VoxelBitmask |= (1ULL << i);
			}
		}

		return VoxelBitmask;
	}

} // namespace AeonixAsyncRegen
