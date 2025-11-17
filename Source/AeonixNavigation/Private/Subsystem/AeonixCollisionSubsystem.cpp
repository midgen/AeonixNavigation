// Copyright 2024 Chris Ashworth


#include "Subsystem/AeonixCollisionSubsystem.h"

bool UAeonixCollisionSubsystem::IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const
{
	FCollisionQueryParams Params;
	Params.bFindInitialOverlaps = true;
	Params.bTraceComplex = false;
	Params.TraceTag = "AeonixLeafRasterize";

	return GetWorld()->OverlapBlockingTestByChannel(Position, FQuat::Identity, CollisionChannel, FCollisionShape::MakeBox(FVector(VoxelSize + AgentRadius)), Params);
}

bool UAeonixCollisionSubsystem::IsLeafBlocked(const FVector& Position, const float LeafSize, ECollisionChannel CollisionChannel, const float AgentRadius) const
{
	FCollisionQueryParams Params;
	Params.bFindInitialOverlaps = true;
	Params.bTraceComplex = false;
	Params.TraceTag = "AeonixWholeLeafTest";

	// Test the entire leaf volume (4x4x4 voxel block) with agent radius buffer
	return GetWorld()->OverlapBlockingTestByChannel(Position, FQuat::Identity, CollisionChannel, FCollisionShape::MakeBox(FVector(LeafSize + AgentRadius)), Params);
}

bool UAeonixCollisionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// All the worlds, so it works in editor
	return true;
}