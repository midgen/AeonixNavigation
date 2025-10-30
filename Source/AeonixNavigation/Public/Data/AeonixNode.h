#pragma once

#include "Data/AeonixLink.h"
#include "Data/AeonixDefines.h"

struct AEONIXNAVIGATION_API AeonixNode
{
	mortoncode_t Code;

	AeonixLink Parent;
	AeonixLink FirstChild;

	AeonixLink myNeighbours[6];

	AeonixNode() :
		Code(0),
		Parent(AeonixLink::GetInvalidLink()),
		FirstChild(AeonixLink::GetInvalidLink()) {}

	bool HasChildren() const { return FirstChild.IsValid(); }

};

FORCEINLINE FArchive &operator <<(FArchive &Ar, AeonixNode& aAeonixNode)
{
	// Cast mortoncode_t (uint_fast64_t) to uint64 for serialization
	uint64 CodeValue = aAeonixNode.Code;
	Ar << CodeValue;
	
	if (Ar.IsLoading())
	{
		aAeonixNode.Code = CodeValue;
	}
	
	Ar << aAeonixNode.Parent;
	Ar << aAeonixNode.FirstChild;

	for (int i = 0; i < 6; i++)
	{
		Ar << aAeonixNode.myNeighbours[i];
	}

	return Ar;
}