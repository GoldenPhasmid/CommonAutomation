#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "CommonAutomationSettings.generated.h"

UCLASS(Config = Editor, DefaultConfig)
class COMMONAUTOMATION_API UCommonAutomationSettings: public UDeveloperSettings
{
	GENERATED_BODY()
public:

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
	TArray<UClass*> GetSubsystemArray(UClass* BaseType);

	UPROPERTY(EditAnywhere, Config)
	bool bDisableProjectSubsystemsByDefault = true;
	
	/** A list of world subsystems will be always present when running automation world */
	UPROPERTY(EditAnywhere, Config, meta = (Validate, NoElementDuplicate))
	TArray<TSubclassOf<UWorldSubsystem>> PersistentWorldSubsystems;

	/** A list of game instance subsystems will be always present when running automation world */
	UPROPERTY(EditAnywhere, Config, meta = (Validate, NoElementDuplicate))
	TArray<TSubclassOf<UGameInstanceSubsystem>> PersistentGameInstanceSubsystems;
};
