#pragma once

#include <AeonixNavigation/Public/Data/AeonixData.h>
#include <AeonixNavigation/Public/Interface/AeonixDebugDrawInterface.h>
#include <AeonixNavigation/Public/Interface/AeonixSubsystemInterface.h>

#include <Runtime/Engine/Classes/GameFramework/Volume.h>

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

	bool Generate();
	bool HasData() const;
	void ClearData();

	const FAeonixData& GetNavData() const { return NavigationData; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix")
	FAeonixGenerationParameters GenerationParameters;

	bool bIsReadyForNavigation{false};

protected:
	FAeonixData NavigationData;

	UPROPERTY()
	TScriptInterface<IAeonixSubsystemInterface> AeonixSubsystemInterface;
	UPROPERTY()
	TScriptInterface<IAeonixCollisionQueryInterface> CollisionQueryInterface;

	void UpdateBounds();

	//  IAeonixDebugDrawInterface BEGIN
	void AeonixDrawDebugString(const FVector& Position, const FString& String, const FColor& Color) const override;
	void AeonixDrawDebugBox(const FVector& Position, const float Size, const FColor& Color) const override;
	//  IAeonixDebugDrawInterface END
};
