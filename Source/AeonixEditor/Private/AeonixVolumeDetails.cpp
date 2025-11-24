// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AeonixVolumeDetails.h"
#include "Actor/AeonixBoundingVolume.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "Components/BrushComponent.h"

#define LOCTEXT_NAMESPACE "AeonixVolumeDetails"

static const FName AeonixCategoryName(TEXT("Aeonix"));

TSharedRef<IDetailCustomization> FAeonixVolumeDetails::MakeInstance()
{
	return MakeShareable( new FAeonixVolumeDetails);
}

void FAeonixVolumeDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	TSharedPtr<IPropertyHandle> PrimaryTickProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UActorComponent, PrimaryComponentTick));

	// Defaults only show tick properties
	if (PrimaryTickProperty->IsValidHandle() && DetailBuilder.HasClassDefaultObject())
	{
		IDetailCategoryBuilder& TickCategory = DetailBuilder.EditCategory("ComponentTick");

		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bStartWithTickEnabled)));
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, TickInterval)));
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bTickEvenWhenPaused)), EPropertyLocation::Advanced);
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, bAllowTickOnDedicatedServer)), EPropertyLocation::Advanced);
		TickCategory.AddProperty(PrimaryTickProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTickFunction, TickGroup)), EPropertyLocation::Advanced);
	}

	PrimaryTickProperty->MarkHiddenByCustomization();

	IDetailCategoryBuilder& navigationCategory = DetailBuilder.EditCategory(AeonixCategoryName);
	
	TSharedPtr<IPropertyHandle> debugDistanceProperty = DetailBuilder.GetProperty("GenerationParameters.DebugDistance");
	TSharedPtr<IPropertyHandle> showVoxelProperty = DetailBuilder.GetProperty("GenerationParameters.ShowVoxels");
	TSharedPtr<IPropertyHandle> showVoxelLeafProperty = DetailBuilder.GetProperty("GenerationParameters.ShowLeafVoxels");
	TSharedPtr<IPropertyHandle> showMortonCodesProperty = DetailBuilder.GetProperty("GenerationParameters.ShowMortonCodes");
	TSharedPtr<IPropertyHandle> showNeighbourLinksProperty = DetailBuilder.GetProperty("GenerationParameters.ShowNeighbourLinks");
	TSharedPtr<IPropertyHandle> showParentChildLinksProperty = DetailBuilder.GetProperty("GenerationParameters.ShowParentChildLinks");
	TSharedPtr<IPropertyHandle> voxelPowerProperty = DetailBuilder.GetProperty("GenerationParameters.OctreeDepth");
	TSharedPtr<IPropertyHandle> collisionChannelProperty = DetailBuilder.GetProperty("GenerationParameters.CollisionChannel");
	TSharedPtr<IPropertyHandle> agentRadiusProperty = DetailBuilder.GetProperty("GenerationParameters.AgentRadius");
	TSharedPtr<IPropertyHandle> generationStrategyProperty = DetailBuilder.GetProperty("GenerationParameters.GenerationStrategy");

	debugDistanceProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Distance", "Debug Distance"));
	showVoxelProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Voxels", "Debug Voxels"));
	showVoxelLeafProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Leaf Voxels", "Debug Leaf Voxels"));
	showMortonCodesProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Morton Codes", "Debug Morton Codes"));
	showNeighbourLinksProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Links", "Debug Links"));
	showParentChildLinksProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Parent Child Links", "Parent Child Links"));
	voxelPowerProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Octree Depth", "Octree Depth"));
	voxelPowerProperty->SetToolTipText(NSLOCTEXT("SVO Volume", "OctreeDepthTooltip", "Controls octree subdivision depth. Higher values create more voxels for finer detail but use more memory. Creates OctreeDepth+1 hierarchical layers. Layer 0 has the smallest voxels. Typical range: 3-6 for human-scale navigation."));
	voxelPowerProperty->SetInstanceMetaData("UIMin", TEXT("1"));
	voxelPowerProperty->SetInstanceMetaData("UIMax", TEXT("12"));
	collisionChannelProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Collision Channel", "Collision Channel"));
	agentRadiusProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Agent Radius", "Agent Radius"));
	generationStrategyProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Generation Strategy", "Generation Strategy"));

	navigationCategory.AddProperty(voxelPowerProperty);
	navigationCategory.AddProperty(collisionChannelProperty);
	navigationCategory.AddProperty(agentRadiusProperty);
	navigationCategory.AddProperty(generationStrategyProperty);

	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailBuilder.GetSelectedObjects();

	for (int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex)
	{
		const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[ObjectIndex];
		if (CurrentObject.IsValid())
		{
			AAeonixBoundingVolume* currentVolume = Cast<AAeonixBoundingVolume>(CurrentObject.Get());
			if (currentVolume != NULL)
			{
				myVolume = currentVolume;
				break;
			}
		}
	}

	DetailBuilder.EditCategory(AeonixCategoryName)
		.AddCustomRow(NSLOCTEXT("Aeonix", "Generate", "Generate"))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(NSLOCTEXT("Aeonix", "Generate", "Generate"))
		]
	.ValueContent()
		.MaxDesiredWidth(125.f)
		.MinDesiredWidth(125.f)
		[
			SNew(SButton)
			.ContentPadding(2)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.OnClicked(this, &FAeonixVolumeDetails::OnUpdateVolume)
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(NSLOCTEXT("Aeonix", "Generate", "Generate"))
		]
		];

	DetailBuilder.EditCategory(AeonixCategoryName)
		.AddCustomRow(NSLOCTEXT("Aeonix", "Clear", "Clear"))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(NSLOCTEXT("Aeonix", "Clear", "Clear"))
		]
	.ValueContent()
		.MaxDesiredWidth(125.f)
		.MinDesiredWidth(125.f)
		[
			SNew(SButton)
			.ContentPadding(2)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.OnClicked(this, &FAeonixVolumeDetails::OnClearVolumeClick)
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(NSLOCTEXT("Aeonix", "Clear", "Clear"))
		]
		];

	DetailBuilder.EditCategory(AeonixCategoryName)
		.AddCustomRow(NSLOCTEXT("Aeonix", "RegenerateDynamic", "Regenerate Dynamic Subregions"))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(NSLOCTEXT("Aeonix", "RegenerateDynamic", "Regenerate Dynamic Subregions"))
		]
	.ValueContent()
		.MaxDesiredWidth(125.f)
		.MinDesiredWidth(125.f)
		[
			SNew(SButton)
			.ContentPadding(2)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.OnClicked(this, &FAeonixVolumeDetails::OnRegenerateDynamicSubregions)
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(NSLOCTEXT("Aeonix", "RegenerateDynamic", "Regenerate Dynamic Subregions"))
		]
		];

	DetailBuilder.EditCategory(AeonixCategoryName).SetSortOrder(0);
	
	navigationCategory.AddProperty(debugDistanceProperty);
	navigationCategory.AddProperty(showVoxelProperty);
	navigationCategory.AddProperty(showVoxelLeafProperty);
	navigationCategory.AddProperty(showMortonCodesProperty);
	navigationCategory.AddProperty(showNeighbourLinksProperty);
	navigationCategory.AddProperty(showParentChildLinksProperty);
}

FReply FAeonixVolumeDetails::OnUpdateVolume()
{
	if (myVolume.IsValid())
	{		
		myVolume->Generate();
	}

	return FReply::Handled();
}

FReply FAeonixVolumeDetails::OnClearVolumeClick()
{
	if (myVolume.IsValid())
	{
		myVolume->ClearData();
	}

	return FReply::Handled();
}

FReply FAeonixVolumeDetails::OnRegenerateDynamicSubregions()
{
	if (myVolume.IsValid())
	{
		myVolume->RegenerateDynamicSubregions();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
