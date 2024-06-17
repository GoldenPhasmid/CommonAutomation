#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "CommonAutomationSettings.generated.h"

class UWorldSubsystem;
class ULocalPlayerSubsystem;
class UGameInstanceSubsystem;

struct FSubsystemContainer
{
	FSubsystemContainer() = default;
	FSubsystemContainer(UClass* InBaseType);

	FORCEINLINE bool Initialized() const { return BaseType != nullptr; }

	FORCEINLINE void MarkDirty() const
	{
		DisabledSubsystems.Reset();
		bDirty = true;
	}

	FORCEINLINE bool IsDirty() const
	{
		return bDirty;
	}

	template <typename TSubsystemType>
	const TArray<UClass*>& GetDisabledSubsystems(const TArray<TSubclassOf<TSubsystemType>>& EnabledSubsystems) const
	{
		if (bDirty)
		{
			return DisabledSubsystems;
		}
		
		bDirty = false;
		DisabledSubsystems = AllSubsystems;
		
		if (EnabledSubsystems.IsEmpty())
		{
			return DisabledSubsystems;
		}
		
		TArray<UClass*> Copy{EnabledSubsystems};
		Algo::Sort(Copy);

		int32 Index = 0;
		for (auto It = DisabledSubsystems.CreateIterator(); It; ++It)
		{
			if (*It == EnabledSubsystems[Index])
			{
				It.RemoveCurrentSwap();
				++Index;
			}
		}

		return DisabledSubsystems;
	}

	TArray<UClass*> AllSubsystems;
	TArray<UClass*> ProjectModuleSubsystems;

private:
	UClass* BaseType = nullptr;
	mutable TArray<UClass*> DisabledSubsystems;
	mutable bool bDirty = true;
};

UCLASS(Config = Editor, DefaultConfig)
class COMMONAUTOMATION_API UCommonAutomationSettings: public UDeveloperSettings
{
	GENERATED_BODY()
public:

	static const UCommonAutomationSettings* Get();

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	static const TArray<FName>& GetProjectModules();
    static bool IsProjectModuleClass(UClass* Class);

	template <typename TSubsystemType>
	const TArray<UClass*>& GetDisabledSubsystems() const;
	
protected:
	
	UPROPERTY(EditAnywhere, Config)
	bool bDisableProjectSubsystems = true;
	
	/**
	 * A list of world subsystems will be always present when running automation world
	 * We store enabled subsystems so that new subsystems automatically disabled
	 */
	UPROPERTY(EditAnywhere, Config, meta = (Validate, NoElementDuplicate), DisplayName = "Enabled World Subsystems")
	TArray<TSubclassOf<UWorldSubsystem>> WorldSubsystems;

	/**
	 * A list of game instance subsystems will be always present when running automation world
	 * We store enabled subsystems so that new subsystems automatically disabled
	 */
	UPROPERTY(EditAnywhere, Config, meta = (Validate, NoElementDuplicate), DisplayName = "Enabled Game Instance Subsystems")
	TArray<TSubclassOf<UGameInstanceSubsystem>> GameInstanceSubsystems;

	/**
	 * A list of local player subsystems will be always present when running automation world
	 * We store enabled subsystems so that new subsystems automatically disabled
	 */
	UPROPERTY(EditAnywhere, Config, meta = (Validate, NoElementDuplicate), DisplayName = "Enabled Local Player Subsystems")
	TArray<TSubclassOf<ULocalPlayerSubsystem>> LocalPlayerSubsystems;
	
	template <typename TSubsystemType>
	void InitializeToDefault(FSubsystemContainer& Container, TArray<TSubclassOf<TSubsystemType>>& EnabledArray, const FString& ConfigKey) const
	{
		static const TCHAR* ConfigSection{TEXT("/Script/CommonAutomation.CommonAutomationSettings")};

		if (!Container.Initialized())
		{
			Container = FSubsystemContainer{TSubsystemType::StaticClass()};
		}

		FString ConfigArray{};
		const bool bResult = GConfig->GetString(ConfigSection, *ConfigKey, ConfigArray, GEditorIni);

		if (bResult == false || ConfigArray.IsEmpty())
		{
			EnabledArray.Reset();
			EnabledArray = TArray<TSubclassOf<TSubsystemType>>{Container.AllSubsystems};
			if (bDisableProjectSubsystems)
			{
				for (auto It = EnabledArray.CreateIterator(); It; ++It)
				{
					if (Container.ProjectModuleSubsystems.Contains(*It))
					{
						It.RemoveCurrentSwap();
					}
				}
			}
		}
	}

	template <typename TSubsystemType>
	FString GetConfigKey() const = delete;
	
	FSubsystemContainer WorldSubsystemContainer;
	FSubsystemContainer GameInstanceSubsystemContainer;
	FSubsystemContainer LocalPlayerSubsystemContainer;
};
