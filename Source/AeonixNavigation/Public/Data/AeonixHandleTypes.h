#pragma once

#include <Runtime/MassEntity/Public/MassEntityTypes.h>

#include "AeonixHandleTypes.generated.h"

class AAeonixBoundingVolume;
class AAeonixDynamicSubregion;
class UAeonixNavAgentComponent;

USTRUCT()
struct FAeonixBoundingVolumeHandle
{
	GENERATED_BODY()

	FAeonixBoundingVolumeHandle(){}
	FAeonixBoundingVolumeHandle(AAeonixBoundingVolume* Volume) : VolumeHandle(Volume) {}

	bool operator==(const FAeonixBoundingVolumeHandle& Volume ) const { return Volume.VolumeHandle == VolumeHandle; }
	
	UPROPERTY()
	TObjectPtr<AAeonixBoundingVolume> VolumeHandle;

	UPROPERTY()
	TArray<TObjectPtr<AAeonixDynamicSubregion>> SubRegions;

	UPROPERTY()
	FMassEntityHandle EntityHandle;
};

USTRUCT()
struct FAeonixNavAgentHandle
{
	GENERATED_BODY()

	FAeonixNavAgentHandle(){}
	explicit FAeonixNavAgentHandle(UAeonixNavAgentComponent* Agent, FMassEntityHandle Entity) : NavAgentComponent(Agent), EntityHandle(Entity) {}

	bool operator==(const FAeonixNavAgentHandle& Agent ) const { return Agent.NavAgentComponent == NavAgentComponent; }
	bool operator==(UAeonixNavAgentComponent* AgentPtr ) const { return AgentPtr == NavAgentComponent; }

	UPROPERTY()
	TObjectPtr<UAeonixNavAgentComponent> NavAgentComponent;
	
	UPROPERTY()
	FMassEntityHandle EntityHandle;
};