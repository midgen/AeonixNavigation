
#include "AeonixPathDebugActor.h"

#include "AenoixEditorDebugSubsystem.h"
#include "Component/AeonixNavAgentComponent.h"
#include "Subsystem/AeonixSubsystem.h"

static const FName NavAgentComponentName(TEXT("AeonixNavAgentComponent"));
static const FName RootComponentName(TEXT("RootComponent"));

AAeonixPathDebugActor::AAeonixPathDebugActor(const FObjectInitializer& Initializer)
	: Super(Initializer)
	, DebugType(EAeonixPathDebugActorType::START)
	, NavAgentComponent(CreateDefaultSubobject<UAeonixNavAgentComponent>(NavAgentComponentName))	 
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(RootComponentName));
}

void AAeonixPathDebugActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	GetWorld()->GetSubsystem<UAeonixSubsystem>()->RegisterNavComponent(NavAgentComponent);

	GEditor->GetEditorSubsystem<UAenoixEditorDebugSubsystem>()->UpdateDebugActor(this);
}

void AAeonixPathDebugActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	
	GEditor->GetEditorSubsystem<UAenoixEditorDebugSubsystem>()->UpdateDebugActor(this);
}

void AAeonixPathDebugActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Only trigger path recalculation for properties that affect pathfinding
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AAeonixPathDebugActor, DebugType))
	{
		GEditor->GetEditorSubsystem<UAenoixEditorDebugSubsystem>()->UpdateDebugActor(this);
	}
}

void AAeonixPathDebugActor::BeginDestroy()
{
	// Notify debug subsystem to clear references to this actor
	if (GEditor && GEditor->GetEditorSubsystem<UAenoixEditorDebugSubsystem>())
	{
		GEditor->GetEditorSubsystem<UAenoixEditorDebugSubsystem>()->ClearDebugActor(this);
	}

	// Unregister nav component from subsystem
	if (NavAgentComponent && GetWorld() && GetWorld()->GetSubsystem<UAeonixSubsystem>())
	{
		GetWorld()->GetSubsystem<UAeonixSubsystem>()->UnRegisterNavComponent(NavAgentComponent);
	}

	Super::BeginDestroy();
}

void AAeonixPathDebugActor::Destroyed()
{
	// Additional safety cleanup
	if (GEditor && GEditor->GetEditorSubsystem<UAenoixEditorDebugSubsystem>())
	{
		GEditor->GetEditorSubsystem<UAenoixEditorDebugSubsystem>()->ClearDebugActor(this);
	}

	Super::Destroyed();
}