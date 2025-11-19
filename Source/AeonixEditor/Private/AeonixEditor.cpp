#include "AeonixEditor/AeonixEditor.h"
#include "AeonixEditor/Private/AeonixVolumeDetails.h"
#include "SAeonixNavigationTreeView.h"

#include "PropertyEditorModule.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "LevelEditor.h"
#include "ToolMenus.h"

IMPLEMENT_GAME_MODULE(FAeonixEditorModule, AeonixEditor);

DEFINE_LOG_CATEGORY(LogAeonixEditor)

#define LOCTEXT_NAMESPACE "AeonixEditor"

static const FName AeonixNavigationTreeTabName("AeonixNavigationTree");

void FAeonixEditorModule::StartupModule()
{
	UE_LOG(LogAeonixEditor, Log, TEXT("AeonixEditorModule: Log Started"));

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.RegisterCustomClassLayout("AeonixBoundingVolume", FOnGetDetailCustomizationInstance::CreateStatic(&FAeonixVolumeDetails::MakeInstance));

	// Register the navigation tree tab
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(AeonixNavigationTreeTabName, FOnSpawnTab::CreateRaw(this, &FAeonixEditorModule::SpawnNavigationTreeTab))
		.SetDisplayName(LOCTEXT("AeonixNavigationTreeTabTitle", "AeonixNavigation"))
		.SetTooltipText(LOCTEXT("AeonixNavigationTreeTabTooltip", "View and manage Aeonix navigation elements"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Volume"));

	// Register menu extension
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([this]()
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		FToolMenuSection& Section = Menu->FindOrAddSection("LevelEditor");

		Section.AddMenuEntry(
			"AeonixNavigationTree",
			LOCTEXT("AeonixNavigationTreeMenuLabel", "AeonixNavigation"),
			LOCTEXT("AeonixNavigationTreeMenuTooltip", "Open the Aeonix Navigation panel"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Volume"),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(AeonixNavigationTreeTabName);
			}))
		);
	}));
}

void FAeonixEditorModule::ShutdownModule()
{
	UE_LOG(LogAeonixEditor, Log, TEXT("AeonixEditorModule: Log Ended"));

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AeonixNavigationTreeTabName);

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("AeonixBoundingVolume");
	}
}

TSharedRef<SDockTab> FAeonixEditorModule::SpawnNavigationTreeTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SAeonixNavigationTreeView)
		];
}

#undef LOCTEXT_NAMESPACE