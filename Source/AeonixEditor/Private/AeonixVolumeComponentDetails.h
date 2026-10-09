#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class IPropertyUtilities;
class UAeonixVolumeComponent;

/**
 *  Fit / Generate / Bake buttons for UAeonixVolumeComponent.
 *
 *  Selected in a Blueprint editor the component is a template with no world, so each action runs on
 *  the Blueprint editor's preview instance (debug drawing shows in its viewport), else on a temporary
 *  instance spawned in the editor world, and copies the result (placement, size, depth, baked asset)
 *  back onto the template.
 */
class FAeonixVolumeComponentDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization

private:
	enum class EAction : uint8
	{
		Fit,
		Generate,
		Bake,
		Clear
	};

	FReply OnActionClicked(EAction Action);

	/** Runs an action on one selected component; a template runs on an instance of its Blueprint */
	static void RunAction(UAeonixVolumeComponent* Selected, EAction Action);

	/** Returns whether the action changed what a template should copy back */
	static bool RunOnInstance(UAeonixVolumeComponent* Instance, EAction Action, UObject* AssetContext);

	/** The template's instance in an open Blueprint editor's preview viewport, if any */
	static UAeonixVolumeComponent* FindPreviewInstance(const UAeonixVolumeComponent* Template);

	/** Generates and stores into the component's asset, creating one beside its owner if it has none */
	static void Bake(UAeonixVolumeComponent* Component, UObject* AssetContext);

	// Read on each click: a Blueprint recompile replaces the component templates the panel was built for
	TWeakPtr<IPropertyUtilities> PropertyUtilities;
};
