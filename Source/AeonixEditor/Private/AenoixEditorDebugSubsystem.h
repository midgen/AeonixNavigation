
#pragma once

#include <AeonixNavigation/Public/Pathfinding/AeonixNavigationPath.h>

#include <EditorSubsystem.h>

#include "AenoixEditorDebugSubsystem.generated.h"

/**
 *  A subsystem that provides debug functionality for the Aenoix Editor.
 *
 * @see UEditorSubsystem
 */
UCLASS()
class AEONIXEDITOR_API UAenoixEditorDebugSubsystem : public UEditorSubsystem,  public FTickableGameObject
{
	GENERATED_BODY()
	
	UPROPERTY()
	TObjectPtr<AAeonixPathDebugActor> StartDebugActor{nullptr};
	UPROPERTY()
	TObjectPtr<AAeonixPathDebugActor> EndDebugActor{nullptr};
	UPROPERTY()
	FAeonixNavigationPath CurrentDebugPath{};
	UPROPERTY()
	TObjectPtr<AAeonixBoundingVolume> CurrentDebugVolume{nullptr};

	bool bIsPathPending{false};
	
public:
	UFUNCTION(BlueprintCallable, Category="Aeonix")
	void UpdateDebugActor(AAeonixPathDebugActor* DebugActor);

	UFUNCTION()
	void OnPathFindComplete(EAeonixPathFindStatus Status);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override;
	virtual bool IsTickableWhenPaused() const override;
};

