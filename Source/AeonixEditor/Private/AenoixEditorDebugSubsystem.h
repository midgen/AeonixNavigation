
#pragma once

#include <AeonixNavigation/Public/Pathfinding/AeonixNavigationPath.h>

#include <EditorSubsystem.h>

#include "AenoixEditorDebugSubsystem.generated.h"

USTRUCT()
struct FAeonixFailedPath
{
	GENERATED_BODY()

	UPROPERTY()
	FVector StartPoint;

	UPROPERTY()
	FVector EndPoint;

	FAeonixFailedPath() {}
	FAeonixFailedPath(const FVector& InStart, const FVector& InEnd)
		: StartPoint(InStart), EndPoint(InEnd) {}
};

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
	FAeonixNavigationPath CachedDebugPath{};
	UPROPERTY(Transient)
	TSoftObjectPtr<AAeonixBoundingVolume> CurrentDebugVolume{nullptr};

	bool bIsPathPending{false};
	bool bHasValidCachedPath{false};
	bool bNeedsRedraw{true};
	bool bBatchPathsNeedRedraw{false};
	bool bFailedPathsNeedRedraw{false};

	// Batch run paths for visualization
	UPROPERTY(Transient)
	TArray<FAeonixNavigationPath> BatchRunPaths;

	// Failed batch run paths for visualization
	UPROPERTY(Transient)
	TArray<FAeonixFailedPath> FailedBatchRunPaths;

public:
	UFUNCTION(BlueprintCallable, Category="Aeonix")
	void UpdateDebugActor(AAeonixPathDebugActor* DebugActor);

	UFUNCTION(BlueprintCallable, Category="Aeonix")
	void ClearDebugActor(AAeonixPathDebugActor* ActorToRemove);

	UFUNCTION()
	void OnPathFindComplete(EAeonixPathFindStatus Status);

	UFUNCTION(BlueprintCallable, Category="Aeonix")
	void SetBatchRunPaths(const TArray<FAeonixNavigationPath>& Paths);

	UFUNCTION(BlueprintCallable, Category="Aeonix")
	void ClearBatchRunPaths();

	void SetFailedBatchRunPaths(const TArray<TPair<FVector, FVector>>& FailedPaths);

	UFUNCTION(BlueprintCallable, Category="Aeonix")
	void ClearFailedBatchRunPaths();

	UFUNCTION(BlueprintCallable, Category="Aeonix")
	void ClearCachedPath();

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override;
	virtual bool IsTickableWhenPaused() const override;
};

