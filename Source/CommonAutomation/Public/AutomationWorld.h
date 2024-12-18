#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "EngineUtils.h"
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
	None				= 0,		// No flags
	InitScene			= 1 << 0,	// If set, will initialize FScene for rendering. Will be set automatically if world requires HitProxy, Physics Simulation, Trace Collision or FX
	InitAudio			= 1 << 1,	// If set, will initialize audio mixer
	InitHitProxy		= 1 << 2,	// If set, will initialize editor world hit proxies
	InitPhysics			= 1 << 3,	// If set, will initialize physics scene handler
	InitNavigation		= 1 << 4,	// If set, will properly initialize UNavigationSystem
	InitAI				= 1 << 5,	// If set, will create and initialize AISystem
	InitWeldedBodies	= 1 << 6,	// 
	InitCollision		= 1 << 7,	// If set, will initialize collision handler
	InitFX				= 1 << 8,	// If set, will initialize FXSystem
	InitWorldPartition	= 1 << 9,	// If set, will create and initialize UWorldPartition
	DisableStreaming	= 1 << 10,  // If set, will enable streaming for UWorldPartition

	CreateGameInstance  = 1 << 11,	// creates game instance and game mode during initialization. By default, automation world runs without them
	CreateLocalPlayer	= 1 << 12,	// creates local player during initialization
	StartPlay			= 1 << 13,	// calls BeginPlay during initialization

	// @todo investigate if InitScene can be removed from default options
	Minimal				= InitScene | StartPlay,											// initializes scene and calls BeginPlay for game worlds
	WithBeginPlay		= InitScene | StartPlay,											// alternative to Minimal
	WithGameInstance	= InitScene | StartPlay | CreateGameInstance,						// same as WithBeginPlay, but also creates game instance
	WithLocalPlayer		= InitScene | StartPlay | CreateGameInstance | CreateLocalPlayer	// same as WithGameInstance, but creates one primary local player as well
};
ENUM_CLASS_FLAGS(EWorldInitFlags)

using FAutomationWorldPtr = TSharedPtr<FAutomationWorld>;
using FAutomationWorldRef = TSharedRef<FAutomationWorld>;

/**
 * Initialization params for automation world
 * Each setter returns a reference to itself, so users can create chain initialization inside a single definition
 * Handy method that can be called at the end of init chain:
 * 
 * FAutomationWorldPtr AutoWorld = Init(FWorldInitParams::WithGameMode)
 * .AddFlags(EWorldInitFlags::InitNavigation)
 * .RemoveFlags(EWorldInitFlags::StartPlay)
 * .SetGameMode<AGameMode>()
 * .EnableSubsystem<UWorldSubsystem>()
 * .EnableSubsystem<UGameInstanceSubsystem>()
 * .Create();
 * 
 */
struct COMMONAUTOMATION_API FAutomationWorldInitParams
{
	FAutomationWorldInitParams(EWorldType::Type InWorldType, EWorldInitFlags InInitFlags)
		: WorldType(InWorldType)
		, InitFlags(InInitFlags)
	{}
	
	FAutomationWorldInitParams(const FAutomationWorldInitParams& Other) = default;
	FAutomationWorldInitParams(FAutomationWorldInitParams&& Other) = default;
	
	/** Create automation world from initialization params */
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
		if (Callback)
		{
			InitWorld.BindLambda(Forward<T>(Callback));
		}
		return *this;
	}
	
	template <typename T>
	FAutomationWorldInitParams& SetInitWorldSettings(T&& Callback)
	{
		if (Callback)
		{
			InitWorldSettings.BindLambda(Forward<T>(Callback));
		}
		return *this;
	}
	
	/** @return world initialization values produced from this params */
	FWorldInitializationValues CreateWorldInitValues() const;

	FORCEINLINE bool HasWorldPackage() const { return WorldPackage.IsSet(); }
	FORCEINLINE FString GetWorldPackage() const { return WorldPackage.GetValue(); }
	
	bool ShouldInitScene() const;
	bool ShouldInitWorldPartition() const;
	FORCEINLINE bool CreateGameInstance() const { return !!(InitFlags & EWorldInitFlags::CreateGameInstance); }
	FORCEINLINE bool CreatePrimaryPlayer() const { return !!(InitFlags & EWorldInitFlags::CreateLocalPlayer); }
	FORCEINLINE bool RouteStartPlay() const { return !!(InitFlags & EWorldInitFlags::StartPlay); }
	FORCEINLINE bool IsEditorWorld() const { return WorldType == EWorldType::Editor; }
	
	/** World type */
	EWorldType::Type WorldType = EWorldType::Game;

	/** World initialization flags */
	EWorldInitFlags InitFlags = EWorldInitFlags::None;

	/** World package to load */
	TOptional<FString> WorldPackage;

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
	static const FAutomationWorldInitParams WithBeginPlay;
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
 * Automation world tries to behave as close as possible to the real game/editor world.
 * Automation test cannot create multiple instances of automation world - in real game scenario, there's one global world and one global game instance.
 *
 * In common case you create automation world at the beginning of your test setup, either inside FAutomationTest or FAutomationSpec:
 *
 *	bool FMyTest::Run()
 *	{
 *		FAutomationWorldPtr ScopedWorld = FAutomationWorld::CreateGameWorld();
 *		ScopedWorld->GetWorld()->SpawnActor<AMyActor>();
 *		//... do test checks, automation world is destroyed automatically when goes out of scope
 *	}
 *
 *	bool FMySpec::Define()
 *	{
 *		BeforeEach([]
 *		{
 *			ScopedWorld = FAutomationWorld::CreateGameWorld();
 *		});
 *
 *		AfterEach([]
 *		{
 *			// for automation specs scoped world should be manually reset
 *			ScopedWorld.Reset();
 *		});
 *	}
 *
 *	CreateWorld function family creates a new empty world from scratch.
 *	LoadWorld	function family requires a valid world package located on disk or in memory.
 *	Use LoadWorld only for tests that require explicit setup in editor, like pathfinding or building navigation. Otherwise, prefer creating world from scratch.
 *	
 *	You can further customize world initialization by creating FAutomationWorldInitParams structure as a CreateWorld argument.
 *	@see AutomationWorldTests.cpp to check for full invariants that automation world provides and various ways to initialize it.
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
	static FAutomationWorldPtr CreateGameWorld(EWorldInitFlags InitFlags = EWorldInitFlags::WithBeginPlay);
	
	/** Creates a game world with game instance and game mode, immediately routes start play */
	static FAutomationWorldPtr CreateGameWorldWithGameInstance(TSubclassOf<AGameModeBase> DefaultGameMode = nullptr, EWorldInitFlags InitFlags = EWorldInitFlags::None);

	/** Creates a game world with local player (meaning game instance and game mode as well), immediately routes start play */
	static FAutomationWorldPtr CreateGameWorldWithPlayer(TSubclassOf<AGameModeBase> DefaultGameMode = nullptr, EWorldInitFlags InitFlags = EWorldInitFlags::None);

	/**
	 * Load specified world as a game world and initialize it
	 * @WorldPackage long package name pointed to a world asset, for example /Game/Maps/Startup
	 */
	static FAutomationWorldPtr LoadGameWorld(const FString& WorldPackage, EWorldInitFlags InitFlags = EWorldInitFlags::WithBeginPlay);

	/**
	 * Load specified world as a game world and initialize it
	 * @WorldPath soft world asset, for example /Game/Maps/Startup
	 */
	static FAutomationWorldPtr LoadGameWorld(const FSoftObjectPath& WorldPath, EWorldInitFlags InitFlags = EWorldInitFlags::WithBeginPlay);
	
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
	static FAutomationWorldPtr CreateEditorWorld(EWorldInitFlags InitFlags = EWorldInitFlags::WithBeginPlay);

	/**
	 * Load specified world as an editor world and initialize it
	 * @WorldPackage long package name pointed to a world asset, for example /Game/Maps/Startup
	 */
	static FAutomationWorldPtr LoadEditorWorld(const FString& WorldPackage, EWorldInitFlags InitFlags = EWorldInitFlags::WithBeginPlay);

	/**
	 * Load specified world as an editor world and initialize it
	 * @WorldPath soft world asset, for example /Game/Maps/Startup
	 */
	static FAutomationWorldPtr LoadEditorWorld(const FSoftObjectPath& WorldPath, EWorldInitFlags InitFlags = EWorldInitFlags::WithBeginPlay);
	
	/** @return whether automation world has been created */
	static bool Exists();

	/** Create and return game instance subsystem */
	UGameInstanceSubsystem* GetOrCreateSubsystem(TSubclassOf<UGameInstanceSubsystem> SubsystemClass);

	/** Create and return world subsystem */
	UWorldSubsystem* GetOrCreateSubsystem(TSubclassOf<UWorldSubsystem> SubsystemClass);
	
	/** Get or create subsystem of specified type */
	template <
		typename T,
		TEMPLATE_REQUIRES(TOr<TIsDerivedFrom<T, UGameInstanceSubsystem>, TIsDerivedFrom<T, UWorldSubsystem>>::Value)
	>
	T* GetOrCreateSubsystem()
	{
		return CastChecked<T>(GetOrCreateSubsystem(TSubclassOf<T>{T::StaticClass()}), ECastCheckedType::NullAllowed);
	}

	template <typename T, TEMPLATE_REQUIRES(TIsDerivedFrom<T, UGameInstanceSubsystem>::Value)>
	T* GetSubsystem()
	{
		return CastChecked<T>(GameInstance->GetSubsystem<T>(), ECastCheckedType::NullAllowed);
	}

	template <typename T, TEMPLATE_REQUIRES(TIsDerivedFrom<T, UWorldSubsystem>::Value)>
	T* GetSubsystem()
	{
		return CastChecked<T>(World->GetSubsystem<T>(), ECastCheckedType::NullAllowed);
	}

	/** implicit conversion operator to UWorld* */
	operator UWorld*() const
	{
		return GetWorld();
	}

	FORCEINLINE bool IsEditorWorld() const
	{
		return World->WorldType == EWorldType::Editor;
	}
	
	/** create primary player for this world. If player has already been created, return it */
	ULocalPlayer* GetOrCreatePrimaryPlayer(bool bSpawnPlayerController = true);
	
	/** create new local player instance, along with PlayerController, HUD, etc. */
	ULocalPlayer* CreateLocalPlayer(bool bSpawnPlayerController = true);

	/** perform logout for given local player */
	void DestroyLocalPlayer(ULocalPlayer* LocalPlayer);

	/** route begin play event to world and actors */
	void RouteStartPlay() const;

	/** tick active world */
	void TickWorld(int32 NumFrames);

	/** route end play event to world and actors */
	void RouteEndPlay() const;

	/**
	* travel to a new world initiated via UGameplayStatics::OpenLevel
	 * After completion, GetWorld() will return a newly loaded world. Game instance and local players are unchanged
	 * Does nothing if there's not pending travel
	 * @param WorldToTravel world to travel
	 * @param TravelOptions travel options
	 */
	void AbsoluteWorldTravel(TSoftObjectPtr<UWorld> WorldToTravel, TSubclassOf<AGameModeBase> GameModeClass = nullptr, FString TravelOptions = {});

	/**
	 * travel to a new world initiated via UGameplayStatics::OpenLevel
	 * After completion, GetWorld() will return a newly loaded world. Game instance and local players are unchanged
	 * Does nothing if there's not pending travel
	 */
	void FinishWorldTravel();

	/** @return active world */
	UWorld* GetWorld() const;
	/** @return world context */
	FWorldContext* GetWorldContext() const;
	/** @return game instance */
	UGameInstance* GetGameInstance() const;
	/** @return auth game mode */
	template <typename T = AGameModeBase>
	T* GetGameMode() const
	{
		return World->GetAuthGameMode<T>();
	}
	/** @return game state */
	template <typename T = AGameStateBase>
	AGameStateBase* GetGameState() const
	{
		return World->GetGameState<T>();
	}

	/** spawn actor overload */
	template <typename T = AActor>
	T* SpawnActor(UClass* Class = T::StaticClass(), const FTransform& Transform = FTransform::Identity, FActorSpawnParameters SpawnParams = FActorSpawnParameters{})
	{
		return CastChecked<T>(GetWorld()->SpawnActor(Class, &Transform, SpawnParams), ECastCheckedType::NullAllowed);
	}

	/** spawn actor overload */
	template <typename T = AActor>
	T* SpawnActorSimple(FActorSpawnParameters SpawnParams = FActorSpawnParameters{})
	{
		static const FTransform Identity = FTransform::Identity;
		return CastChecked<T>(GetWorld()->SpawnActor(T::StaticClass(), &Identity, SpawnParams), ECastCheckedType::NullAllowed);
	}
	
	/** @return actor with a given tag */
	template <typename T = AActor>
	T* FindActorByTag(FName Tag)
	{
		for (TActorIterator<AActor> It(World, T::StaticClass()); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor->ActorHasTag(Tag))
			{
				return CastChecked<T>(Actor);
			}
		}
	
		return nullptr;
	}

	/** @return first actor of a given type */
	template <typename T = AActor>
	T* FindActorByType()
	{
		for (TActorIterator<AActor> It(World, T::StaticClass()); It; ++It)
		{
			AActor* Actor = *It;
			return CastChecked<T>(Actor);
		}

		return nullptr;
	}
	
	
	~FAutomationWorld();
	// explicitly deleted copy/move constructor/assignment
	FAutomationWorld(const FAutomationWorld& Other) = delete;
	FAutomationWorld(FAutomationWorld&& Other) = delete;
	FAutomationWorld& operator=(const FAutomationWorld& Other) = delete;
	FAutomationWorld& operator=(FAutomationWorld&& Other) = delete;
private:

	FAutomationWorld(UWorld* NewWorld, const FAutomationWorldInitParams& InitParams);

	void HandleTestCompleted(FAutomationTestBase* Test);
	void HandleLevelStreamingStateChange(UWorld* OtherWorld, const ULevelStreaming* LevelStreaming, ULevel* LevelIfLoaded, ELevelStreamingState PrevState, ELevelStreamingState NewState);

	void InitializeNewWorld(UWorld* InWorld, const FAutomationWorldInitParams& InitParams);
	void InitializeWorldPartition(UWorld* InWorld);
	
	USubsystem* AddAndInitializeSubsystem(FSubsystemCollectionBase* Collection, TSubclassOf<USubsystem> SubsystemClass, UObject* Outer);
	
	void CreateGameInstance(const FAutomationWorldInitParams& InitParams);
	void CreateViewportClient();

	const TArray<UWorldSubsystem*>& GetWorldSubsystems() const;

	/** Cached pointer to a world subsystem collection, retrieved in a fancy way from @World */
	FObjectSubsystemCollection<UWorldSubsystem>* WorldCollection = nullptr;
	/**
	 * Cached pointer to a game subsystem collection, retrieved in a fancy way from @GameInstance.
	 * Can be null if game instance is not created for automation world
	 */
	FObjectSubsystemCollection<UGameInstanceSubsystem>* GameInstanceCollection = nullptr;
	
	UWorld* World = nullptr;
	FWorldContext* WorldContext = nullptr;
	UGameInstance* GameInstance = nullptr;

	/** GWorld value before this automation world was created */
	UWorld* PrevGWorld = nullptr;
	/** GFrameCounter value before this automation world was created */
	uint64 InitialFrameCounter = 0;
	/** Handle to TestEndEvent delegate */
	FDelegateHandle TestCompletedHandle;
	/** Handle to LevelStreamingStateChanged delegate */
	FDelegateHandle StreamingStateHandle;

	/** cached init params for this automation world */
	FWorldInitParams CachedInitParams;
	/** cached game mode, either overriden from init params or extracted from default world settings. If null, means project default game mode */
	TSubclassOf<AGameModeBase> CachedGameMode;

	/** cached tick type, different for game and editor world */
	ELevelTick TickType = LEVELTICK_All;

	/** @return world package with an unique name */
	static UPackage* CreateUniqueWorldPackage(const FString& PackageName, const FString& TestName);

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


