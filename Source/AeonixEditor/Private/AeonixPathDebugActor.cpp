
#include "Subsystem/AeonixSubsystem.h"

#include <AeonixEditor/Private/AeonixPathDebugActor.h>
#include <AeonixEditor/Private/AenoixEditorDebugSubsystem.h>

#include <AeonixNavigation/Public/Component/AeonixNavAgentComponent.h>

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
}

void AAeonixPathDebugActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	// Update debug shenanigans
	GEditor->GetEditorSubsystem<UAenoixEditorDebugSubsystem>()->UpdateDebugActor(this);
}

void AAeonixPathDebugActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Update debug shenanigans
	GEditor->GetEditorSubsystem<UAenoixEditorDebugSubsystem>()->UpdateDebugActor(this);
}