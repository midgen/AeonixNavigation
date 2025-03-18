#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UnrealEd.h"


DECLARE_LOG_CATEGORY_EXTERN(AeonixEditor, All, All)

class FAeonixEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

};