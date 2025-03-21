




#include <AeonixEditor/Private/AeonixPathDebugActor.h>
#include <AeonixNavigation/Public/Subsystem/AeonixSubsystem.h>
#include <AeonixNavigation/Public/Component/AeonixNavigationComponent.h>

#include <AeonixEditor/Private/AenoixEditorDebugSubsystem.h>

static const FName NavigationComponentName(TEXT("AeonixNavigationComponent"));
static const FName RootComponentName(TEXT("RootComponent"));

AAeonixPathDebugActor::AAeonixPathDebugActor(const FObjectInitializer& Initializer)
	: Super(Initializer)
	, DebugType(EAeonixPathDebugActorType::START)
	, NavigationComponent(CreateDefaultSubobject<UAeonixNavigationComponent>(NavigationComponentName))	 
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(RootComponentName));
}

void AAeonixPathDebugActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	//UAeonixSubsystem* AeonixSubsystem = GEngine->GetEngineSubsystem<UAeonixSubsystem>();

	//NavigationComponent->CurrentNavVolume = AeonixSubsystem->GetVolumeForPosition(GetActorLocation());
	
	GEditor->GetEditorSubsystem<UAenoixEditorDebugSubsystem>()->UpdateDebugActor(this);
}

void AAeonixPathDebugActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	GEditor->GetEditorSubsystem<UAenoixEditorDebugSubsystem>()->UpdateDebugActor(this);
}