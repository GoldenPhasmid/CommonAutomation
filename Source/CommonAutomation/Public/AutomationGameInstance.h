#pragma once

#include "CoreMinimal.h"
#include "GameInstanceAutomationSupport.h"
#include "Engine/GameInstance.h"

#include "AutomationGameInstance.generated.h"

struct FAutomationWorldInitParams;
class AGameModeBase;

UCLASS()
class COMMONAUTOMATION_API UAutomationGameInstance: public UGameInstance, public IGameInstanceAutomationSupport
{
	GENERATED_BODY()
public:

	virtual void InitForAutomation(FWorldContext* InWorldContext) override;
};
