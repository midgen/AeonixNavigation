#pragma once

#include "CoreMinimal.h"

class UAeonixVolumeComponent;

/** Editor actions on a placed or previewed volume component instance, for the volume's details panel and for
 *  any editor tool that owns an instance of a Blueprint's volume */
struct AEONIXEDITOR_API FAeonixVolumeEditorUtils
{
	/** Generates and stores into the component's asset, creating one beside AssetContext if it has none, and
	 *  saves it. False if nothing was stored. */
	static bool Bake(UAeonixVolumeComponent* Component, UObject* AssetContext);

	/** Copies what Fit and Bake change on an instance (placement, size, depth, baked asset) onto its Blueprint
	 *  template, recorded into the caller's transaction, and marks the Blueprint modified */
	static void CopyToTemplate(const UAeonixVolumeComponent& Instance, UAeonixVolumeComponent& Template);
};
