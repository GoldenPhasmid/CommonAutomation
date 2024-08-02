#include "CommonAutomationSettings.h"

#include "GameProjectUtils.h"
#include "ModuleDescriptor.h"
#include "GameFramework/GameModeBase.h"

template <>
FString UCommonAutomationSettings::GetConfigKey<UWorldSubsystem>() const
{
	return GET_MEMBER_NAME_CHECKED(ThisClass, WorldSubsystems).ToString();
}

template <>
FString UCommonAutomationSettings::GetConfigKey<UGameInstanceSubsystem>() const
{
	return GET_MEMBER_NAME_CHECKED(ThisClass, GameInstanceSubsystems).ToString();
}

template <>
FString UCommonAutomationSettings::GetConfigKey<ULocalPlayerSubsystem>() const
{
	return GET_MEMBER_NAME_CHECKED(ThisClass, LocalPlayerSubsystems).ToString();
}

template <>
const TArray<UClass*>& UCommonAutomationSettings::GetDisabledSubsystems<UWorldSubsystem>() const
{
	return WorldSubsystemContainer.GetDisabledSubsystems(WorldSubsystems);
}

template <>
const TArray<UClass*>& UCommonAutomationSettings::GetDisabledSubsystems<UGameInstanceSubsystem>() const
{
	return GameInstanceSubsystemContainer.GetDisabledSubsystems(GameInstanceSubsystems);
}

template <>
const TArray<UClass*>& UCommonAutomationSettings::GetDisabledSubsystems<ULocalPlayerSubsystem>() const
{
	return LocalPlayerSubsystemContainer.GetDisabledSubsystems(LocalPlayerSubsystems);
}

UCommonAutomationSettings::UCommonAutomationSettings(const FObjectInitializer& Initializer): Super(Initializer)
{
	DefaultGameMode = AGameModeBase::StaticClass();
}

const UCommonAutomationSettings* UCommonAutomationSettings::Get()
{
	return GetDefault<UCommonAutomationSettings>();
}

UCommonAutomationSettings* UCommonAutomationSettings::GetMutable()
{
	return GetMutableDefault<UCommonAutomationSettings>();
}

void UCommonAutomationSettings::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		InitializeToDefault(WorldSubsystemContainer, WorldSubsystems, *GetConfigKey<UWorldSubsystem>());
		InitializeToDefault(GameInstanceSubsystemContainer, GameInstanceSubsystems, *GetConfigKey<UGameInstanceSubsystem>());
		InitializeToDefault(LocalPlayerSubsystemContainer, LocalPlayerSubsystems, *GetConfigKey<ULocalPlayerSubsystem>());
	}
}

const TArray<FName>& UCommonAutomationSettings::GetProjectModules()
{
	static TArray<FModuleContextInfo, TInlineAllocator<6>> GameModules{GameProjectUtils::GetCurrentProjectModules()};
	static TArray<FModuleContextInfo, TInlineAllocator<32>> PluginModules{GameProjectUtils::GetCurrentProjectPluginModules()};
			
	static TArray<FName> ModuleNames = []() -> TArray<FName>
	{
		TArray<FName> OutModuleNames;
		OutModuleNames.Reserve(GameModules.Num() + PluginModules.Num());

		auto Trans = [](const FModuleContextInfo& Module) { return FName{Module.ModuleName}; };
		Algo::Transform(GameModules, OutModuleNames, Trans);
		Algo::Transform(PluginModules, OutModuleNames, Trans);

		return OutModuleNames;
	}();

	return ModuleNames;
}

bool UCommonAutomationSettings::IsProjectModuleClass(UClass* Class)
{
	const FString ClassPath = Class->GetClassPathName().ToString();
	
	FRegexPattern Regex{TEXT("\\/Script\\/([A-Z][A-Za-z]*)\\.")};
	FRegexMatcher Matcher{Regex, ClassPath};
	
	if (Matcher.FindNext())
	{
		const FName ModuleName{Matcher.GetCaptureGroup(1)};
		return GetProjectModules().Contains(ModuleName);
	}

	checkNoEntry();
	return false;
}

UE::Automation::FSubsystemContainer::FSubsystemContainer(UClass* InBaseType)
	: BaseType(InBaseType)
{
	// initialize all subsystems
	GetDerivedClasses(BaseType, AllSubsystems, true);
	for (auto It = AllSubsystems.CreateIterator(); It; ++It)
	{
		if ((*It)->HasAnyClassFlags(EClassFlags::CLASS_Abstract))
		{
			It.RemoveCurrentSwap();
		}
	}
	Algo::Sort(AllSubsystems);

	// find project module subsystems
	Algo::CopyIf(AllSubsystems, ProjectModuleSubsystems, [](UClass* SubsystemClass)
	{
		return UCommonAutomationSettings::IsProjectModuleClass(SubsystemClass);
	});
}

#if WITH_EDITOR
void UCommonAutomationSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, bDisableProjectSubsystems))
	{
		InitializeToDefault(WorldSubsystemContainer, WorldSubsystems, *GetConfigKey<UWorldSubsystem>());
		InitializeToDefault(GameInstanceSubsystemContainer, GameInstanceSubsystems, *GetConfigKey<UGameInstanceSubsystem>());
		InitializeToDefault(LocalPlayerSubsystemContainer, LocalPlayerSubsystems, *GetConfigKey<ULocalPlayerSubsystem>());
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, WorldSubsystems))
	{
		WorldSubsystemContainer.MarkDirty();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, GameInstanceSubsystems))
	{
		GameInstanceSubsystemContainer.MarkDirty();
	}
}
#endif

