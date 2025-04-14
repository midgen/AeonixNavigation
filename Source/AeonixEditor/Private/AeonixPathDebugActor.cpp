
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

	GetWorld()->GetSubsystem<UAeonixSubsystem>()->RegisterNavComponent(NavAgentComponent, EAeonixMassEntityFlag::YES);

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
	
	GEditor->GetEditorSubsystem<UAenoixEditorDebugSubsystem>()->UpdateDebugActor(this);
}