// Copyright 2024 Chris Ashworth

#pragma once

#include <Runtime/MassEntity/Public/MassEntityTypes.h>

#include "AeonixFragments.generated.h"

class UAeonixNavAgentComponent;
class AAeonixBoundingVolume;

USTRUCT()
struct FAeonixNavAgentFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UAeonixNavAgentComponent> Agent;
};

USTRUCT()
struct FAeonixBoundingVolumeFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AAeonixBoundingVolume> Actor;
};

USTRUCT()
struct FAeonixSphereColliderTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FAeonixBoxColliderTag : public FMassTag
{
	GENERATED_BODY()
};