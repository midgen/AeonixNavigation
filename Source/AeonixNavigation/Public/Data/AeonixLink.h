#pragma once

struct AEONIXNAVIGATION_API AeonixLink
{
	unsigned int LayerIndex:4;
	unsigned int NodeIndex:22;
	unsigned int SubnodeIndex:6;

	AeonixLink() : 
		LayerIndex(15),
		NodeIndex(0),
		SubnodeIndex(0) {}

	AeonixLink(uint8 aLayer, uint_fast32_t aNodeIndex, uint8 aSubNodeIndex)
		: LayerIndex(aLayer),
		NodeIndex(aNodeIndex),
		SubnodeIndex(aSubNodeIndex) {}

	uint8 GetLayerIndex() const { return LayerIndex; }
	void SetLayerIndex(const uint8 aLayerIndex) { LayerIndex = aLayerIndex; }

	uint_fast32_t GetNodeIndex() const { return NodeIndex; }
	void SetNodeIndex(const uint_fast32_t aNodeIndex) { NodeIndex = aNodeIndex; }

	uint8 GetSubnodeIndex() const { return SubnodeIndex; }
	void SetSubnodeIndex(const uint8 aSubnodeIndex) { SubnodeIndex = aSubnodeIndex; }

	bool IsValid() const { return LayerIndex != 15; }
	void SetInvalid() { LayerIndex = 15; }

	bool operator==(const AeonixLink& aOther) const {
		return memcmp(this, &aOther, sizeof(AeonixLink)) == 0;
	}

	static AeonixLink GetInvalidLink() { return AeonixLink(15, 0, 0); }

	FString ToString() 
	{
		return FString::Printf(TEXT("%i:%i:%i"), LayerIndex, NodeIndex, SubnodeIndex);
	};

};

FORCEINLINE uint32 GetTypeHash(const AeonixLink& b)
{
	return *(uint32*)&b;
}


FORCEINLINE FArchive &operator <<(FArchive &Ar, AeonixLink& aAeonixLink)
{
	Ar.Serialize(&aAeonixLink, sizeof(AeonixLink));
	return Ar;
}