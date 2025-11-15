#pragma once

#include "Data/AeonixData.h"
#include "Interface/AeonixDebugDrawInterface.h"
#include "Interface/AeonixSubsystemInterface.h"

#include "GameFramework/Volume.h"

#include "AeonixBoundingVolume.generated.h"

/**
 *  AeonixVolume is a bounding volume that forms a navigable area
 */
UCLASS(hidecategories = (Tags, Cooking, Actor, HLOD, Mobile, LOD))
class AEONIXNAVIGATION_API AAeonixBoundingVolume : public AVolume, public IAeonixDebugDrawInterface
{
	GENERATED_BODY()

public:

	AAeonixBoundingVolume(const FObjectInitializer& ObjectInitializer);

	//~ Begin AActor Interface
	void BeginPlay() override;
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	bool ShouldTickIfViewportsOnly() const override { return true; }
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Destroyed() override;
	//~ End AActor Interface
	
	//~ Begin UObject 
	void Serialize(FArchive& Ar) override;
	//~ End UObject 

	void UpdateBounds();
	bool Generate();
	bool HasData() const;
	void ClearData();

	// Called by editor subsystem to set the debug filter box
	void SetDebugFilterBox(const FBox& FilterBox);
	void ClearDebugFilterBox();

	const FAeonixData& GetNavData() const { return NavigationData; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix")
	FAeonixGenerationParameters GenerationParameters;

	bool bIsReadyForNavigation{false};

private:
	// Flag to indicate that old format baked data was loaded and needs bounds update in BeginPlay
	bool bNeedsLegacyBoundsUpdate{false};

protected:
	FAeonixData NavigationData;

	UPROPERTY(Transient)
	TScriptInterface<IAeonixSubsystemInterface> AeonixSubsystemInterface;
	UPROPERTY(Transient)
	TScriptInterface<IAeonixCollisionQueryInterface> CollisionQueryInterface;

	// IAeonixDebugDrawInterface
	void AeonixDrawDebugString(const FVector& Position, const FString& String, const FColor& Color) const override;
	void AeonixDrawDebugBox(const FVector& Position, const float Size, const FColor& Color) const override;
	void AeonixDrawDebugLine(const FVector& Start, const FVector& End, const FColor& Color, float Thickness = 0.0f) const override;
	void AeonixDrawDebugDirectionalArrow(const FVector& Start, const FVector& End, const FColor& Color, float ArrowSize = 0.0f) const override;
};
