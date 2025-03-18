#pragma once


#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class AAeonixBoundingVolume;

class FAeonixVolumeDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;


	FReply OnUpdateVolume();

	FReply OnClearVolumeClick();

private:
	TWeakObjectPtr<AAeonixBoundingVolume> myVolume;
};
