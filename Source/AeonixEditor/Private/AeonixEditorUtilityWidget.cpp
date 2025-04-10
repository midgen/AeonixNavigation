// Copyright 2024 Chris Ashworth


#include <AeonixEditor/Private/AeonixEditorUtilityWidget.h>

#include <AeonixNavigation/Public/Subsystem/AeonixSubsystem.h>

void UAeonixEditorUtilityWidget::CompleteAllPendingPathfindingTasks()
{

	

	
	GetWorld()->GetSubsystem<UAeonixSubsystem>()->CompleteAllPendingPathfindingTasks();
}

int32 UAeonixEditorUtilityWidget::GetNumberOfPendingPathFindTasks() const
{
	if (GEditor->GetPIEWorldContext())
	{
		return GEditor->GetPIEWorldContext()->World()->GetSubsystem<UAeonixSubsystem>()->GetNumberOfPendingTasks();
	}
	
	return GetWorld()->GetSubsystem<UAeonixSubsystem>()->GetNumberOfPendingTasks();
}

int32 UAeonixEditorUtilityWidget::GetNumberOfRegisteredNavAgents() const
{
	if (GEditor->GetPIEWorldContext())
	{
		return GEditor->GetPIEWorldContext()->World()->GetSubsystem<UAeonixSubsystem>()->GetNumberOfRegisteredNavAgents();
	}
	
	return GetWorld()->GetSubsystem<UAeonixSubsystem>()->GetNumberOfRegisteredNavAgents();
}

int32 UAeonixEditorUtilityWidget::GetNumberOfRegisteredNavVolumes() const
{
	if (GEditor->GetPIEWorldContext())
	{
		return GEditor->GetPIEWorldContext()->World()->GetSubsystem<UAeonixSubsystem>()->GetNumberOfRegisteredNavVolumes();
	}
	
	return GetWorld()->GetSubsystem<UAeonixSubsystem>()->GetNumberOfRegisteredNavVolumes();
}