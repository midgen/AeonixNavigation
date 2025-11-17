#pragma once

#include "Modules/ModuleManager.h"

AEONIXNAVIGATION_API DECLARE_LOG_CATEGORY_EXTERN(LogAeonixNavigation, Log, All);
AEONIXNAVIGATION_API DECLARE_LOG_CATEGORY_EXTERN(VLogAeonixNavigation, Log, All);
AEONIXNAVIGATION_API DECLARE_LOG_CATEGORY_EXTERN(LogAeonixRegen, Warning, All);

class FAeonixNavigationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};