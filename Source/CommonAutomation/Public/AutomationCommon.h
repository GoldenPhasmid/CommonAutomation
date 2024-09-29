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
	
	/** @return newly created and registered actor component for @OwnerActor */
	template <typename TComponentType>
	TComponentType* CreateActorComponent(UWorld* World, AActor* OwnerActor, UClass* ComponentClass = TComponentType::StaticClass(), FName Name = NAME_None)
	{
		check(World != nullptr && OwnerActor != nullptr && ComponentClass != nullptr);
		TComponentType* Component = NewObject<TComponentType>(OwnerActor, ComponentClass, Name);
		Component->RegisterComponentWithWorld(World);

		return Component;
	}

	/** create and register actor component for @OwnerActor */
	template <typename TComponentType>
	void AddActorComponent(UWorld* World, AActor* OwnerActor, UClass* ComponentClass = TComponentType::StaticClass(), FName Name = NAME_None)
	{
		CreateActorComponent<TComponentType>(World, OwnerActor, ComponentClass, Name);
	}
	
	/** @return asset data for an asset specified by name */
	template <typename T>
	FAssetData FindAssetDataByName(const FString& AssetName, EPackageFlags RequiredFlags = EPackageFlags::PKG_None)
	{
		return FindAssetDataByName(AssetName, RequiredFlags, T::StaticClass());
	}

	template <typename T>
	FAssetData FindAssetDataByPath(const FString& AssetPath, EPackageFlags RequiredFlags = EPackageFlags::PKG_None)
	{
		return FindAssetDataByPath(AssetPath, RequiredFlags, T::StaticClass());
	}
	
	/**
	 * @return asset data for an asset specified by name.
	 * @param AssetName
	 * @param RequiredFlags
	 * @param ClassFilter
	 */
	COMMONAUTOMATION_API FAssetData	FindAssetDataByName(
		const FString& AssetName,
		EPackageFlags RequiredFlags = EPackageFlags::PKG_None,
		const UClass* ClassFilter = nullptr
	);

	/**
	 * @return asset data for an asset specified by full path.
	 * @param AssetPath
	 * @param RequiredFlags
	 * @param ClassFilter
	 */
	COMMONAUTOMATION_API FAssetData FindAssetDataByPath(
		const FString& AssetPath,
		EPackageFlags RequiredFlags = EPackageFlags::PKG_None,
		const UClass* ClassFilter = nullptr
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
