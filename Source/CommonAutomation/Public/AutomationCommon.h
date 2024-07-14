#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

#include "AutomationCommon.generated.h"

struct FAssetData;
class AAutomationTargetPoint;

/**
 * Base struct for automation test custom data
 * Stored in target points and other entities
 */
USTRUCT(meta = (Hidden))
struct COMMONAUTOMATION_API FAutomationTestCustomData
{
	GENERATED_BODY()
};

namespace UE::Automation
{
	/** @return asset data for an asset specified by name */
	template <typename T>
	FAssetData FindAssetDataByName(const FString& AssetName, EPackageFlags RequiredFlags = EPackageFlags::PKG_None)
	{
		return FindAssetDataByName(AssetName, RequiredFlags, T::StaticClass());
	}
	
	/**
	 * @return asset data for an asset specified by name
	 * @param AssetName
	 * @param RequiredFlags
	 * @param ClassFilter
	 */
	COMMONAUTOMATION_API FAssetData	FindAssetDataByName(
		const FString& AssetName,
		EPackageFlags RequiredFlags = EPackageFlags::PKG_None,
		UClass* ClassFilter = nullptr
	);
	
	/**
	 * @return asset path for an asset specified by name
	 * @param AssetName
	 * @param RequiredFlags
	 * @param ClassFilter
	 */
	COMMONAUTOMATION_API FSoftObjectPath FindAssetByName(
		const FString& AssetName,
		EPackageFlags RequiredFlags = EPackageFlags::PKG_None,
		UClass* ClassFilter = nullptr
	);
	
	/** @return world asset with a specified name */
	COMMONAUTOMATION_API FSoftObjectPath FindWorldAssetByName(
		const FString& AssetName
	);

	/** @return target point from a world identified by a label */
	COMMONAUTOMATION_API AAutomationTargetPoint* FindTargetPoint(
		const UWorld* World,
		FName Label
	);
}

DECLARE_LOG_CATEGORY_EXTERN(LogCommonAutomation, Log, Log);
