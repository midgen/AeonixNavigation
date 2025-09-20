﻿
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
	
	UPROPERTY(Transient)
	TSoftObjectPtr<AAeonixPathDebugActor> StartDebugActor{nullptr};
	UPROPERTY(Transient)
	TSoftObjectPtr<AAeonixPathDebugActor> EndDebugActor{nullptr};
	UPROPERTY(Transient)
	FAeonixNavigationPath CurrentDebugPath{};
	UPROPERTY(Transient)
	TSoftObjectPtr<AAeonixBoundingVolume> CurrentDebugVolume{nullptr};

	bool bIsPathPending{false};

	// Batch run paths for visualization
	UPROPERTY(Transient)
	TArray<FAeonixNavigationPath> BatchRunPaths;
	
public:
	UFUNCTION(BlueprintCallable, Category="Aeonix")
	void UpdateDebugActor(AAeonixPathDebugActor* DebugActor);

	UFUNCTION()
	void OnPathFindComplete(EAeonixPathFindStatus Status);

	UFUNCTION(BlueprintCallable, Category="Aeonix")
	void SetBatchRunPaths(const TArray<FAeonixNavigationPath>& Paths);

	UFUNCTION(BlueprintCallable, Category="Aeonix")
	void ClearBatchRunPaths();

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override;
	virtual bool IsTickableWhenPaused() const override;
};

