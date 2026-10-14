#include "AeonixVolumeComponentDetails.h"
#include "AeonixVolumeEditorUtils.h"
#include "AeonixEditor/AeonixEditor.h"
#include "Component/AeonixVolumeComponent.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/UObjectIterator.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "AeonixVolumeComponentDetails"

static const FName AeonixCategoryName(TEXT("Aeonix"));

namespace
{
	UAeonixVolumeComponent* FindInstanceOfTemplate(AActor* Actor, const UAeonixVolumeComponent* Template)
	{
		const FString TemplateName = Template->GetName();
		TInlineComponentArray<UAeonixVolumeComponent*> Components(Actor);
		for (UAeonixVolumeComponent* Component : Components)
		{
			// SCS templates carry a _GEN_VARIABLE suffix the instances drop
			if (Component->GetArchetype() == Template || TemplateName == Component->GetName() + TEXT("_GEN_VARIABLE"))
			{
				return Component;
			}
		}
		return nullptr;
	}
}

TSharedRef<IDetailCustomization> FAeonixVolumeComponentDetails::MakeInstance()
{
	return MakeShareable(new FAeonixVolumeComponentDetails);
}

void FAeonixVolumeComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	PropertyUtilities = DetailBuilder.GetPropertyUtilities();

	auto MakeButton = [this](const FText& Label, const FText& ToolTip, EAction Action)
	{
		return SNew(SButton)
			.HAlign(HAlign_Center)
			.ToolTipText(ToolTip)
			.OnClicked(this, &FAeonixVolumeComponentDetails::OnActionClicked, Action)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(Label)
			];
	};

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(AeonixCategoryName);
	Category.SetSortOrder(0);
	Category.AddCustomRow(LOCTEXT("ActionsRow", "Aeonix Volume Actions"))
		.WholeRowContent()
		[
			SNew(SUniformGridPanel)
			.SlotPadding(2.f)
			+ SUniformGridPanel::Slot(0, 0)
			[
				MakeButton(LOCTEXT("Fit", "Fit"), LOCTEXT("FitTooltip", "Centre and size the cube around the collision source's blocking geometry, plus Fit Padding; picks Octree Depth from Target Leaf Voxel Size"), EAction::Fit)
			]
			+ SUniformGridPanel::Slot(1, 0)
			[
				MakeButton(LOCTEXT("Generate", "Generate"), LOCTEXT("GenerateTooltip", "Generate navigation data for preview (not saved)"), EAction::Generate)
			]
			+ SUniformGridPanel::Slot(2, 0)
			[
				MakeButton(LOCTEXT("Bake", "Bake"), LOCTEXT("BakeTooltip", "Generate and save into Nav Data Asset, creating one beside the owner if unset"), EAction::Bake)
			]
			+ SUniformGridPanel::Slot(3, 0)
			[
				MakeButton(LOCTEXT("Clear", "Clear"), LOCTEXT("ClearTooltip", "Discard generated navigation data"), EAction::Clear)
			]
		];
}

FReply FAeonixVolumeComponentDetails::OnActionClicked(EAction Action)
{
	const FScopedTransaction Transaction(LOCTEXT("AeonixVolumeAction", "Aeonix Volume"));
	const TSharedPtr<IPropertyUtilities> Utilities = PropertyUtilities.Pin();
	if (!Utilities)
	{
		return FReply::Handled();
	}
	for (const TWeakObjectPtr<UObject>& Object : Utilities->GetSelectedObjects())
	{
		if (UAeonixVolumeComponent* Component = Cast<UAeonixVolumeComponent>(Object.Get()))
		{
			RunAction(Component, Action);
		}
	}
	return FReply::Handled();
}

void FAeonixVolumeComponentDetails::RunAction(UAeonixVolumeComponent* Selected, EAction Action)
{
	if (!Selected->IsTemplate())
	{
		RunOnInstance(Selected, Action, Selected->GetOwner());
		return;
	}

	UBlueprintGeneratedClass* BlueprintClass = Selected->GetTypedOuter<UBlueprintGeneratedClass>();
	UBlueprint* Blueprint = BlueprintClass ? Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy) : nullptr;
	if (!Blueprint)
	{
		UE_LOG(LogAeonixEditor, Warning, TEXT("Aeonix volume %s: template is not part of a Blueprint, can't run it"), *Selected->GetName());
		return;
	}

	// The template has no world. Run on the Blueprint editor's preview instance, so debug drawing lands in its
	// viewport, or failing that on a temporary instance in the editor world. Either stays out of the undo
	// buffer; only the template edit below is recorded.
	TOptional<TGuardValue<ITransaction*>> NoUndo(InPlace, GUndo, nullptr);
	AActor* TemporaryActor = nullptr;
	UAeonixVolumeComponent* Instance = FindPreviewInstance(Selected);
	if (!Instance)
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World)
		{
			return;
		}
		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags = RF_Transient;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.bTemporaryEditorActor = true;
		SpawnParams.bHideFromSceneOutliner = true;
		TemporaryActor = World->SpawnActor<AActor>(BlueprintClass, FTransform::Identity, SpawnParams);
		Instance = TemporaryActor ? FindInstanceOfTemplate(TemporaryActor, Selected) : nullptr;
	}

	if (Instance)
	{
		const bool bChanged = RunOnInstance(Instance, Action, Blueprint);
		NoUndo.Reset();
		if (bChanged)
		{
			FAeonixVolumeEditorUtils::CopyToTemplate(*Instance, *Selected);
		}
	}
	else
	{
		UE_LOG(LogAeonixEditor, Warning, TEXT("Aeonix volume %s: no instance of it to run on"), *Selected->GetName());
	}

	if (TemporaryActor)
	{
		NoUndo.Reset();
		TGuardValue<ITransaction*> NoUndoOnDestroy(GUndo, nullptr);
		TemporaryActor->GetWorld()->DestroyActor(TemporaryActor, false, false);
	}
}

bool FAeonixVolumeComponentDetails::RunOnInstance(UAeonixVolumeComponent* Instance, EAction Action, UObject* AssetContext)
{
	switch (Action)
	{
	case EAction::Fit: return Instance->FitToCollisionSource();
	case EAction::Generate: Instance->Generate(); return false;
	case EAction::Bake: return FAeonixVolumeEditorUtils::Bake(Instance, AssetContext);
	case EAction::Clear: Instance->ClearData(); return false;
	}
	return false;
}

UAeonixVolumeComponent* FAeonixVolumeComponentDetails::FindPreviewInstance(const UAeonixVolumeComponent* Template)
{
	const UClass* BlueprintClass = Template->GetTypedOuter<UClass>();
	for (TObjectIterator<UAeonixVolumeComponent> It; It; ++It)
	{
		const UWorld* World = It->GetWorld();
		AActor* Owner = It->GetOwner();
		if (World && World->WorldType == EWorldType::EditorPreview && It->IsRegistered() && Owner && Owner->IsA(BlueprintClass)
			&& FindInstanceOfTemplate(Owner, Template) == *It)
		{
			return *It;
		}
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
