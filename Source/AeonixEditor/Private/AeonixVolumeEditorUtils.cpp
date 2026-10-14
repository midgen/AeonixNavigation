#include "AeonixVolumeEditorUtils.h"
#include "AeonixEditor/AeonixEditor.h"
#include "Component/AeonixVolumeComponent.h"
#include "Data/AeonixNavDataAsset.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "FileHelpers.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"

bool FAeonixVolumeEditorUtils::Bake(UAeonixVolumeComponent* Component, UObject* AssetContext)
{
	if (!Component)
	{
		return false;
	}
	if (!Component->Generate())
	{
		UE_LOG(LogAeonixEditor, Warning, TEXT("Aeonix volume %s: generation failed, nothing baked"), *Component->GetName());
		return false;
	}

	UAeonixNavDataAsset* Asset = Component->NavDataAsset;
	if (!Asset && AssetContext)
	{
		const FString PackagePath = FPackageName::GetLongPackagePath(AssetContext->GetOutermost()->GetName());
		const FString AssetName = FString::Printf(TEXT("NAV_%s_%s"), *AssetContext->GetName(), *Component->GetName());
		const FString PackageName = PackagePath / AssetName;

		UPackage* Package = FPackageName::DoesPackageExist(PackageName) ? LoadPackage(nullptr, *PackageName, LOAD_NoWarn | LOAD_Quiet) : CreatePackage(*PackageName);
		if (!Package)
		{
			UE_LOG(LogAeonixEditor, Warning, TEXT("Aeonix volume %s: could not load or create %s, nothing baked"), *Component->GetName(), *PackageName);
			return false;
		}
		UObject* Existing = FindObject<UObject>(Package, *AssetName);
		Asset = Cast<UAeonixNavDataAsset>(Existing);
		if (Existing && !Asset)
		{
			UE_LOG(LogAeonixEditor, Error, TEXT("Aeonix volume %s: %s exists and is not navigation data; set Nav Data Asset by hand"), *Component->GetName(), *Existing->GetPathName());
			return false;
		}
		if (!Asset)
		{
			Asset = NewObject<UAeonixNavDataAsset>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Asset);
		}
	}

	if (!Asset)
	{
		UE_LOG(LogAeonixEditor, Warning, TEXT("Aeonix volume %s: no Nav Data Asset set and nowhere to create one, nothing baked"), *Component->GetName());
		return false;
	}

	Component->StoreToAsset(Asset);
	Asset->MarkPackageDirty();
	UEditorLoadingAndSavingUtils::SavePackages({Asset->GetOutermost()}, true);

	UE_LOG(LogAeonixEditor, Display, TEXT("Aeonix volume %s baked into %s: depth %d, %d leaf nodes, %d bytes"),
		*Component->GetName(), *Asset->GetPathName(), Asset->OctreeDepth, Asset->NumLeafNodes, Asset->NumBytes);
	return true;
}

void FAeonixVolumeEditorUtils::CopyToTemplate(const UAeonixVolumeComponent& Instance, UAeonixVolumeComponent& Template)
{
	Template.Modify();
	Template.HalfExtent = Instance.HalfExtent;
	Template.GenerationParameters.OctreeDepth = Instance.GenerationParameters.OctreeDepth;
	Template.NavDataAsset = Instance.NavDataAsset;
	Template.SetRelativeLocation_Direct(Instance.GetRelativeLocation());
	Template.PostEditChange();

	const UBlueprintGeneratedClass* BlueprintClass = Template.GetTypedOuter<UBlueprintGeneratedClass>();
	if (UBlueprint* Blueprint = BlueprintClass ? Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy) : nullptr)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
}
