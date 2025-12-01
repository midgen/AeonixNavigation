#pragma once

#include "AeonixHandleTypes.generated.h"

class AAeonixBoundingVolume;
class AAeonixModifierVolume;
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
	TArray<TObjectPtr<AAeonixModifierVolume>> ModifierVolumes;
};

USTRUCT()
struct FAeonixNavAgentHandle
{
	GENERATED_BODY()

	FAeonixNavAgentHandle(){}
	explicit FAeonixNavAgentHandle(UAeonixNavAgentComponent* Agent) : NavAgentComponent(Agent) {}

	bool operator==(const FAeonixNavAgentHandle& Agent ) const { return Agent.NavAgentComponent == NavAgentComponent; }
	bool operator==(UAeonixNavAgentComponent* AgentPtr ) const { return AgentPtr == NavAgentComponent; }

	UPROPERTY()
	TObjectPtr<UAeonixNavAgentComponent> NavAgentComponent;
};