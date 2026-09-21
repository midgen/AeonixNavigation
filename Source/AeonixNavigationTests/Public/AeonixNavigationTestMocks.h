#pragma once

#include "Data/AeonixData.h"
#include "Data/AeonixLink.h"
#include "Data/AeonixNode.h"
#include "Data/AeonixLeafNode.h"
#include "Data/AeonixOctreeData.h"
#include "Interface/AeonixCollisionQueryInterface.h"
#include "Interface/AeonixDebugDrawInterface.h"
#include "Engine/EngineTypes.h"
#include "Math/Vector.h"
#include "Containers/Array.h"

// Mock debug draw interface for logging and tracking voxel visualization
class FTestDebugDrawInterface : public IAeonixDebugDrawInterface
{
public:
    mutable int32 BlockedVoxelCount = 0;
    mutable int32 TotalVoxelCount = 0;
    mutable TArray<FVector> BlockedPositions;

    virtual void AeonixDrawDebugString(const FVector& Position, const FString& String, const FColor& Color) const override
    {
        // Log morton codes if needed for debugging
    }

    virtual void AeonixDrawDebugBox(const FVector& Position, const float Size, const FColor& Color) const override
    {
        TotalVoxelCount++;
        if (Color == FColor::Red)
        {
            BlockedVoxelCount++;
            BlockedPositions.Add(Position);
            UE_LOG(LogTemp, VeryVerbose, TEXT("Drawing blocked voxel at: %s"), *Position.ToString());
        }
    }

    virtual void AeonixDrawDebugLine(const FVector& Start, const FVector& End, const FColor& Color, float Thickness = 0.0f) const override
    {
        // Could log neighbor connections here
    }

    virtual void AeonixDrawDebugDirectionalArrow(const FVector& Start, const FVector& End, const FColor& Color, float ArrowSize = 0.0f) const override
    {
        // Could log directional connections
    }
};

// Mock implementation that simulates a wall splitting the navigable volume
class FTestWallCollisionQueryInterface : public IAeonixCollisionQueryInterface
{
public:
    // Define wall parameters - wall is at Y=0 plane by default
    float WallYPosition = 0.0f;
    float WallThickness = 50.0f; // Thick enough to ensure it blocks properly
    float WallXMin = -1000.0f;
    float WallXMax = 1000.0f;
    float WallZMin = -1000.0f;
    float WallZMax = 1000.0f;

    virtual bool IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
    {
        // Check if this voxel overlaps with our wall
        float voxelMinY = Position.Y - VoxelSize;
        float voxelMaxY = Position.Y + VoxelSize;

        float wallMinY = WallYPosition - (WallThickness * 0.5f);
        float wallMaxY = WallYPosition + (WallThickness * 0.5f);

        // If voxel overlaps with wall in Y axis
        if (voxelMaxY >= wallMinY && voxelMinY <= wallMaxY)
        {
            // Check if it's within wall bounds in X and Z
            if (Position.X >= WallXMin && Position.X <= WallXMax &&
                Position.Z >= WallZMin && Position.Z <= WallZMax)
            {
                UE_LOG(LogTemp, VeryVerbose, TEXT("Blocking voxel at position: %s (size: %f)"),
                    *Position.ToString(), VoxelSize);
                return true; // This voxel is blocked by the wall
            }
        }

        return false; // Not blocked
    }

    virtual bool IsLeafBlocked(const FVector& Position, const float LeafSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
    {
        // For test purposes, use same logic as IsBlocked but with leaf size
        return IsBlocked(Position, LeafSize, CollisionChannel, AgentRadius);
    }
};

// Mock implementation that simulates obstacles that don't completely block the path
class FTestPartialObstacleCollisionQueryInterface : public IAeonixCollisionQueryInterface
{
public:
    // Define two obstacles that create a maze-like environment
    // First obstacle: vertical wall with gap
    float Obstacle1_X = 0.0f;
    float Obstacle1_YMin = -300.0f;
    float Obstacle1_YMax = -50.0f;  // Gap from -50 to 50
    float Obstacle1_ZMin = -300.0f;
    float Obstacle1_ZMax = 300.0f;
    float Obstacle1_Thickness = 50.0f;

    // Second obstacle: vertical wall with gap offset from first
    float Obstacle2_X = 0.0f;
    float Obstacle2_YMin = 50.0f;   // Gap from -50 to 50
    float Obstacle2_YMax = 300.0f;
    float Obstacle2_ZMin = -300.0f;
    float Obstacle2_ZMax = 300.0f;
    float Obstacle2_Thickness = 50.0f;

    virtual bool IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
    {
        // Check first obstacle
        float voxelMinX = Position.X - VoxelSize;
        float voxelMaxX = Position.X + VoxelSize;
        float voxelMinY = Position.Y - VoxelSize;
        float voxelMaxY = Position.Y + VoxelSize;

        // First obstacle check
        float obs1MinX = Obstacle1_X - (Obstacle1_Thickness * 0.5f);
        float obs1MaxX = Obstacle1_X + (Obstacle1_Thickness * 0.5f);

        if (voxelMaxX >= obs1MinX && voxelMinX <= obs1MaxX)
        {
            // Check if Y is in the solid part of obstacle 1
            if ((voxelMaxY >= Obstacle1_YMin && voxelMinY <= Obstacle1_YMax) &&
                Position.Z >= Obstacle1_ZMin && Position.Z <= Obstacle1_ZMax)
            {
                UE_LOG(LogTemp, VeryVerbose, TEXT("Blocking voxel at position: %s (Obstacle 1)"),
                    *Position.ToString());
                return true;
            }

            // Check if Y is in the solid part of obstacle 2
            if ((voxelMaxY >= Obstacle2_YMin && voxelMinY <= Obstacle2_YMax) &&
                Position.Z >= Obstacle2_ZMin && Position.Z <= Obstacle2_ZMax)
            {
                UE_LOG(LogTemp, VeryVerbose, TEXT("Blocking voxel at position: %s (Obstacle 2)"),
                    *Position.ToString());
                return true;
            }
        }

        return false; // Not blocked
    }

    virtual bool IsLeafBlocked(const FVector& Position, const float LeafSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
    {
        // For test purposes, use same logic as IsBlocked but with leaf size
        return IsBlocked(Position, LeafSize, CollisionChannel, AgentRadius);
    }
};

// Utility class for common test operations
class FAeonixNavigationTestUtils
{
public:
    // Find a navigation link at or near the specified position
    static bool FindLinkAtPosition(struct FAeonixData& NavData, const FVector& Position,
                                  struct AeonixLink& OutLink, FString& OutLogMessage)
    {
        for (int layer = 0; layer < NavData.OctreeData.NumLayers; ++layer)
        {
            const TArray<struct AeonixNode>& nodes = NavData.OctreeData.GetLayer(layer);
            for (int32 i = 0; i < nodes.Num(); ++i)
            {
                FVector nodePos;
                if (NavData.GetLinkPosition(AeonixLink(layer, i, 0), nodePos))
                {
                    float voxelSize = NavData.GetVoxelSize(layer);

                    if (FVector::Dist(nodePos, Position) < voxelSize)
                    {
                        if (!nodes[i].HasChildren() || layer == 0)
                        {
                            OutLink = AeonixLink(layer, i, 0);
                            OutLogMessage = FString::Printf(TEXT("Found link at layer %d, index %d, position %s"),
                                layer, i, *nodePos.ToString());
                            return true;
                        }
                    }
                }
            }
        }

        OutLogMessage = FString::Printf(TEXT("Could not find navigation link near position %s"), *Position.ToString());
        return false;
    }
};

// Silent debug draw interface that discards all draw calls
class FSilentDebugDrawInterface : public IAeonixDebugDrawInterface
{
public:
	virtual void AeonixDrawDebugString(const FVector& Position, const FString& String, const FColor& Color) const override {}
	virtual void AeonixDrawDebugBox(const FVector& Position, const float Size, const FColor& Color) const override {}
	virtual void AeonixDrawDebugLine(const FVector& Start, const FVector& End, const FColor& Color, float Thickness = 0.0f) const override {}
	virtual void AeonixDrawDebugDirectionalArrow(const FVector& Start, const FVector& End, const FColor& Color, float ArrowSize = 0.0f) const override {}
};

// Helper function to get a link from a position (simplified from AeonixMediator::GetLinkFromPosition)
inline bool GetLinkFromPosition(const FVector& Position, const FAeonixData& NavData, AeonixLink& OutLink)
{
	const FAeonixGenerationParameters& Params = NavData.GetParams();
	const FVector& Origin = Params.Origin;
	const FVector& Extent = Params.Extents;

	// Check if position is within bounds
	const FBox Bounds(Origin - Extent, Origin + Extent);
	if (!Bounds.IsInside(Position))
	{
		return false;
	}

	// Z-order origin (where code == 0)
	const FVector ZOrigin = Origin - Extent;
	const FVector LocalPos = Position - ZOrigin;

	// Start from top layer and descend
	int32 LayerIndex = NavData.OctreeData.GetNumLayers() - 1;
	nodeindex_t NodeIndex = 0;

	while (LayerIndex >= 0 && LayerIndex < NavData.OctreeData.GetNumLayers())
	{
		const TArray<AeonixNode>& Layer = NavData.OctreeData.GetLayer(LayerIndex);
		const float VoxelSize = NavData.GetVoxelSize(LayerIndex);

		// Calculate XYZ coordinates
		const int32 X = FMath::FloorToInt(LocalPos.X / VoxelSize);
		const int32 Y = FMath::FloorToInt(LocalPos.Y / VoxelSize);
		const int32 Z = FMath::FloorToInt(LocalPos.Z / VoxelSize);
		const mortoncode_t Code = morton3D_64_encode(X, Y, Z);

		// Find node with this code
		for (nodeindex_t j = NodeIndex; j < Layer.Num(); j++)
		{
			const AeonixNode& Node = Layer[j];
			if (Node.Code == Code)
			{
				// No children - this is the link
				if (!Node.FirstChild.IsValid())
				{
					OutLink.LayerIndex = LayerIndex;
					OutLink.NodeIndex = j;
					OutLink.SubnodeIndex = 0;
					return true;
				}

				// Leaf node - find subnode
				if (LayerIndex == 0)
				{
					const AeonixLeafNode& Leaf = NavData.OctreeData.GetLeafNode(Node.FirstChild.NodeIndex);

					// Get node world position
					FVector NodePosition;
					NavData.GetNodePosition(LayerIndex, Node.Code, NodePosition);
					const FVector NodeOrigin = NodePosition - FVector(VoxelSize * 0.5f);
					const FVector NodeLocalPos = Position - NodeOrigin;

					// Calculate leaf voxel coordinates
					const int32 LeafX = FMath::FloorToInt(NodeLocalPos.X / (VoxelSize * 0.25f));
					const int32 LeafY = FMath::FloorToInt(NodeLocalPos.Y / (VoxelSize * 0.25f));
					const int32 LeafZ = FMath::FloorToInt(NodeLocalPos.Z / (VoxelSize * 0.25f));
					const mortoncode_t LeafIndex = morton3D_64_encode(LeafX, LeafY, LeafZ);

					// Check if blocked
					if (Leaf.GetNode(LeafIndex))
					{
						return false; // Blocked
					}

					OutLink.LayerIndex = 0;
					OutLink.NodeIndex = j;
					OutLink.SubnodeIndex = LeafIndex;
					return true;
				}

				// Has children - descend
				LayerIndex = Layer[j].FirstChild.GetLayerIndex();
				NodeIndex = Layer[j].FirstChild.GetNodeIndex();
				break;
			}
		}
	}

	return false;
}
