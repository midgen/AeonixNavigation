// Copyright 2024 Chris Ashworth


#include <AeonixNavigation/Public/Subsystem/AeonixCollisionSubsystem.h>

bool UAeonixCollisionSubsystem::IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const
{
	FCollisionQueryParams Params;
	Params.bFindInitialOverlaps = true;
	Params.bTraceComplex = false;
	Params.TraceTag = "AeonixLeafRasterize";

	bool isBlocked = World->OverlapBlockingTestByChannel(Position, FQuat::Identity, CollisionChannel, FCollisionShape::MakeBox(FVector(VoxelSize + AgentRadius)), Params);

	return World->OverlapBlockingTestByChannel(Position, FQuat::Identity, CollisionChannel, FCollisionShape::MakeBox(FVector(VoxelSize + AgentRadius)), Params);
}