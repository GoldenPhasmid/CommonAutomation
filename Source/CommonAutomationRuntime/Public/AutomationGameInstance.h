#pragma once

#include "CoreMinimal.h"
#include "GameInstanceAutomationSupport.h"
#include "Engine/GameInstance.h"

#include "AutomationGameInstance.generated.h"

struct FAutomationWorldInitParams;
class AGameModeBase;

/**
 *
 * Games can either subclass UAutomationGameInstance or implement IGameInstanceAutomationSupport interface to
 * use automation world functionality with game instance
 */
UCLASS()
class COMMONAUTOMATIONRUNTIME_API UAutomationGameInstance: public UGameInstance, public IGameInstanceAutomationSupport
{
	GENERATED_BODY()
public:

	virtual void InitForAutomation(FWorldContext* InWorldContext) override;
};
