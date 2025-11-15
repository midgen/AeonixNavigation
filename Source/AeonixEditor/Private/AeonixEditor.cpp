#include "AeonixEditor/AeonixEditor.h"
#include "AeonixEditor/Private/AeonixVolumeDetails.h"

#include "PropertyEditorModule.h"

IMPLEMENT_GAME_MODULE(FAeonixEditorModule, AeonixEditor);

DEFINE_LOG_CATEGORY(LogAeonixEditor)

#define LOCTEXT_NAMESPACE "AeonixEditor"

void FAeonixEditorModule::StartupModule()
{
	UE_LOG(LogAeonixEditor, Log, TEXT("AeonixEditorModule: Log Started"));

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.RegisterCustomClassLayout("AeonixBoundingVolume", FOnGetDetailCustomizationInstance::CreateStatic(&FAeonixVolumeDetails::MakeInstance));
}

void FAeonixEditorModule::ShutdownModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	UE_LOG(LogAeonixEditor, Log, TEXT("AeonixEditorModule: Log Ended"));
}

#undef LOCTEXT_NAMESPACE