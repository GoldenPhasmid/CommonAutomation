#pragma once

#include "CoreMinimal.h"

#include "GameInstanceAutomationSupport.generated.h"

UINTERFACE()
class COMMONAUTOMATIONRUNTIME_API UGameInstanceAutomationSupport: public UInterface
{
	GENERATED_BODY()
public:
	
};

class COMMONAUTOMATIONRUNTIME_API IGameInstanceAutomationSupport
{
	GENERATED_BODY()
public:
	
	// notify game instance that it is being initialized for automation
	// should call Init() the same way InitializeStandalone or InitForPlayInEditor does
	// @see UAutomationGameInstance
	virtual void InitForAutomation(FWorldContext* WorldContext) = 0;
};
