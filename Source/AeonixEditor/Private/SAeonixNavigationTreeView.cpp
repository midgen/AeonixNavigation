// Copyright 2024 Chris Ashworth

#include "SAeonixNavigationTreeView.h"

#include "Actor/AeonixBoundingVolume.h"
#include "Actor/AeonixModifierVolume.h"
#include "Component/AeonixDynamicObstacleComponent.h"
#include "Subsystem/AeonixSubsystem.h"
#include "Data/AeonixHandleTypes.h"

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

		// Status bar
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(this, &SAeonixNavigationTreeView::GetStatusText)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(this, &SAeonixNavigationTreeView::GetPendingTasksText)
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.7f, 0.0f)))
			]
		]
	];

	// Initial population
	RefreshTreeData();
}

SAeonixNavigationTreeView::~SAeonixNavigationTreeView()
{
}

TSharedRef<ITableRow> SAeonixNavigationTreeView::OnGenerateRow(FAeonixTreeItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	FSlateColor TextColor = FSlateColor::UseForeground();

	// Gray out invalid items
	if (!Item->IsValid() && Item->Type != EAeonixTreeItemType::World)
	{
		TextColor = FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f));
	}

	return SNew(STableRow<FAeonixTreeItemPtr>, OwnerTable)
		[
			SNew(SHorizontalBox)

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
			]
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
	CachedVolumeCount = 0;
	CachedModifierCount = 0;
	CachedDynamicCount = 0;

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

		CachedVolumeCount++;

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
				CachedModifierCount++;

				FString ModifierName = ModifierVolume->GetActorNameOrLabel();
				FAeonixTreeItemPtr ModifierItem = MakeShared<FAeonixTreeItem>(EAeonixTreeItemType::ModifierVolume, ModifierName);
				ModifierItem->ModifierVolume = ModifierVolume;
				ModifierItem->Parent = VolumeItem;
				VolumeItem->Children.Add(ModifierItem);
			}
		}

		// Find dynamic obstacle components within this bounding volume
		for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
		{
			AActor* Actor = *ActorIt;
			if (!Actor)
			{
				continue;
			}

			TArray<UAeonixDynamicObstacleComponent*> DynamicComponents;
			Actor->GetComponents<UAeonixDynamicObstacleComponent>(DynamicComponents);

			for (UAeonixDynamicObstacleComponent* DynamicComp : DynamicComponents)
			{
				if (!DynamicComp)
				{
					continue;
				}

				// Check if this component's owner is within the bounding volume
				FBox VolumeBounds = Volume->GetComponentsBoundingBox();
				FVector CompLocation = Actor->GetActorLocation();

				if (VolumeBounds.IsInsideOrOn(CompLocation))
				{
					CachedDynamicCount++;

					FString CompName = FString::Printf(TEXT("%s (%s)"), *Actor->GetActorNameOrLabel(), *DynamicComp->GetName());
					FAeonixTreeItemPtr DynamicItem = MakeShared<FAeonixTreeItem>(EAeonixTreeItemType::DynamicComponent, CompName);
					DynamicItem->DynamicComponent = DynamicComp;
					DynamicItem->Parent = VolumeItem;
					VolumeItem->Children.Add(DynamicItem);
				}
			}
		}
	}

	// Expand root by default
	if (RootItems.Num() > 0)
	{
		TreeView->SetItemExpansion(RootItems[0], true);
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

FText SAeonixNavigationTreeView::GetStatusText() const
{
	return FText::Format(
		LOCTEXT("StatusText", "Volumes: {0} | Modifiers: {1} | Dynamic: {2}"),
		FText::AsNumber(CachedVolumeCount),
		FText::AsNumber(CachedModifierCount),
		FText::AsNumber(CachedDynamicCount)
	);
}

FText SAeonixNavigationTreeView::GetPendingTasksText() const
{
	UAeonixSubsystem* Subsystem = GetSubsystem();
	if (Subsystem)
	{
		size_t PendingTasks = Subsystem->GetNumberOfPendingTasks();
		if (PendingTasks > 0)
		{
			return FText::Format(
				LOCTEXT("PendingTasksText", "Pending Pathfinds: {0}"),
				FText::AsNumber(static_cast<int32>(PendingTasks))
			);
		}
	}
	return FText::GetEmpty();
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

#undef LOCTEXT_NAMESPACE
