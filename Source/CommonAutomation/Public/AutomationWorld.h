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

/**
 * RAII wrapper to create, initialize and destroy a world. Can be used to test various levels in Game mode and Editor mode.
 * It is designed to run in a single automation test scope and destroyed after test has finished.
 * Automation world should not be stored or explicitly destroyed by calling Reset when the test has finished. You cannot
 * create multiple instances of an automation world by design, you will get an assertion
 * Sets GWorld and other global properties to try to behave as close as possible to the real world in PIE/Game
 */
class COMMONAUTOMATION_API FAutomationWorld
{
public:
	
	/**
	 * Create and initialize new automation world with specified init params
	 * @param InitParams initialization params
	 * @return minimal world
	 */
	static FAutomationWorldPtr CreateWorld(const FAutomationWorldInitParams& InitParams);

	/** Create an empty game world and initialize it */
	static FAutomationWorldPtr CreateGameWorld(EWorldInitFlags InitFlags = EWorldInitFlags::Minimal);
	
	/** Creates a game world with game instance and game mode, immediately routes start play */
	static FAutomationWorldPtr CreateGameWorldWithGameInstance(TSubclassOf<AGameModeBase> DefaultGameMode = nullptr, EWorldInitFlags InitFlags = EWorldInitFlags::None);

	/** Creates a game world with local player (meaning game instance and game mode as well), immediately routes start play */
	static FAutomationWorldPtr CreateGameWorldWithPlayer(TSubclassOf<AGameModeBase> DefaultGameMode = nullptr, EWorldInitFlags InitFlags = EWorldInitFlags::None);

	/**
	 * Load specified world as a game world and initialize it
	 * @WorldPackage long package name pointed to a world asset, for example /Game/Maps/Startup
	 */
	static FAutomationWorldPtr LoadGameWorld(const FString& WorldPackage, EWorldInitFlags InitFlags = EWorldInitFlags::Minimal);

	/**
	 * Load specified world as a game world and initialize it
	 * @WorldPath soft world asset, for example /Game/Maps/Startup
	 */
	static FAutomationWorldPtr LoadGameWorld(FSoftObjectPath WorldPath, EWorldInitFlags InitFlags = EWorldInitFlags::Minimal);
	
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
	
	/** Creates an editor world and initializes it */
	static FAutomationWorldPtr CreateEditorWorld(EWorldInitFlags InitFlags = EWorldInitFlags::Minimal);

	/**
	 * Load specified world as an editor world and initialize it
	 * @WorldPackage long package name pointed to a world asset, for example /Game/Maps/Startup
	 */
	static FAutomationWorldPtr LoadEditorWorld(const FString& WorldPackage, EWorldInitFlags InitFlags = EWorldInitFlags::Minimal);

	/**
	 * Load specified world as an editor world and initialize it
	 * @WorldPath soft world asset, for example /Game/Maps/Startup
	 */
	static FAutomationWorldPtr LoadEditorWorld(FSoftObjectPath WorldPath, EWorldInitFlags InitFlags = EWorldInitFlags::Minimal);
	
	/** @return whether automation world has been created */
	static bool Exists();

	/** Create and return game instance subsystem */
	UGameInstanceSubsystem* CreateSubsystem(TSubclassOf<UGameInstanceSubsystem> SubsystemClass);

	/** Create and return world subsystem */
	UWorldSubsystem* CreateSubsystem(TSubclassOf<UWorldSubsystem> SubsystemClass);
	
	/** Create subsystem of specified type */
	template <
		typename T,
		TEMPLATE_REQUIRES(std::disjunction_v<std::is_same<T, UGameInstanceSubsystem>, std::is_same<T, UWorldSubsystem>>)
	>
	T* CreateSubsystem()
	{
		return CreateSubsystem(TSubclassOf<T>{T::StaticClass()});
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

	const TArray<UWorldSubsystem*>& GetWorldSubsystems() const;

	UWorld* World = nullptr;
	FWorldContext* WorldContext = nullptr;
	UGameInstance* GameInstance = nullptr;
	/** cached tick type, different for game and editor world */
	ELevelTick TickType = LEVELTICK_All;

	/** GWorld value before this automation world was created */
	UWorld* PrevGWorld = nullptr;
	/** GFrameCounter value before this automation world was created */
	uint64 InitialFrameCounter = 0;
	
	FSubsystemCollection<UWorldSubsystem> WorldSubsystemCollection;
	FSubsystemCollection<UGameInstanceSubsystem> GameInstanceSubsystemCollection;
	
	static UGameInstance* SharedGameInstance;
	static bool bExists;
};

template <>
FORCEINLINE UGameInstanceSubsystem* FAutomationWorld::CreateSubsystem<UGameInstanceSubsystem>()
{
	return CreateSubsystem(TSubclassOf<UGameInstanceSubsystem>{UGameInstanceSubsystem::StaticClass()});
}

template <>
FORCEINLINE UWorldSubsystem* FAutomationWorld::CreateSubsystem<UWorldSubsystem>()
{
	return CreateSubsystem(TSubclassOf<UWorldSubsystem>{UWorldSubsystem::StaticClass()});
}


