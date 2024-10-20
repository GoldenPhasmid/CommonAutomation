#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FCommonAutomationModule: public IModuleInterface
{
public:
    static FCommonAutomationModule& Get()
    {
        return FModuleManager::GetModuleChecked<FCommonAutomationModule>("CommonAutomation");
    }
    
    static void RequestGC();

protected:
    void HandleTestRunEnded();

    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    bool bForceGarbageCollectionAfterTestRun = false;
};