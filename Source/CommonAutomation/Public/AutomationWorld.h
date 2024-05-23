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

enum class EWorldInitFlags: uint32
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
ENUM_CLASS_FLAGS(EWorldInitFlags)

using FAutomationWorldPtr = TSharedPtr<FAutomationWorld>;
using FAutomationWorldRef = TSharedRef<FAutomationWorld>;

struct COMMONAUTOMATION_API FAutomationWorldInitParams
{
	FAutomationWorldInitParams(EWorldType::Type InWorldType, EWorldInitFlags InInitFlags, TSubclassOf<AGameModeBase> InGameMode = nullptr)
		: WorldType(InWorldType)
		, InitFlags(InInitFlags)
		, DefaultGameMode(InGameMode)
	{}

	/** @return world initialization values produced from this params */
	FWorldInitializationValues CreateWorldInitValues() const;

	bool HasWorldPackage() const { return !WorldPackage.IsEmpty(); }
	
	bool ShouldInitScene() const;
	bool CreateGameInstance() const { return !!(InitFlags & EWorldInitFlags::CreateGameInstance); }
	bool CreatePrimaryPlayer() const { return !!(InitFlags & EWorldInitFlags::CreateLocalPlayer); }
	bool RouteStartPlay() const { return !!(InitFlags & EWorldInitFlags::StartPlay); }
	
	/** World type */
	EWorldType::Type WorldType = EWorldType::Game;

	/** World package to load */
	FString WorldPackage{};

	/** world load flags, used if WorldPackage is set. Quiet by default */
	ELoadFlags LoadFlags = ELoadFlags::LOAD_Quiet;
	
	/** World initialization flags */
	EWorldInitFlags InitFlags = EWorldInitFlags::None;
	
	/** Default game mode */
	TSubclassOf<AGameModeBase> DefaultGameMode = nullptr;

	/** */
	TDelegate<void(UWorld*)> InitWorld;

	/** */
	TDelegate<void(AWorldSettings*)> InitWorldSettings;
	
	static FAutomationWorldInitParams Minimal;
	static FAutomationWorldInitParams WithGameInstance;
	static FAutomationWorldInitParams WithLocalPlayer;
};

class COMMONAUTOMATION_API FAutomationWorld
{
public:
	
	/**
	 * Create and initialize new automation world with specified init params
	 * @param InitParams initialization params
	 * @return minimal world
	 */
	static FAutomationWorldPtr CreateWorld(const FAutomationWorldInitParams& InitParams);
	
	/** Create empty game world and initialize it */
	static FAutomationWorldPtr CreateGameWorld(EWorldInitFlags InitFlags = EWorldInitFlags::Minimal);

	/**
	 * Load specified world as a game world and initialize it
	 * @WorldPackage long package name pointed to a world asset
	 */
	static FAutomationWorldPtr LoadGameWorld(const FString& WorldPackage, EWorldInitFlags InitFlags = EWorldInitFlags::Minimal);

	/** Load specified world as a game world and initialize it */
	static FAutomationWorldPtr LoadGameWorld(FSoftObjectPath WorldPath, EWorldInitFlags InitFlags = EWorldInitFlags::Minimal);
	
	/** Creates game world with game instance and game mode, immediately routes start play */
	static FAutomationWorldPtr CreateGameWorldWithGameInstance(TSubclassOf<AGameModeBase> DefaultGameMode = nullptr, EWorldInitFlags InitFlags = EWorldInitFlags::None);

	/** Creates game world with local player (meaning game instance and game mode as well), immediately routes start play */
	static FAutomationWorldPtr CreateGameWorldWithPlayer(TSubclassOf<AGameModeBase> DefaultGameMode = nullptr, EWorldInitFlags InitFlags = EWorldInitFlags::None);

	template <typename TGameMode>
	static FAutomationWorldPtr CreateGameWorldWithGameInstance(EWorldInitFlags InitFlags = EWorldInitFlags::None)
	{
		return CreateGameWorldWithGameInstance(TGameMode::StaticClass(), InitFlags);
	}

	template <typename TGameMode>
	static FAutomationWorldPtr CreateGameWorldWithPlayer(EWorldInitFlags InitFlags = EWorldInitFlags::None)
	{
		return CreateGameWorldWithPlayer(TGameMode::StaticClass(), InitFlags);
	}
	
	/** @return whether automation world has been created */
	static bool Exists();

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

	/** tick active world */
	void TickWorld(int32 NumFrames);
	
	/** route end play event to world and actors */
	void RouteEndPlay() const;

	/** @return active world */
	UWorld* GetWorld() const;
	/** @return world context */
	FWorldContext* GetWorldContext() const;
	/** @return game instance */
	UGameInstance* GetGameInstance() const;
	
	~FAutomationWorld();

private:

	void HandleLevelStreamingStateChange(UWorld* OtherWorld, const ULevelStreaming* LevelStreaming, ULevel* LevelIfLoaded, ELevelStreamingState PrevState, ELevelStreamingState NewState);

	FAutomationWorld(UWorld* NewWorld, const FAutomationWorldInitParams& InitParams);

	void InitializeNewWorld(UWorld* InWorld, const FAutomationWorldInitParams& InitParams);
	
	void CreateGameInstance();
	void CreateViewportClient();
	
	UWorld* World = nullptr;
	FWorldContext* WorldContext = nullptr;
	UGameInstance* GameInstance = nullptr;

	/** GWorld value before this automation world was created */
	UWorld* PrevGWorld = nullptr;
	/** GFrameCounter value before this automation world was created */
	uint64 InitialFrameCounter = 0;

	TArray<TObjectPtr<UWorldSubsystem>> WorldSubsystems;
	TArray<TObjectPtr<UGameInstanceSubsystem>> GameInstanceSubsystems;
	
	static UGameInstance* SharedGameInstance;
	static bool bExists;
};

