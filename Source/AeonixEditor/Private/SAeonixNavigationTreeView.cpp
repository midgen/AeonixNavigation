// Copyright 2024 Chris Ashworth

#include "SAeonixNavigationTreeView.h"

#include "Actor/AeonixBoundingVolume.h"
#include "Actor/AeonixModifierVolume.h"
#include "Component/AeonixDynamicObstacleComponent.h"
#include "Subsystem/AeonixSubsystem.h"
#include "Data/AeonixHandleTypes.h"
#include "Data/AeonixGenerationParameters.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Selection.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "AeonixNavigationTreeView"

// FAeonixTreeItem implementation
bool FAeonixTreeItem::IsValid() const
{
	switch (Type)
	{
	case EAeonixTreeItemType::World:
		return true;
	case EAeonixTreeItemType::BoundingVolume:
		return BoundingVolume.IsValid();
	case EAeonixTreeItemType::ModifierVolume:
		return ModifierVolume.IsValid();
	case EAeonixTreeItemType::DynamicComponent:
		return DynamicComponent.IsValid();
	}
	return false;
}

FString FAeonixTreeItem::GetIconName() const
{
	switch (Type)
	{
	case EAeonixTreeItemType::World:
		return TEXT("WorldSettings.Tab");
	case EAeonixTreeItemType::BoundingVolume:
		return TEXT("ClassIcon.Volume");
	case EAeonixTreeItemType::ModifierVolume:
		return TEXT("ClassIcon.TriggerVolume");
	case EAeonixTreeItemType::DynamicComponent:
		return TEXT("ClassIcon.MovementComponent");
	}
	return TEXT("ClassIcon.Actor");
}

AActor* FAeonixTreeItem::GetActor() const
{
	switch (Type)
	{
	case EAeonixTreeItemType::BoundingVolume:
		return BoundingVolume.Get();
	case EAeonixTreeItemType::ModifierVolume:
		return ModifierVolume.Get();
	case EAeonixTreeItemType::DynamicComponent:
		if (DynamicComponent.IsValid())
		{
			return DynamicComponent->GetOwner();
		}
		break;
	default:
		break;
	}
	return nullptr;
}

// SAeonixNavigationTreeView implementation
void SAeonixNavigationTreeView::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		// Toolbar
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Refresh", "Refresh"))
				.ToolTipText(LOCTEXT("RefreshTooltip", "Refresh the tree view with current world data"))
				.OnClicked(this, &SAeonixNavigationTreeView::OnRefreshClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ExpandAll", "Expand All"))
				.ToolTipText(LOCTEXT("ExpandAllTooltip", "Expand all items in the tree"))
				.OnClicked(this, &SAeonixNavigationTreeView::OnExpandAllClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("CollapseAll", "Collapse All"))
				.ToolTipText(LOCTEXT("CollapseAllTooltip", "Collapse all items in the tree"))
				.OnClicked(this, &SAeonixNavigationTreeView::OnCollapseAllClicked)
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SSpacer)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("TerminatePathfinds", "Terminate All Pathfinds"))
				.ToolTipText(LOCTEXT("TerminatePathfindsTooltip", "Cancel all in-progress pathfinding tasks and reset state"))
				.ButtonStyle(FAppStyle::Get(), "FlatButton.Danger")
				.OnClicked(this, &SAeonixNavigationTreeView::OnTerminatePathfindsClicked)
			]
		]

		// Separator
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]

		// Tree view
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(4.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SAssignNew(TreeView, STreeView<FAeonixTreeItemPtr>)
				.TreeItemsSource(&RootItems)
				.OnGenerateRow(this, &SAeonixNavigationTreeView::OnGenerateRow)
				.OnGetChildren(this, &SAeonixNavigationTreeView::OnGetChildren)
				.OnSelectionChanged(this, &SAeonixNavigationTreeView::OnSelectionChanged)
				.OnMouseButtonDoubleClick(this, &SAeonixNavigationTreeView::OnItemDoubleClick)
				.SelectionMode(ESelectionMode::Single)
			]
		]

		// Pathfinding metrics panel
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PathfindingMetricsHeader", "Pathfinding Metrics"))
					.Font(FAppStyle::GetFontStyle("BoldFont"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(this, &SAeonixNavigationTreeView::GetPathfindMetricsText)
						.Font(FAppStyle::GetFontStyle("SmallFont"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(this, &SAeonixNavigationTreeView::GetWorkerPoolStatusText)
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 1.0f, 0.7f)))
					]
				]
			]
		]

		// Generation metrics panel
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("GenerationMetricsHeader", "Generation Metrics"))
					.Font(FAppStyle::GetFontStyle("BoldFont"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text(this, &SAeonixNavigationTreeView::GetGenerationMetricsText)
					.Font(FAppStyle::GetFontStyle("SmallFont"))
				]
			]
		]

	];

	// Initial population
	RefreshTreeData();

	// Subscribe to registration changes for auto-refresh
	UAeonixSubsystem* Subsystem = GetSubsystem();
	if (Subsystem)
	{
		Subsystem->GetOnRegistrationChanged().AddSP(this, &SAeonixNavigationTreeView::OnRegistrationChanged);
	}

	// Register active timer for PIE updates (refreshes tree every 0.5 seconds during PIE)
	PIERefreshTimerHandle = RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SAeonixNavigationTreeView::UpdateDuringPIE));
}

SAeonixNavigationTreeView::~SAeonixNavigationTreeView()
{
	// Unregister active timer
	if (PIERefreshTimerHandle.IsValid())
	{
		UnRegisterActiveTimer(PIERefreshTimerHandle.ToSharedRef());
		PIERefreshTimerHandle.Reset();
	}

	// Unsubscribe from registration changes
	UAeonixSubsystem* Subsystem = GetSubsystem();
	if (Subsystem)
	{
		Subsystem->GetOnRegistrationChanged().RemoveAll(this);
	}
}

TSharedRef<ITableRow> SAeonixNavigationTreeView::OnGenerateRow(FAeonixTreeItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	FSlateColor TextColor = FSlateColor::UseForeground();

	// Gray out invalid items
	if (!Item->IsValid() && Item->Type != EAeonixTreeItemType::World)
	{
		TextColor = FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f));
	}

	// Create the row content
	TSharedRef<SHorizontalBox> RowContent = SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(FAppStyle::GetBrush(*Item->GetIconName()))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->DisplayName))
			.ColorAndOpacity(TextColor)
		];

	// Add generation strategy icon for bounding volumes
	if (Item->Type == EAeonixTreeItemType::BoundingVolume && Item->IsValid())
	{
		AAeonixBoundingVolume* Volume = Item->BoundingVolume.Get();
		if (Volume)
		{
			const bool bUsesBaked = Volume->GenerationParameters.GenerationStrategy == ESVOGenerationStrategy::UseBaked;
			RowContent->AddSlot()
				.AutoWidth()
				.Padding(4.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush(bUsesBaked ? "Icons.Save" : "Icons.Refresh"))
					.ToolTipText(bUsesBaked ? LOCTEXT("BakedDataTooltip", "Using Baked Data") : LOCTEXT("RuntimeGenTooltip", "Generate OnBeginPlay"))
					.ColorAndOpacity(bUsesBaked ? FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f)) : FSlateColor(FLinearColor(0.8f, 0.6f, 0.2f)))
				];
		}
	}

	// Add action buttons based on item type
	if (Item->Type == EAeonixTreeItemType::BoundingVolume && Item->IsValid())
	{
		RowContent->AddSlot()
			.AutoWidth()
			.Padding(4.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(LOCTEXT("RegenerateVolumeTooltip", "Regenerate navigation data for this volume"))
				.OnClicked(this, &SAeonixNavigationTreeView::OnRegenerateVolumeClicked, Item)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Refresh"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			];
	}
	else if (Item->Type == EAeonixTreeItemType::ModifierVolume && Item->IsValid())
	{
		RowContent->AddSlot()
			.AutoWidth()
			.Padding(4.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(LOCTEXT("RegenerateModifierTooltip", "Regenerate dynamic region for this modifier"))
				.OnClicked(this, &SAeonixNavigationTreeView::OnRegenerateModifierClicked, Item)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Refresh"))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.8f, 1.0f)))
				]
			];
	}

	return SNew(STableRow<FAeonixTreeItemPtr>, OwnerTable)
		[
			RowContent
		];
}

void SAeonixNavigationTreeView::OnGetChildren(FAeonixTreeItemPtr Item, TArray<FAeonixTreeItemPtr>& OutChildren)
{
	OutChildren = Item->Children;
}

void SAeonixNavigationTreeView::OnSelectionChanged(FAeonixTreeItemPtr Item, ESelectInfo::Type SelectInfo)
{
	if (!Item.IsValid() || SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	// Select the actor in the editor
	AActor* Actor = Item->GetActor();
	if (Actor && GEditor)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(Actor, true, true, true);
	}
}

void SAeonixNavigationTreeView::OnItemDoubleClick(FAeonixTreeItemPtr Item)
{
	if (!Item.IsValid())
	{
		return;
	}

	// Focus on the actor in the viewport
	AActor* Actor = Item->GetActor();
	if (Actor && GEditor)
	{
		GEditor->MoveViewportCamerasToActor(*Actor, false);
	}
}

void SAeonixNavigationTreeView::RefreshTreeData()
{
	RootItems.Empty();
	ExpandedItems.Empty();

	UWorld* World = GetTargetWorld();
	if (World)
	{
		PopulateTreeFromWorld(World);
	}

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}

void SAeonixNavigationTreeView::PopulateTreeFromWorld(UWorld* World)
{
	if (!World)
	{
		return;
	}

	// Create world root
	FAeonixTreeItemPtr WorldItem = MakeShared<FAeonixTreeItem>(EAeonixTreeItemType::World, World->GetMapName());
	RootItems.Add(WorldItem);

	// Get subsystem data
	UAeonixSubsystem* Subsystem = World->GetSubsystem<UAeonixSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	// We need to iterate all bounding volumes in the world
	// Since RegisteredVolumes is private, we'll iterate actors directly
	for (TActorIterator<AAeonixBoundingVolume> It(World); It; ++It)
	{
		AAeonixBoundingVolume* Volume = *It;
		if (!Volume)
		{
			continue;
		}

		// Create volume item
		FString VolumeName = Volume->GetActorNameOrLabel();
		FAeonixTreeItemPtr VolumeItem = MakeShared<FAeonixTreeItem>(EAeonixTreeItemType::BoundingVolume, VolumeName);
		VolumeItem->BoundingVolume = Volume;
		VolumeItem->Parent = WorldItem;
		WorldItem->Children.Add(VolumeItem);

		// Find modifier volumes that overlap with this bounding volume
		for (TActorIterator<AAeonixModifierVolume> ModIt(World); ModIt; ++ModIt)
		{
			AAeonixModifierVolume* ModifierVolume = *ModIt;
			if (!ModifierVolume)
			{
				continue;
			}

			// Check if this modifier is within the bounding volume's bounds
			FBox VolumeBounds = Volume->GetComponentsBoundingBox();
			FVector ModifierLocation = ModifierVolume->GetActorLocation();

			if (VolumeBounds.IsInsideOrOn(ModifierLocation))
			{
				FString ModifierName = ModifierVolume->GetActorNameOrLabel();
				FAeonixTreeItemPtr ModifierItem = MakeShared<FAeonixTreeItem>(EAeonixTreeItemType::ModifierVolume, ModifierName);
				ModifierItem->ModifierVolume = ModifierVolume;
				ModifierItem->Parent = VolumeItem;
				VolumeItem->Children.Add(ModifierItem);
			}
		}

	}

	// Add registered dynamic obstacles from the subsystem to their respective volumes
	if (Subsystem)
	{
		for (UAeonixDynamicObstacleComponent* DynamicComp : Subsystem->GetRegisteredDynamicObstacles())
		{
			if (!DynamicComp || !IsValid(DynamicComp))
			{
				continue;
			}

			AActor* Actor = DynamicComp->GetOwner();
			if (!Actor)
			{
				continue;
			}

			// Find which bounding volume this component belongs to
			FVector CompLocation = Actor->GetActorLocation();
			for (const FAeonixTreeItemPtr& VolumeItem : WorldItem->Children)
			{
				if (VolumeItem->Type == EAeonixTreeItemType::BoundingVolume && VolumeItem->BoundingVolume.IsValid())
				{
					AAeonixBoundingVolume* Volume = VolumeItem->BoundingVolume.Get();
					FBox VolumeBounds = Volume->GetComponentsBoundingBox();

					if (VolumeBounds.IsInsideOrOn(CompLocation))
					{
						FString CompName = FString::Printf(TEXT("%s (%s)"), *Actor->GetActorNameOrLabel(), *DynamicComp->GetName());
						FAeonixTreeItemPtr DynamicItem = MakeShared<FAeonixTreeItem>(EAeonixTreeItemType::DynamicComponent, CompName);
						DynamicItem->DynamicComponent = DynamicComp;
						DynamicItem->Parent = VolumeItem;
						VolumeItem->Children.Add(DynamicItem);
						break; // Component added to a volume, move to next component
					}
				}
			}
		}
	}

	// Expand all items by default
	for (const FAeonixTreeItemPtr& Item : RootItems)
	{
		ExpandItemRecursive(Item);
	}
}

UAeonixSubsystem* SAeonixNavigationTreeView::GetSubsystem() const
{
	UWorld* World = GetTargetWorld();
	if (World)
	{
		return World->GetSubsystem<UAeonixSubsystem>();
	}
	return nullptr;
}

UWorld* SAeonixNavigationTreeView::GetTargetWorld() const
{
	// Prefer PIE world if available
	if (GEditor && GEditor->GetPIEWorldContext())
	{
		return GEditor->GetPIEWorldContext()->World();
	}

	// Fall back to editor world
	if (GEditor)
	{
		return GEditor->GetEditorWorldContext().World();
	}

	return nullptr;
}

FReply SAeonixNavigationTreeView::OnRefreshClicked()
{
	RefreshTreeData();
	return FReply::Handled();
}

FReply SAeonixNavigationTreeView::OnTerminatePathfindsClicked()
{
	UAeonixSubsystem* Subsystem = GetSubsystem();
	if (Subsystem)
	{
		Subsystem->CompleteAllPendingPathfindingTasks();
	}
	return FReply::Handled();
}

FReply SAeonixNavigationTreeView::OnExpandAllClicked()
{
	ExpandAllItems();
	return FReply::Handled();
}

FReply SAeonixNavigationTreeView::OnCollapseAllClicked()
{
	CollapseAllItems();
	return FReply::Handled();
}

void SAeonixNavigationTreeView::ExpandAllItems()
{
	for (const FAeonixTreeItemPtr& Item : RootItems)
	{
		ExpandItemRecursive(Item);
	}
}

void SAeonixNavigationTreeView::CollapseAllItems()
{
	if (TreeView.IsValid())
	{
		for (const FAeonixTreeItemPtr& Item : RootItems)
		{
			TreeView->SetItemExpansion(Item, false);
			for (const FAeonixTreeItemPtr& Child : Item->Children)
			{
				TreeView->SetItemExpansion(Child, false);
			}
		}
	}
}

void SAeonixNavigationTreeView::ExpandItemRecursive(FAeonixTreeItemPtr Item)
{
	if (!Item.IsValid() || !TreeView.IsValid())
	{
		return;
	}

	TreeView->SetItemExpansion(Item, true);

	for (const FAeonixTreeItemPtr& Child : Item->Children)
	{
		ExpandItemRecursive(Child);
	}
}

FReply SAeonixNavigationTreeView::OnRegenerateVolumeClicked(FAeonixTreeItemPtr Item)
{
	if (Item.IsValid() && Item->BoundingVolume.IsValid())
	{
		Item->BoundingVolume->Generate();
	}
	return FReply::Handled();
}

FReply SAeonixNavigationTreeView::OnRegenerateModifierClicked(FAeonixTreeItemPtr Item)
{
	if (Item.IsValid() && Item->ModifierVolume.IsValid())
	{
		// Find the parent bounding volume
		FAeonixTreeItemPtr ParentItem = Item->Parent.Pin();
		if (ParentItem.IsValid() && ParentItem->BoundingVolume.IsValid())
		{
			// Regenerate the dynamic region for this modifier
			ParentItem->BoundingVolume->RegenerateDynamicSubregion(Item->ModifierVolume->DynamicRegionId);
		}
	}
	return FReply::Handled();
}

void SAeonixNavigationTreeView::OnRegistrationChanged()
{
	RefreshTreeData();
}

FText SAeonixNavigationTreeView::GetPathfindMetricsText() const
{
	UAeonixSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return LOCTEXT("NoSubsystem", "Subsystem unavailable");
	}

	const FAeonixLoadMetrics& Metrics = Subsystem->GetLoadMetrics();

	return FText::Format(
		LOCTEXT("PathfindMetricsText", "Pending: {0} | Active: {1} | Completed: {2} | Failed: {3} | Cancelled: {4} | Avg Time: {5}μs"),
		FText::AsNumber(Metrics.PendingPathfinds.load()),
		FText::AsNumber(Metrics.ActivePathfinds.load()),
		FText::AsNumber(Metrics.CompletedPathfindsTotal.load()),
		FText::AsNumber(Metrics.FailedPathfindsTotal.load()),
		FText::AsNumber(Metrics.CancelledPathfindsTotal.load()),
		FText::AsNumber(FMath::RoundToInt(Metrics.AveragePathfindTimeMs.Load() * 1000.0f))
	);
}

FText SAeonixNavigationTreeView::GetWorkerPoolStatusText() const
{
	UAeonixSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return FText::GetEmpty();
	}

	const FAeonixLoadMetrics& Metrics = Subsystem->GetLoadMetrics();
	const int32 ActiveWorkers = Metrics.ActivePathfinds.load();

	if (ActiveWorkers > 0)
	{
		return FText::Format(
			LOCTEXT("WorkerPoolActive", "Workers Active: {0}"),
			FText::AsNumber(ActiveWorkers)
		);
	}

	return LOCTEXT("WorkerPoolIdle", "Workers: Idle");
}

FText SAeonixNavigationTreeView::GetGenerationMetricsText() const
{
	UAeonixSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return LOCTEXT("NoSubsystemGen", "Subsystem unavailable");
	}

	// Aggregate metrics from all registered bounding volumes
	int32 TotalLayers = 0;
	int32 TotalNodes = 0;
	int32 TotalLeafNodes = 0;
	int32 TotalDynamicRegions = 0;
	int32 TotalMemoryBytes = 0;

	const TArray<FAeonixBoundingVolumeHandle>& RegisteredVolumes = Subsystem->GetRegisteredVolumes();

	for (const FAeonixBoundingVolumeHandle& VolumeHandle : RegisteredVolumes)
	{
		if (AAeonixBoundingVolume* Volume = VolumeHandle.VolumeHandle.Get())
		{
			if (Volume->HasData())
			{
				const FAeonixData& NavData = Volume->GetNavData();

				// Count layers (take max across volumes)
				TotalLayers = FMath::Max(TotalLayers, static_cast<int32>(NavData.OctreeData.GetNumLayers()));

				// Count leaf nodes
				TotalLeafNodes += NavData.OctreeData.LeafNodes.Num();

				// Count total nodes across all layers
				for (int32 i = 0; i < NavData.OctreeData.GetNumLayers(); ++i)
				{
					TotalNodes += NavData.OctreeData.GetLayer(i).Num();
				}

				// Count dynamic regions
				TotalDynamicRegions += NavData.GetParams().DynamicRegionBoxes.Num();

				// Sum memory usage
				TotalMemoryBytes += NavData.OctreeData.GetSize();
			}
		}
	}

	// Get average regeneration time from load metrics
	const FAeonixLoadMetrics& Metrics = Subsystem->GetLoadMetrics();
	const float AvgRegenTimeMs = Metrics.AverageRegenTimeMs.Load();

	return FText::Format(
		LOCTEXT("GenerationMetricsText", "Layers: {0} | Nodes: {1} | Leaves: {2} | Dynamic Regions: {3} | Memory: {4} KB | Avg Regen: {5}μs"),
		FText::AsNumber(TotalLayers),
		FText::AsNumber(TotalNodes),
		FText::AsNumber(TotalLeafNodes),
		FText::AsNumber(TotalDynamicRegions),
		FText::AsNumber(TotalMemoryBytes / 1024),
		FText::AsNumber(FMath::RoundToInt(AvgRegenTimeMs * 1000.0f))
	);
}

EActiveTimerReturnType SAeonixNavigationTreeView::UpdateDuringPIE(double InCurrentTime, float InDeltaTime)
{
	const bool bIsInPIE = GEditor && GEditor->GetPIEWorldContext();

	// Auto-refresh during PIE to show dynamic changes
	if (bIsInPIE)
	{
		// Safety check: ensure subsystem is valid and not being destroyed
		UAeonixSubsystem* Subsystem = GetSubsystem();
		if (Subsystem && IsValid(Subsystem) && !Subsystem->HasAnyFlags(RF_BeginDestroyed))
		{
			RefreshTreeData();
		}
		bWasInPIE = true;
	}
	// Detect transition from PIE to editor - refresh to show editor world
	else if (bWasInPIE)
	{
		RefreshTreeData();
		bWasInPIE = false;
	}

	// Continue ticking
	return EActiveTimerReturnType::Continue;
}

#undef LOCTEXT_NAMESPACE
