
#pragma once

#include <AeonixNavigation/Public//Data/AeonixTypes.h>

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

	UAenoixEditorDebugSubsystem(const FObjectInitializer& Initializer);
	
	UPROPERTY()
	TObjectPtr<AAeonixPathDebugActor> StartDebugActor{nullptr};
	UPROPERTY()
	TObjectPtr<AAeonixPathDebugActor> EndDebugActor{nullptr};

	FAeonixNavPathSharedPtr Path;

public:
	UFUNCTION(BlueprintCallable, Category="Aeonix")
	void UpdateDebugActor(AAeonixPathDebugActor* DebugActor);
};
