#include "AutomationGameInstance.h"

void UAutomationGameInstance::InitForAutomation(FWorldContext* InWorldContext)
{
	check(InWorldContext);
	WorldContext = InWorldContext;
	WorldContext->OwningGameInstance = this;
	
	Init();
}