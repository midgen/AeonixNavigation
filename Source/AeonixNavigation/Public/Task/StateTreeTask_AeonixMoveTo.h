#pragma once

#include "StateTreeTaskBase.h"
#include "StateTreeTask_AeonixMoveTo.generated.h"

class UAITask_AeonixMoveTo;
class AAeonixBoundingVolume;
class UAeonixNavAgentComponent;

USTRUCT()
struct FStateTreeTask_AeonixMoveToInstanceData
{
    GENERATED_BODY()

    // Blackboard/StateTree input: goal location
    UPROPERTY(EditAnywhere, Category = Input)
    FVector GoalLocation;

    // Optional: goal actor
    UPROPERTY(EditAnywhere, Category = Input)
    TWeakObjectPtr<AActor> GoalActor;

    // Optional: acceptance radius
    UPROPERTY(EditAnywhere, Category = Input)
    float AcceptableRadius = 50.f;

    // Optional: agent component
    UPROPERTY(EditAnywhere, Category = Input)
    TWeakObjectPtr<UAeonixNavAgentComponent> AgentComponent;

    // Internal task state
    UPROPERTY(Transient)
    UAITask_AeonixMoveTo* MoveTask = nullptr;
};

USTRUCT()
struct AEONIXNAVIGATION_API FStateTreeTask_AeonixMoveTo : public FStateTreeTaskBase
{
    GENERATED_BODY();

    typedef FStateTreeTask_AeonixMoveToInstanceData FInstanceDataType;

    EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const;
    EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const;
    void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const;
};
