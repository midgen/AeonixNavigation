#pragma once

#include "Modules/ModuleManager.h"

class FAeonixGameUtilsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
