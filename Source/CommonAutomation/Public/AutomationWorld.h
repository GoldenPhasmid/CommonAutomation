#pragma once

#include "CoreMinimal.h"
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
struct FAutomationWorldInitParams;

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
	FAutomationWorldInitParams(EWorldType::Type InWorldType, EWorldInitFlags InInitFlags)
		: WorldType(InWorldType)
		, InitFlags(InInitFlags)
	{}
	
	FAutomationWorldInitParams(const FAutomationWorldInitParams& Other) = default;
	FAutomationWorldInitParams(FAutomationWorldInitParams&& Other) = default;
	
	/**
	 * Create automation world from initialization params
	 * Handy method that can be called at the end of init chain:
	 * 
	 * Init(FAutomationWorldInitParams::WithGameMode)
	 * .SetGameMode<AGameMode>()
	 * .EnableSubsystem<UWorldSubsystem>()
	 * .EnableSubsystem<UGameInstanceSubsystem>()
	 * .Create();
	 */
	FAutomationWorldPtr Create() const;

	/** add initialization flags */
	FORCEINLINE FAutomationWorldInitParams& AddFlags(EWorldInitFlags InFlags)
	{
		InitFlags |= InFlags;
		return *this;
	}

	/** remove initialization flags */
	FORCEINLINE FAutomationWorldInitParams& RemoveFlags(EWorldInitFlags InFlags)
	{
		InitFlags &= ~InFlags;
		return *this;
	}

	FORCEINLINE FAutomationWorldInitParams& SetGameMode(TSubclassOf<AGameModeBase> InGameMode)
	{
		DefaultGameMode = InGameMode;
		return *this;
	}
	
	/** set world package to load */
	FAutomationWorldInitParams& SetWorldPackage(FSoftObjectPath InWorldPath);
	FORCEINLINE FAutomationWorldInitParams& SetWorldPackage(const FString& InWorldPackage)
	{
		WorldPackage = InWorldPackage;
		return *this;
	}
	
	template <typename T, TEMPLATE_REQUIRES(TIsDerivedFrom<T, AGameModeBase>::Value)>
	FAutomationWorldInitParams& SetGameMode()
	{
		DefaultGameMode = T::StaticClass();
		return *this;
	}

	template <typename T, TEMPLATE_REQUIRES(TIsDerivedFrom<T, UGameInstanceSubsystem>::Value)>
	FAutomationWorldInitParams& EnableSubsystem()
	{
		GameSubsystems.Add(T::StaticClass());
		return *this;
	}

	template <typename T, TEMPLATE_REQUIRES(TIsDerivedFrom<T, UWorldSubsystem>::Value)>
	FAutomationWorldInitParams& EnableSubsystem()
	{
		WorldSubsystems.Add(T::StaticClass());
		return *this;
	}

	template <typename T, TEMPLATE_REQUIRES(TIsDerivedFrom<T, ULocalPlayerSubsystem>::Value)>
	FAutomationWorldInitParams& EnableSubsystem()
	{
		PlayerSubsystems.Add(T::StaticClass());
		return *this;
	}
	
	FORCEINLINE FAutomationWorldInitParams& SetInitWorld(TDelegate<void(UWorld*)>&& Callback)
	{
		InitWorld = Callback;
		return *this;
	}

	FORCEINLINE FAutomationWorldInitParams& SetInitWorldSettings(TDelegate<void(AWorldSettings*)>&& Callback)
	{
		InitWorldSettings = Callback;
		return *this;
	}

	template <typename T>
	FAutomationWorldInitParams& SetInitWorld(T&& Callback)
	{
		InitWorld.BindLambda(Forward<T>(Callback));
		return *this;
	}
	
	template <typename T>
	FAutomationWorldInitParams& SetInitWorldSettings(T&& Callback)
	{
		InitWorldSettings.BindLambda(Forward<T>(Callback));
		return *this;
	}
	
	/** @return world initialization values produced from this params */
	FWorldInitializationValues CreateWorldInitValues() const;

	FORCEINLINE bool HasWorldPackage() const { return !WorldPackage.IsEmpty(); }
	
	FORCEINLINE bool ShouldInitScene() const;
	FORCEINLINE bool CreateGameInstance() const { return !!(InitFlags & EWorldInitFlags::CreateGameInstance); }
	FORCEINLINE bool CreatePrimaryPlayer() const { return !!(InitFlags & EWorldInitFlags::CreateLocalPlayer); }
	FORCEINLINE bool RouteStartPlay() const { return !!(InitFlags & EWorldInitFlags::StartPlay); }
	
	/** World type */
	EWorldType::Type WorldType = EWorldType::Game;

	/** World initialization flags */
	EWorldInitFlags InitFlags = EWorldInitFlags::None;

	/** World package to load */
	FString WorldPackage{};

	/** world load flags, used if WorldPackage is set. Quiet by default */
	ELoadFlags LoadFlags = ELoadFlags::LOAD_Quiet;
	
	/** Default game mode */
	TSubclassOf<AGameModeBase> DefaultGameMode = nullptr;

	/** world initialization delegate */
	TDelegate<void(UWorld*)> InitWorld;

	/** world settings initialization delegate */
	TDelegate<void(AWorldSettings*)> InitWorldSettings;

	/** A list of game instance subsystems that would be created as part of automation world */
	TArray<UClass*, TInlineAllocator<4>> GameSubsystems;
	/** A list of world subsystems that would be created as part of automation world */
	TArray<UClass*, TInlineAllocator<4>> WorldSubsystems;
	/** A list of local player subsystems that would be created as part of automation world */
	TArray<UClass*, TInlineAllocator<4>> PlayerSubsystems;

	static const FAutomationWorldInitParams Minimal;
	static const FAutomationWorldInitParams WithGameInstance;
	static const FAutomationWorldInitParams WithLocalPlayer;
};

FORCEINLINE FAutomationWorldInitParams Init(const FAutomationWorldInitParams& InitParams)
{
	return InitParams;
}

using FWorldInitParams = FAutomationWorldInitParams;

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
	static FAutomationWorldPtr LoadGameWorld(const FSoftObjectPath& WorldPath, EWorldInitFlags InitFlags = EWorldInitFlags::Minimal);
	
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
	static FAutomationWorldPtr LoadEditorWorld(const FSoftObjectPath& WorldPath, EWorldInitFlags InitFlags = EWorldInitFlags::Minimal);
	
	/** @return whether automation world has been created */
	static bool Exists();

	/** Create and return game instance subsystem */
	UGameInstanceSubsystem* GetOrCreateSubsystem(TSubclassOf<UGameInstanceSubsystem> SubsystemClass);

	/** Create and return world subsystem */
	UWorldSubsystem* GetOrCreateSubsystem(TSubclassOf<UWorldSubsystem> SubsystemClass);
	
	/** Create subsystem of specified type */
	template <
		typename T,
		TEMPLATE_REQUIRES(TOr<TIsDerivedFrom<T, UGameInstanceSubsystem>, TIsDerivedFrom<T, UWorldSubsystem>>::Value)
	>
	T* GetOrCreateSubsystem()
	{
		return CastChecked<T>(GetOrCreateSubsystem(TSubclassOf<T>{T::StaticClass()}), ECastCheckedType::NullAllowed);
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

	FAutomationWorld(UWorld* NewWorld, const FAutomationWorldInitParams& InitParams);

	void HandleTestCompleted(FAutomationTestBase* Test);
	void HandleLevelStreamingStateChange(UWorld* OtherWorld, const ULevelStreaming* LevelStreaming, ULevel* LevelIfLoaded, ELevelStreamingState PrevState, ELevelStreamingState NewState);

	void InitializeNewWorld(UWorld* InWorld, const FAutomationWorldInitParams& InitParams);
	USubsystem* AddAndInitializeSubsystem(FSubsystemCollectionBase* Collection, TSubclassOf<USubsystem> SubsystemClass, UObject* Outer);
	
	void CreateGameInstance(const FAutomationWorldInitParams& InitParams);
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
	/** Handle to TestEndEvent delegate */
	FDelegateHandle TestCompletedHandle;
	/** Handle to LevelStreamingStateChanged delegate */
	FDelegateHandle StreamingStateHandle;

	/** Cached pointer to a world subsystem collection, retrieved in a fancy way from @World */
	FObjectSubsystemCollection<UWorldSubsystem>* WorldCollection = nullptr;
	/** Cached pointer to a game subsystem collection, retrieved in a fancy way from @GameInstance. can be null */
	FObjectSubsystemCollection<UGameInstanceSubsystem>* GameInstanceCollection = nullptr;
	/** A list of player subsystems that should be created for a local player */
	TArray<UClass*, TInlineAllocator<4>> PlayerSubsystems;
	
	static UGameInstance* SharedGameInstance;
	static bool bExists;
};

template <>
FORCEINLINE UGameInstanceSubsystem* FAutomationWorld::GetOrCreateSubsystem<UGameInstanceSubsystem>()
{
	return GetOrCreateSubsystem(TSubclassOf<UGameInstanceSubsystem>{UGameInstanceSubsystem::StaticClass()});
}

template <>
FORCEINLINE UWorldSubsystem* FAutomationWorld::GetOrCreateSubsystem<UWorldSubsystem>()
{
	return GetOrCreateSubsystem(TSubclassOf<UWorldSubsystem>{UWorldSubsystem::StaticClass()});
}


