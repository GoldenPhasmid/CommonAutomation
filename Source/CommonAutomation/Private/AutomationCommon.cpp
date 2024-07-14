#include "AutomationCommon.h"

#include "AutomationTargetPoint.h"
#include "CommonAutomationSettings.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY(LogCommonAutomation);

FAssetData UE::Automation::FindAssetDataByName(const FString& AssetName, EPackageFlags RequiredFlags, UClass* ClassFilter)
{
	const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	
	FString Paths{};
	for (const FDirectoryPath& Path: UCommonAutomationSettings::Get()->GetAssetPaths())
	{
		FString WorldPackagePath = Path.Path / AssetName;
		if (FPackageName::IsValidLongPackageName(WorldPackagePath) && FPackageName::DoesPackageExist(WorldPackagePath))
		{
			TArray<FAssetData> Assets;
			AssetRegistry.GetAssetsByPackageName(FName{WorldPackagePath}, Assets);

			for (const FAssetData& AssetData: Assets)
			{
				if (AssetData.HasAllPackageFlags(RequiredFlags) &&
					(ClassFilter == nullptr || AssetData.AssetClassPath == ClassFilter->GetClassPathName()))
				{
					return AssetData;
				}
			}
		}
		Paths += WorldPackagePath + TEXT("\n");
	}
	
	UE_LOG(LogCommonAutomation, Error, TEXT("%s: Failed to find asset %s. All matches failed: \n%s"), *FString(__FUNCTION__), *AssetName, *Paths);
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
