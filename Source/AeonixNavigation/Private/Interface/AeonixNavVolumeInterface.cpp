#include "Interface/AeonixNavVolumeInterface.h"

UObject* IAeonixNavVolumeInterface::GetVolumeObject() const
{
	return Cast<UObject>(const_cast<IAeonixNavVolumeInterface*>(this));
}

FString IAeonixNavVolumeInterface::GetVolumeName() const
{
	const UObject* Object = GetVolumeObject();
	return Object ? Object->GetName() : FString(TEXT("None"));
}
