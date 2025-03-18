#pragma once

#include<AeonixNavigation/Public/Data/AeonixCustomVersion.h>
#include <Serialization/CustomVersion.h>

const FGuid FAeonixCustomVersion::GUID(0x2625B0F1, 0xBA46495F, 0xA0F4C769, 0xEC2F38C1);

FCustomVersionRegistration GAeonixAnimationCustomVersion(FAeonixCustomVersion::GUID, FAeonixCustomVersion::LatestVersion, TEXT("AeonixVer"));
