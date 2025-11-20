// Copyright 2024 Chris Ashworth

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class AAeonixBoundingVolume;
class AAeonixModifierVolume;
class UAeonixDynamicObstacleComponent;
class UAeonixSubsystem;

/**
 * Tree item types for the navigation hierarchy
 */
enum class EAeonixTreeItemType : uint8
{
	World,
	BoundingVolume,
	ModifierVolume,
	DynamicComponent
};

/**
 * Data structure representing a single item in the navigation tree view
 */
struct FAeonixTreeItem : TSharedFromThis<FAeonixTreeItem>
{
	FAeonixTreeItem() = default;

	FAeonixTreeItem(EAeonixTreeItemType InType, const FString& InDisplayName)
		: Type(InType)
		, DisplayName(InDisplayName)
	{}

	EAeonixTreeItemType Type = EAeonixTreeItemType::World;
	FString DisplayName;

	// Object references based on type
	TWeakObjectPtr<AAeonixBoundingVolume> BoundingVolume;
	TWeakObjectPtr<AAeonixModifierVolume> ModifierVolume;
	TWeakObjectPtr<UAeonixDynamicObstacleComponent> DynamicComponent;

	// Tree structure
	TWeakPtr<FAeonixTreeItem> Parent;
	TArray<TSharedPtr<FAeonixTreeItem>> Children;

	bool IsValid() const;
	FString GetIconName() const;
	AActor* GetActor() const;
};

typedef TSharedPtr<FAeonixTreeItem> FAeonixTreeItemPtr;

/**
 * Slate widget that displays a tree view of Aeonix navigation elements:
 * World -> Bounding Volumes -> Modifier Volumes / Dynamic Components
 */
class SAeonixNavigationTreeView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAeonixNavigationTreeView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SAeonixNavigationTreeView();

private:
	// Tree view generation
	TSharedRef<ITableRow> OnGenerateRow(FAeonixTreeItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(FAeonixTreeItemPtr Item, TArray<FAeonixTreeItemPtr>& OutChildren);
	void OnSelectionChanged(FAeonixTreeItemPtr Item, ESelectInfo::Type SelectInfo);
	void OnItemDoubleClick(FAeonixTreeItemPtr Item);

	// Data management
	void RefreshTreeData();
	void PopulateTreeFromWorld(UWorld* World);
	UAeonixSubsystem* GetSubsystem() const;
	UWorld* GetTargetWorld() const;

	// PIE auto-refresh timer
	EActiveTimerReturnType UpdateDuringPIE(double InCurrentTime, float InDeltaTime);

	// Button callbacks
	FReply OnRefreshClicked();
	FReply OnTerminatePathfindsClicked();
	FReply OnExpandAllClicked();
	FReply OnCollapseAllClicked();

	// Item action callbacks
	FReply OnRegenerateVolumeClicked(FAeonixTreeItemPtr Item);
	FReply OnRegenerateModifierClicked(FAeonixTreeItemPtr Item);

	// Registration change callback
	void OnRegistrationChanged();

	// Status text
	FText GetStatusText() const;
	FText GetPendingTasksText() const;
	FText GetPathfindMetricsText() const;
	FText GetWorkerPoolStatusText() const;
	FText GetGenerationMetricsText() const;

	// Tree state management
	void ExpandAllItems();
	void CollapseAllItems();
	void ExpandItemRecursive(FAeonixTreeItemPtr Item);

private:
	TSharedPtr<STreeView<FAeonixTreeItemPtr>> TreeView;
	TArray<FAeonixTreeItemPtr> RootItems;
	TSet<FAeonixTreeItemPtr> ExpandedItems;

	// Cached counts for status display
	int32 CachedVolumeCount = 0;
	int32 CachedModifierCount = 0;
	int32 CachedDynamicCount = 0;
};
