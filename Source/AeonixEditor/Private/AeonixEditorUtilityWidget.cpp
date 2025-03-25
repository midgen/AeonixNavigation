// Copyright 2024 Chris Ashworth


#include <AeonixEditor/Private/AeonixEditorUtilityWidget.h>

#include <AeonixNavigation/Public/Subsystem/AeonixSubsystem.h>

void UAeonixEditorUtilityWidget::CompleteAllPendingPathfindingTasks()
{
	GetWorld()->GetSubsystem<UAeonixSubsystem>()->CompleteAllPendingPathfindingTasks();
}