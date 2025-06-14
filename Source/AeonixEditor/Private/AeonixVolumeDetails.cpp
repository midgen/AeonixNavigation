// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include <AeonixEditor/Private/AeonixVolumeDetails.h>
#include <AeonixNavigation/Public/Actor/AeonixBoundingVolume.h>

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
	
	TSharedPtr<IPropertyHandle> debugDistanceProperty = DetailBuilder.GetProperty("myDebugDistance");
	TSharedPtr<IPropertyHandle> showVoxelProperty = DetailBuilder.GetProperty("myShowVoxels");
	TSharedPtr<IPropertyHandle> showVoxelLeafProperty = DetailBuilder.GetProperty("myShowLeafVoxels");
	TSharedPtr<IPropertyHandle> showMortonCodesProperty = DetailBuilder.GetProperty("myShowMortonCodes");
	TSharedPtr<IPropertyHandle> showNeighbourLinksProperty = DetailBuilder.GetProperty("myShowNeighbourLinks");
	TSharedPtr<IPropertyHandle> showParentChildLinksProperty = DetailBuilder.GetProperty("myShowParentChildLinks");
	TSharedPtr<IPropertyHandle> voxelPowerProperty = DetailBuilder.GetProperty("myVoxelPower");
	TSharedPtr<IPropertyHandle> collisionChannelProperty = DetailBuilder.GetProperty("myCollisionChannel");
	TSharedPtr<IPropertyHandle> clearanceProperty = DetailBuilder.GetProperty("myClearance");
	TSharedPtr<IPropertyHandle> generationStrategyProperty = DetailBuilder.GetProperty("myGenerationStrategy");
	TSharedPtr<IPropertyHandle> numLayersProperty = DetailBuilder.GetProperty("myNumLayers");
	TSharedPtr<IPropertyHandle> numBytesProperty = DetailBuilder.GetProperty("myNumBytes");

	debugDistanceProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Distance", "Debug Distance"));
	showVoxelProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Voxels", "Debug Voxels"));
	showVoxelLeafProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Leaf Voxels", "Debug Leaf Voxels"));
	showMortonCodesProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Morton Codes", "Debug Morton Codes"));
	showNeighbourLinksProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Debug Links", "Debug Links"));
	showParentChildLinksProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Parent Child Links", "Parent Child Links"));
	voxelPowerProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Layers", "Layers"));
	voxelPowerProperty->SetInstanceMetaData("UIMin", TEXT("1"));
	voxelPowerProperty->SetInstanceMetaData("UIMax", TEXT("12"));
	collisionChannelProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Collision Channel", "Collision Channel"));
	clearanceProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Clearance", "Clearance"));
	generationStrategyProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Generation Strategy", "Generation Strategy"));
	numLayersProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Num Layers", "Num Layers"));
	numBytesProperty->SetPropertyDisplayName(NSLOCTEXT("SVO Volume", "Num Bytes", "Num Bytes"));

	navigationCategory.AddProperty(voxelPowerProperty);
	navigationCategory.AddProperty(collisionChannelProperty);
	navigationCategory.AddProperty(clearanceProperty);
	navigationCategory.AddProperty(generationStrategyProperty);
	navigationCategory.AddProperty(numLayersProperty);
	navigationCategory.AddProperty(numBytesProperty);

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

#undef LOCTEXT_NAMESPACE
