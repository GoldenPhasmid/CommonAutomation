#include "CommonAutomationModule.h"

void FCommonAutomationModule::RequestGC()
{
	FCommonAutomationModule::Get().bForceGarbageCollectionAfterTestRun = true;
}

void FCommonAutomationModule::HandleTestRunEnded()
{
	if (bForceGarbageCollectionAfterTestRun)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CommonAutomation_CollectGarbage);
		
		constexpr bool bFullPurge = false;
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, bFullPurge);
		bForceGarbageCollectionAfterTestRun = false;
	}
}

void FCommonAutomationModule::StartupModule()
{
	FAutomationTestFramework::Get().OnAfterAllTestsEvent.AddRaw(this, &FCommonAutomationModule::HandleTestRunEnded);
}

void FCommonAutomationModule::ShutdownModule()
{
	FAutomationTestFramework::Get().OnAfterAllTestsEvent.RemoveAll(this);
}

IMPLEMENT_MODULE(FCommonAutomationModule, CommonAutomation)
