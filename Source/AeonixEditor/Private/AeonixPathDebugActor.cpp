
#include <AeonixEditor/Private/AeonixPathDebugActor.h>

#include <AeonixEditor/Private/AenoixEditorDebugSubsystem.h>
#include <AeonixNavigation/Public/Component/AeonixNavAgentComponent.h>
#include <AeonixNavigation/Public/Subsystem/AeonixSubsystem.h>

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

	GetWorld()->GetSubsystem<UAeonixSubsystem>()->RegisterNavComponent(NavAgentComponent, EAeonixMassEntityFlag::Enabled);

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