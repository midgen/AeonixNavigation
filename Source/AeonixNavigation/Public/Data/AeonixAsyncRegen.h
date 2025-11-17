// Copyright 2024 Chris Ashworth

#pragma once

#include "CoreMinimal.h"
#include "Data/AeonixTypes.h"
#include "Data/AeonixDefines.h"
#include "Data/AeonixGenerationParameters.h"

// Forward declarations
class AAeonixBoundingVolume;
class IAeonixCollisionQueryInterface;
class FPhysScene_Chaos;

/**
 * Data structure for async dynamic subregion regeneration batch
 */
struct FAeonixAsyncRegenBatch
{
	/** Leaf node indices that need to be processed */
	TArray<mortoncode_t> LeafIndicesToProcess;

	/** Leaf node coordinates for processing */
	TArray<FIntVector> LeafCoordinates;

	/** Leaf origins (corner positions) for rasterization */
	TArray<FVector> LeafOrigins;

	/** Pointer to physics scene for collision queries */
	FPhysScene_Chaos* PhysicsScenePtr = nullptr;

	/** Generation parameters (collision channel, agent radius, voxel power, etc.) */
	FAeonixGenerationParameters GenParams;

	/** Weak pointer to the volume being regenerated */
	TWeakObjectPtr<AAeonixBoundingVolume> VolumePtr;

	/** Chunk size for lock management (number of leaves to process before releasing lock) */
	int32 ChunkSize = 75;

	FAeonixAsyncRegenBatch() = default;
};

/**
 * Result of a single leaf node rasterization
 */
struct FAeonixLeafRasterResult
{
	/** Morton code index of the leaf */
	mortoncode_t LeafIndex;

	/** Index into the LeafNodes array */
	nodeindex_t LeafNodeArrayIndex;

	/** 64-bit voxel bitmask for this leaf */
	uint64 VoxelBitmask;

	FAeonixLeafRasterResult()
		: LeafIndex(0)
		, LeafNodeArrayIndex(0)
		, VoxelBitmask(0)
	{}

	FAeonixLeafRasterResult(mortoncode_t InLeafIndex, nodeindex_t InLeafNodeArrayIndex, uint64 InVoxelBitmask)
		: LeafIndex(InLeafIndex)
		, LeafNodeArrayIndex(InLeafNodeArrayIndex)
		, VoxelBitmask(InVoxelBitmask)
	{}
};

/**
 * Namespace for async dynamic subregion regeneration functions
 */
namespace AeonixAsyncRegen
{
	/**
	 * Execute async dynamic subregion regeneration on background thread with chunked physics scene locking
	 * @param Batch The batch data containing all information needed for regeneration
	 */
	void ExecuteAsyncRegen(const FAeonixAsyncRegenBatch& Batch);

	/**
	 * Process a single chunk of leaves with physics scene read lock held
	 * @param Batch The batch data
	 * @param ChunkStart Starting index in the LeafIndicesToProcess array
	 * @param ChunkEnd Ending index (exclusive)
	 * @param OutResults Array to append results to
	 */
	void ProcessLeafChunk(
		const FAeonixAsyncRegenBatch& Batch,
		int32 ChunkStart,
		int32 ChunkEnd,
		TArray<FAeonixLeafRasterResult>& OutResults);

	/**
	 * Rasterize a single leaf node with two-pass optimization
	 * @param LeafOrigin Corner position of the leaf
	 * @param LeafIndex Morton code of the leaf
	 * @param LeafNodeArrayIndex Index into the LeafNodes array
	 * @param GenParams Generation parameters
	 * @param CollisionInterface Collision query interface
	 * @return Voxel bitmask for this leaf (0 if all clear)
	 */
	uint64 RasterizeLeafNodeAsync(
		const FVector& LeafOrigin,
		mortoncode_t LeafIndex,
		nodeindex_t LeafNodeArrayIndex,
		const FAeonixGenerationParameters& GenParams,
		const IAeonixCollisionQueryInterface& CollisionInterface);
}
