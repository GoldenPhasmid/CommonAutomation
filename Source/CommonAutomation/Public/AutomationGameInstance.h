#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"

#include "AutomationGameInstance.generated.h"

struct FAutomationWorldInitParams;
class AGameModeBase;

UCLASS()
class COMMONAUTOMATION_API UAutomationGameInstance: public UGameInstance
{
	GENERATED_BODY()
public:

	/* Called to initialize the game instance with a minimal world suitable for automation */
	void InitializeForAutomation(const FAutomationWorldInitParams& InitParams, const FWorldInitializationValues& InitValues);

	virtual AGameModeBase* CreateGameModeForURL(FURL InURL, UWorld* InWorld) override;

	TSubclassOf<AGameModeBase> DefaultGameModeClass = nullptr;
};
