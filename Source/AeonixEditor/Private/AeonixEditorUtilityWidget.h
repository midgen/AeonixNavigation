// Copyright 2024 Chris Ashworth

#pragma once

#include "CoreMinimal.h"
#include "Editor/Blutility/Classes/EditorUtilityWidget.h"
#include "AeonixEditorUtilityWidget.generated.h"

/**
 * 
 */
UCLASS()
class AEONIXEDITOR_API UAeonixEditorUtilityWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable)
	void CompleteAllPendingPathfindingTasks();
};
