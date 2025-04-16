// Copyright 2024 Chris Ashworth


#include <AeonixNavigation/Public/Subsystem/AeonixCollisionSubsystem.h>

bool UAeonixCollisionSubsystem::IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const
{
	FCollisionQueryParams Params;
	Params.bFindInitialOverlaps = true;
	Params.bTraceComplex = false;
	Params.TraceTag = "AeonixLeafRasterize";

	return GetWorld()->OverlapBlockingTestByChannel(Position, FQuat::Identity, CollisionChannel, FCollisionShape::MakeBox(FVector(VoxelSize + AgentRadius)), Params);
}

bool UAeonixCollisionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// All the worlds, so it works in editor
	return true;
}