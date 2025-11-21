#pragma once

// These types moved around in 5.6, using this to maintain compatibility across versions on main branch
#if !defined(ENGINE_MAJOR_VERSION) || ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 6)
#include "MassEntityTypes.h"
#else
#include "MassEntityHandle.h"
#endif //!defined(ENGINE_MAJOR_VERSION) || ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 6)


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