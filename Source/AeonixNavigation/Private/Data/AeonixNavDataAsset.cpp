#include "Data/AeonixNavDataAsset.h"
#include "Data/AeonixCustomVersion.h"

void UAeonixNavDataAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FAeonixCustomVersion::GUID);
	// Unversioned archives (memory, undo) report -1 and carry the current format
	const int32 Version = Ar.IsLoading() ? Ar.CustomVer(FAeonixCustomVersion::GUID) : FAeonixCustomVersion::LatestVersion;
	if (Ar.IsObjectReferenceCollector() || (Version >= 0 && Version < FAeonixCustomVersion::NavDataAsset))
	{
		return;
	}

	FAeonixGenerationParameters Params = NavData.GetParams();
	Ar << NavData.OctreeData;
	Ar << Params.Origin;
	Ar << Params.Extents;
	Ar << Params.OctreeDepth;

	if (Ar.IsLoading())
	{
		NavData.UpdateGenerationParameters(Params);
	}
}

void UAeonixNavDataAsset::Store(const FAeonixData& Source)
{
	NavData.ResetForGeneration();
	NavData.OctreeData = Source.OctreeData;
	NavData.OctreeData.BlockedIndices.Empty();
	NavData.UpdateGenerationParameters(Source.GetParams());

	OctreeDepth = Source.GetParams().OctreeDepth;
	Extents = Source.GetParams().Extents;
	NumLeafNodes = NavData.OctreeData.LeafNodes.Num();
	NumBytes = NavData.OctreeData.GetSize();
}

void UAeonixNavDataAsset::Restore(FAeonixData& Target, const FAeonixGenerationParameters& VolumeParameters) const
{
	FAeonixGenerationParameters Params = VolumeParameters;
	Params.Origin = NavData.GetParams().Origin;
	Params.Extents = NavData.GetParams().Extents;
	Params.OctreeDepth = NavData.GetParams().OctreeDepth;

	Target.ResetForGeneration();
	Target.OctreeData = NavData.OctreeData;
	Target.UpdateGenerationParameters(Params);
}
