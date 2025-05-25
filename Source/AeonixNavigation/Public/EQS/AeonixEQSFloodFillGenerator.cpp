#include "EQS/AeonixEQSFloodFillGenerator.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "NavigationSystem.h"
#include "Data/AeonixData.h"
#include "Subsystem/AeonixSubsystem.h"
#include "Actor/AeonixBoundingVolume.h"
#include "Util/AeonixMediator.h"
#include "Data/AeonixLink.h"

UAeonixEQSFloodFillGenerator::UAeonixEQSFloodFillGenerator(const FObjectInitializer& ObjectInitializer)
{
	Context = UEnvQueryContext_Querier::StaticClass();
    ItemType = UEnvQueryItemType_Point::StaticClass();
    FloodRadius.DefaultValue = 1000.0f;
    NavAgentIndex.DefaultValue = 0;
}

void UAeonixEQSFloodFillGenerator::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
    UObject* Owner = QueryInstance.Owner.Get();
    if (!Owner) return;

    UWorld* World = GEngine->GetWorldFromContextObject(Owner, EGetWorldErrorMode::ReturnNull);
    if (!World) return;

    TArray<FVector> ContextLocations;
    QueryInstance.PrepareContext(Context, ContextLocations);
    if (ContextLocations.Num() == 0) return;
    FVector Origin = ContextLocations[0];

    UAeonixSubsystem* AeonixSubsystem = World->GetSubsystem<UAeonixSubsystem>();
    if (!AeonixSubsystem) return;

    float Radius = FloodRadius.GetValue();
    int32 AgentIdx = NavAgentIndex.GetValue();

    // Get nav volume and data
    const AAeonixBoundingVolume* NavVolume = AeonixSubsystem->GetVolumeForPosition(Origin);
    if (!NavVolume || !NavVolume->HasData()) return;
    const FAeonixData& NavData = NavVolume->GetNavData();

    // Convert start position to AeonixLink
    AeonixLink StartLink;
    if (!AeonixMediator::GetLinkFromPosition(Origin, *NavVolume, StartLink))
        return;

    TSet<AeonixLink> Visited;
    TQueue<AeonixLink> Queue;
    Queue.Enqueue(StartLink);
    Visited.Add(StartLink);

    FVector StartPos;
    NavData.GetLinkPosition(StartLink, StartPos);

    while (!Queue.IsEmpty())
    {
        AeonixLink CurrentLink;
        Queue.Dequeue(CurrentLink);
        FVector CurrentPos;
        if (!NavData.GetLinkPosition(CurrentLink, CurrentPos))
            continue;
        if (FVector::DistSquared(CurrentPos, Origin) > Radius * Radius)
            continue;
        QueryInstance.AddItemData<UEnvQueryItemType_Point>(CurrentPos);

        // Get neighbors
        TArray<AeonixLink> Neighbors;
        const AeonixNode& CurrentNode = NavData.OctreeData.GetNode(CurrentLink);
        if (CurrentLink.GetLayerIndex() == 0 && CurrentNode.FirstChild.IsValid())
        {
            NavData.OctreeData.GetLeafNeighbours(CurrentLink, Neighbors);
        }
        else
        {
            NavData.OctreeData.GetNeighbours(CurrentLink, Neighbors);
        }
        for (const AeonixLink& Neighbor : Neighbors)
        {
            if (!Neighbor.IsValid() || Visited.Contains(Neighbor))
                continue;
            FVector NeighborPos;
            if (!NavData.GetLinkPosition(Neighbor, NeighborPos))
                continue;
            if (FVector::DistSquared(NeighborPos, Origin) > Radius * Radius)
                continue;
            Visited.Add(Neighbor);
            Queue.Enqueue(Neighbor);
        }
    }
}
