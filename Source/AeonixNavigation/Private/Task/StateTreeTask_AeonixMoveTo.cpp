#include "Task/StateTreeTask_AeonixMoveTo.h"
#include "Task/AITask_AeonixMoveTo.h"
#include "AIController.h"
#include "GameFramework/Actor.h"
#include "NavigationSystem.h"
#include "Subsystem/AeonixSubsystem.h"
#include "Component/AeonixNavAgentComponent.h"
#include "DrawDebugHelpers.h"
#include <StateTreeModule/Public/StateTreeExecutionContext.h>

EStateTreeRunStatus FStateTreeTask_AeonixMoveTo::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    auto& Data = Context.GetInstanceData(*this);
    if (!Data.AgentComponent.IsValid())
    {
        AActor* OwnerActor = Cast<AActor>(Context.GetOwner());
        if (OwnerActor)
        {
            Data.AgentComponent = OwnerActor->FindComponentByClass<UAeonixNavAgentComponent>();
        }
    }
    if (!Data.AgentComponent.IsValid())
    {
        return EStateTreeRunStatus::Failed;
    }

    FVector Target = Data.GoalLocation;
    if (Data.GoalActor.IsValid())
    {
        Target = Data.GoalActor->GetActorLocation();
    }

    AAIController* Controller = Cast<AAIController>(Data.AgentComponent->GetOwner());
    if (!Controller)
    {
        return EStateTreeRunStatus::Failed;
    }

    Data.MoveTask = UAITask_AeonixMoveTo::AeonixAIMoveTo(Controller, Target, false, Data.GoalActor.Get(), Data.AcceptableRadius, EAIOptionFlag::Default, false, false);
    if (!Data.MoveTask)
    {
        return EStateTreeRunStatus::Failed;
    }
    Data.MoveTask->ReadyForActivation();
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_AeonixMoveTo::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    auto& Data = Context.GetInstanceData(*this);
    if (!Data.MoveTask)
    {
        return EStateTreeRunStatus::Failed;
    }
    if (Data.MoveTask->WasMoveSuccessful())
    {
        return EStateTreeRunStatus::Succeeded;
    }
    if (Data.MoveTask->IsFinished())
    {
        return EStateTreeRunStatus::Failed;
    }
    return EStateTreeRunStatus::Running;
}

void FStateTreeTask_AeonixMoveTo::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    auto& Data = Context.GetInstanceData(*this);
    if (Data.MoveTask && !Data.MoveTask->IsFinished())
    {
        Data.MoveTask->EndTask();
    }
    Data.MoveTask = nullptr;
}
