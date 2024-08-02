#include "AutomationCommon.h"

#include "AutomationTargetPoint.h"
#include "CommonAutomationSettings.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY(LogCommonAutomation);

FAssetData UE::Automation::FindAssetDataByName(const FString& AssetName, EPackageFlags RequiredFlags, const UClass* ClassFilter)
{
	const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	
	FString Paths{};
	for (const FDirectoryPath& Path: UCommonAutomationSettings::Get()->GetAssetPaths())
	{
		FString AssetPath = Path.Path / AssetName;
		if (FPackageName::IsValidLongPackageName(AssetPath) && FPackageName::DoesPackageExist(AssetPath))
		{
			TArray<FAssetData> Assets;
			AssetRegistry.GetAssetsByPackageName(FName{AssetPath}, Assets);

			for (const FAssetData& AssetData: Assets)
			{
				if (AssetData.HasAllPackageFlags(RequiredFlags) &&
					(ClassFilter == nullptr || AssetData.AssetClassPath == ClassFilter->GetClassPathName()))
				{
					return AssetData;
				}
			}
		}
		Paths += AssetPath + TEXT("\n");
	}
	
	UE_LOG(LogCommonAutomation, Error, TEXT("%s: Failed to find asset %s. All matches failed: \n%s"), *FString(__FUNCTION__), *AssetName, *Paths);
	return FAssetData{};
}

FAssetData UE::Automation::FindAssetDataByPath(const FString& AssetPath, EPackageFlags RequiredFlags, const UClass* ClassFilter)
{
	const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	if (FPackageName::IsValidLongPackageName(AssetPath) && FPackageName::DoesPackageExist(AssetPath))
	{
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPackageName(FName{AssetPath}, Assets);

		for (const FAssetData& AssetData: Assets)
		{
			if (AssetData.HasAllPackageFlags(RequiredFlags) &&
				(ClassFilter == nullptr || AssetData.AssetClassPath == ClassFilter->GetClassPathName()))
			{
				return AssetData;
			}
		}
	}

	UE_LOG(LogCommonAutomation, Error, TEXT("%s: Failed to find asset %s."), *FString(__FUNCTION__), *AssetPath);
	return FAssetData{};
}

FSoftObjectPath UE::Automation::FindAssetByName(const FString& AssetName, EPackageFlags RequiredFlags, UClass* ClassFilter)
{
	return FindAssetDataByName(AssetName, RequiredFlags, ClassFilter).ToSoftObjectPath();
}

FSoftObjectPath UE::Automation::FindWorldAssetByName(const FString& AssetName)
{
	return FindAssetDataByName(AssetName, EPackageFlags::PKG_ContainsMap, UWorld::StaticClass()).ToSoftObjectPath();
}

AAutomationTargetPoint* UE::Automation::FindTargetPoint(const UWorld* World, FName Label)
{
	for (TActorIterator<AAutomationTargetPoint> It{World}; It; ++It)
	{
		AAutomationTargetPoint* TargetPoint = *It;
		if (TargetPoint->Label == Label)
		{
			return TargetPoint;
		}
	}

	return nullptr;
}
