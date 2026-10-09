#pragma once

#include "Interface/AeonixNavVolumeInterface.h"

#include "AeonixHandleTypes.generated.h"

class AAeonixModifierVolume;
class UAeonixNavAgentComponent;

USTRUCT()
struct FAeonixBoundingVolumeHandle
{
	GENERATED_BODY()

	FAeonixBoundingVolumeHandle(){}
	FAeonixBoundingVolumeHandle(IAeonixNavVolumeInterface* Volume) : VolumeHandle(Volume ? Volume->GetVolumeObject() : nullptr) {}

	bool operator==(const FAeonixBoundingVolumeHandle& Volume ) const { return Volume.VolumeHandle == VolumeHandle; }

	IAeonixNavVolumeInterface* Get() const { return VolumeHandle.GetInterface(); }

	UPROPERTY()
	TScriptInterface<IAeonixNavVolumeInterface> VolumeHandle;

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
