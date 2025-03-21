
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
class AEONIXEDITOR_API UAenoixEditorDebugSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()
	
	UPROPERTY()
	TObjectPtr<AAeonixPathDebugActor> StartDebugActor{nullptr};
	UPROPERTY()
	TObjectPtr<AAeonixPathDebugActor> EndDebugActor{nullptr};
	UPROPERTY()
	FAeonixNavigationPath CurrentDebugPath{};

public:
	UFUNCTION(BlueprintCallable, Category="Aeonix")
	void UpdateDebugActor(AAeonixPathDebugActor* DebugActor);
};
