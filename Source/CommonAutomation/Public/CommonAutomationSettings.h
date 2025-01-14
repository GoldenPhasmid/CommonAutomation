#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "CommonAutomationSettings.generated.h"

class UWorldSubsystem;
class ULocalPlayerSubsystem;
class UGameInstanceSubsystem;

namespace UE::Automation
{
	
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
		if (bDirty == false)
		{
			return DisabledSubsystems;
		}
	
		bDirty = false;
		DisabledSubsystems = AllSubsystems;
	
		if (EnabledSubsystems.IsEmpty())
		{
			return DisabledSubsystems;
		}

		// @todo: fix O(N^2) complexity
		for (auto It = DisabledSubsystems.CreateIterator(); It; ++It)
		{
			if (EnabledSubsystems.Contains(*It))
			{
				It.RemoveCurrentSwap();
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
	
}


UCLASS(Config = Editor, DefaultConfig)
class COMMONAUTOMATION_API UCommonAutomationSettings: public UDeveloperSettings
{
	GENERATED_BODY()
public:

	UCommonAutomationSettings(const FObjectInitializer& Initializer);

	/** @return automation settings */
	static const UCommonAutomationSettings* Get();
	static UCommonAutomationSettings* GetMutable();

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	static const TArray<FName>& GetProjectModules();
    static bool IsProjectModuleClass(UClass* Class);

	/** @return a list of disabled subsystems for each subsystem group: UWorldSubsystem, UGameInstanceSubsystem, ULocalPlayerSubsystem */
	template <typename TSubsystemType>
	TConstArrayView<UClass*> GetDisabledSubsystems() const;

	FORCEINLINE const TArray<FDirectoryPath>& GetAssetPaths() const { return AutomationAssetPaths; }

	/**
	 * If set to false, automation world will use @DefaultGameMode if WorldSettings game mode is empty.
	 * Otherwise, engine automatically uses project default game mode.
	 * This option is created to break dependencies and void loading default game mode, as often it is heavy with code and asset dependencies
	 */
	UPROPERTY(EditAnywhere, Config)
	bool bUseProjectDefaultGameMode = false;

	/**
	 * If set, project and project plugin subsystems are NOT created by default when running automation world
	 * You can still enable them manually via FWorldInitParams, or specify them as Enabled Subsystems in Project Settings
	 */
	UPROPERTY(EditAnywhere, Config)
	bool bDisableProjectSubsystems = true;

	/**
	 * "Default" game mode for automation world, if @bUseProjectDefaultGameMode is set to false
	 * If FWorldInitParams or map's WorldSettings doesn't specify a game mode, this game mode class is used instead
	 */
	UPROPERTY(EditAnywhere, Config, meta = (Validate, EditCondition = "!bUseProjectDefaultGameMode"))
	TSubclassOf<AGameModeBase> DefaultGameMode;

protected:

	/**
	 * A list of paths used for automation asset search.
	 * This way assets can be referenced by name if they're located in one the listed paths
	 */
	UPROPERTY(EditAnywhere, Config, meta = (Validate, DisplayName = "Asset Paths For Automation", LongPackageName))
	TArray<FDirectoryPath> AutomationAssetPaths;
	
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
	void InitializeToDefault(UE::Automation::FSubsystemContainer& Container, TArray<TSubclassOf<TSubsystemType>>& EnabledArray, const FString& ConfigKey) const
	{
		static const TCHAR* ConfigSection{TEXT("/Script/CommonAutomation.CommonAutomationSettings")};

		if (!Container.Initialized())
		{
			Container = UE::Automation::FSubsystemContainer{TSubsystemType::StaticClass()};
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

	/** @return config key name for each subsystem group: UWorldSubsystem, UGameInstanceSubsystem, ULocalPlayerSubsystem */
	template <typename TSubsystemType>
	FString GetConfigKey() const = delete;
	
	UE::Automation::FSubsystemContainer WorldSubsystemContainer;
	UE::Automation::FSubsystemContainer GameInstanceSubsystemContainer;
	UE::Automation::FSubsystemContainer LocalPlayerSubsystemContainer;
};
