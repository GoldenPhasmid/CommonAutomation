#pragma once

#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"

class UAutomationGameInstance;
class UWorld;
class AWorldSettings;
class AGameModeBase;
class ULocalPlayer;
class FAutomationWorld;
class UWorldSubsystem;
class UGameInstanceSubsystem;

enum class EWorldInitType: uint32
{
	None				= 0,
	InitScene			= 1 << 0,
	InitAudio			= 1 << 1,
	InitHitProxy		= 1 << 2,
	InitPhysics			= 1 << 3,
	InitNavigation		= 1 << 4,
	InitAI				= 1 << 5,
	InitWeldedBodies	= 1 << 6,
	InitCollision		= 1 << 7,
	InitFX				= 1 << 8,
	InitWorldPartition	= 1 << 9,

	CreateGameInstance  = 1 << 10,
	CreateLocalPlayer	= 1 << 11,
	StartPlay			= 1 << 12,

	Minimal				= InitScene | StartPlay,
	WithGameInstance	= InitScene | StartPlay | CreateGameInstance,
	WithLocalPlayer		= InitScene | StartPlay | CreateGameInstance | CreateLocalPlayer
};
ENUM_CLASS_FLAGS(EWorldInitType)

using FAutomationWorldPtr = TSharedPtr<FAutomationWorld>;
using FAutomationWorldRef = TSharedRef<FAutomationWorld>;

struct COMMONAUTOMATION_API FAutomationWorldInitParams
{
	FAutomationWorldInitParams() = default;
	FAutomationWorldInitParams(EWorldType::Type InWorldType, EWorldInitType InInitValues)
		: WorldType(InWorldType)
		, InitValues(InInitValues)
	{}
	
	/** */
	EWorldType::Type WorldType = EWorldType::Game;

	/** */
	EWorldInitType InitValues = EWorldInitType::None;
	
	/** */
	TSubclassOf<AGameModeBase> DefaultGameMode = nullptr;
	
	/** */
	TFunction<void(UWorld* World)> InitWorld;

	/** */
	TFunction<void(AWorldSettings* WorldSettings)> InitWorldSettings;

	static FAutomationWorldInitParams Minimal;
	static FAutomationWorldInitParams WithGameInstance;
	static FAutomationWorldInitParams WithLocalPlayer;
};

class COMMONAUTOMATION_API FAutomationWorld
{
public:
	/**
	 * 
	 * @param InitParams 
	 * @return 
	 */
	static FAutomationWorldPtr CreateWorld(const FAutomationWorldInitParams& InitParams);

	/**
	 * Create world for automation tests
	 * @DefaultGameMode world type to create
	 * @InitValues whether perform world initialization and route BeginPlay
	 * @return minimal world
	 */
	static FAutomationWorldPtr CreateGameWorld(TSubclassOf<AGameModeBase> DefaultGameMode = nullptr, EWorldInitType InitValues = EWorldInitType::Minimal);
	
	/** Creates game world with game instance and game mode, immediately routes start play */
	static FAutomationWorldPtr CreateGameWorldWithGameInstance(TSubclassOf<AGameModeBase> DefaultGameMode = nullptr);

	/** Creates game world with local player (meaning game instance and game mode as well), immediately routes start play */
	static FAutomationWorldPtr CreateGameWorldWithPlayer(TSubclassOf<AGameModeBase> DefaultGameMode = nullptr);
	
	/** @return whether minimal world has been created */
	static bool IsRunningAutomationWorld();

	/** Create and return game instance subsystem */
	UGameInstanceSubsystem* CreateSubsystem(TSubclassOf<UGameInstanceSubsystem> SubsystemClass);

	/** Create and return world subsystem */
	UWorldSubsystem* CreateSubsystem(TSubclassOf<UWorldSubsystem> SubsystemClass);
	
	/** Create subsystem of specified type */
	template <typename T>
	T* CreateSubsystem()
	{
		if constexpr (TIsDerivedFrom<T, UGameInstanceSubsystem>::Value)
		{
			return CastChecked<T>(CreateSubsystem(TSubclassOf<UGameInstanceSubsystem>{T::StaticClass()}));
		}
		if constexpr (TIsDerivedFrom<T, UWorldSubsystem>::Value)
		{
			return CastChecked<T>(CreateSubsystem(TSubclassOf<UWorldSubsystem>{T::StaticClass()}));
		}
		
		checkNoEntry();
		return nullptr;
	}

	/** create primary player for this world. If player has already been created, return it */
	ULocalPlayer* GetOrCreatePrimaryPlayer();
	
	/** create new local player instance, along with PlayerController, HUD, etc. */
	ULocalPlayer* CreateLocalPlayer();

	/** perform logout for given local player */
	void DestroyLocalPlayer(ULocalPlayer* LocalPlayer);

	/** route begin play event to world and actors */
	void RouteStartPlay() const;

	/** tick minimal world */
	void TickWorld(int32 NumFrames);
	
	/** route end play event to world and actors */
	void RouteEndPlay() const;

	/** @return World owned by this minimal world */
	UWorld* GetWorld() const;
	FWorldContext* GetWorldContext() const;
	
	~FAutomationWorld();

private:

	friend class UAutomationGameInstance;

	static UAutomationGameInstance* CreateGameInstance(FAutomationWorld& MinimalWorld);

	void HandleLevelStreamingStateChange(UWorld* OtherWorld, const ULevelStreaming* LevelStreaming, ULevel* LevelIfLoaded, ELevelStreamingState PrevState, ELevelStreamingState NewState);

	FAutomationWorld();

	FAutomationWorldInitParams InitParams;
	FWorldInitializationValues InitValues;
	UWorld* World = nullptr;
	FWorldContext* WorldContext = nullptr;
	uint64 InitialFrameCounter = 0;

	TArray<TObjectPtr<UWorldSubsystem>> WorldSubsystems;
	TArray<TObjectPtr<UGameInstanceSubsystem>> GameInstanceSubsystems;
	
	static bool bRunningAutomationWorld;
};

