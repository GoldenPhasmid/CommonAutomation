#include "CommonAutomationSettings.h"

#include "GameProjectUtils.h"
#include "ModuleDescriptor.h"

void UCommonAutomationSettings::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FString WorldSubsystemArray{};
		if (!GConfig->GetString(TEXT("/Script/CommonAutomation.CommonAutomationSettings"), TEXT("PersistentWorldSubsystems"), WorldSubsystemArray, GEditorIni)
			|| WorldSubsystemArray.IsEmpty())
		{
			for (UClass* SubsystemClass: GetSubsystemArray(UWorldSubsystem::StaticClass()))
			{
				PersistentWorldSubsystems.Add(SubsystemClass);
			}
		}
		
		FString GISubsystemArray{};
		if (!GConfig->GetString(TEXT("/Script/CommonAutomation.CommonAutomationSettings"), TEXT("PersistentGameInstanceSubsystems"), GISubsystemArray, GEditorIni)
			|| GISubsystemArray.IsEmpty())
		{
			for (UClass* SubsystemClass: GetSubsystemArray(UGameInstanceSubsystem::StaticClass()))
			{
				PersistentGameInstanceSubsystems.Add(SubsystemClass);
			}
		}
	}
}

#if WITH_EDITOR
void UCommonAutomationSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

TArray<UClass*> UCommonAutomationSettings::GetSubsystemArray(UClass* BaseType)
{
	// @todo: disable all project subsystems by default
	TArray<UClass*> SubsystemClasses;
	GetDerivedClasses(BaseType, SubsystemClasses, true);
	
	for (auto It = SubsystemClasses.CreateIterator(); It; ++It)
	{
		if ((*It)->HasAnyClassFlags(EClassFlags::CLASS_Abstract))
		{
			It.RemoveCurrentSwap();
		}
	}
	
	if (bDisableProjectSubsystemsByDefault)
	{
		static TArray<FModuleContextInfo, TInlineAllocator<6>> GameModules{GameProjectUtils::GetCurrentProjectModules()};
		static TArray<FModuleContextInfo, TInlineAllocator<32>> PluginModules{GameProjectUtils::GetCurrentProjectPluginModules()};
	}

	return SubsystemClasses;
}
