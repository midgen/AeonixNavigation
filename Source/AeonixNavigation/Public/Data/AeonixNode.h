#pragma once

#include <AeonixNavigation/Public/Data/AeonixLink.h>
#include <AeonixNavigation/Public/Data/AeonixDefines.h>

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
	Ar << aAeonixNode.Code;
	Ar << aAeonixNode.Parent;
	Ar << aAeonixNode.FirstChild;

	for (int i = 0; i < 6; i++)
	{
		Ar << aAeonixNode.myNeighbours[i];
	}

	return Ar;
}