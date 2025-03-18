#pragma once

#include <Runtime/Core/Public/Modules/ModuleManager.h>

#if WITH_EDITOR
DECLARE_LOG_CATEGORY_EXTERN(AeonixNavigation, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(VAeonixNavigation, Log, All);
#endif

class FAeonixNavigationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};