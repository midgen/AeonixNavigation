// Copyright 2024 Chris Ashworth

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AeonixSettings.generated.h"

/**
 * 
 */
UCLASS(config = AeonixNavigation)
class AEONIXNAVIGATION_API UAeonixSettings : public UObject
{
	GENERATED_BODY()

	// UPROPERTY(Config, EditAnywhere, Category="Aeonix")
	// TObjectPtr<UDataAsset> BoundingVolumeEntityConfig{nullptr};
	//
	// UPROPERTY(Config, EditAnywhere, Category="Aeonix")
	// TObjectPtr<UDataAsset> SphereColliderEntityConfig{nullptr};
	//
	// UPROPERTY(Config, EditAnywhere, Category="Aeonix")
	// TObjectPtr<UDataAsset> BoxColliderEntityConfig{nullptr};
};