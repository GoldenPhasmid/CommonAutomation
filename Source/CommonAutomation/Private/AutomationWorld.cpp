#include "AutomationWorld.h"

#include "AutomationCommon.h"
#include "AutomationGameInstance.h"
#include "CommonAutomationModule.h"
#include "CommonAutomationSettings.h"
#include "DummyViewport.h"
#include "EngineUtils.h"
#include "GameInstanceAutomationSupport.h"
#include "GameMapsSettings.h"
#include "PackageTools.h"
#include "AI/NavigationSystemBase.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Streaming/LevelStreamingDelegates.h"
#include "Subsystems/LocalPlayerSubsystem.h"

static bool GRunGarbageCollectionForEveryWorld = false;
static FAutoConsoleVariableRef RunGarbageCollectionForEveryWorld(
	TEXT("CommonAutomation.RunGCForEveryWorld"),
	GRunGarbageCollectionForEveryWorld,
	TEXT("If set, garbage collection runs every time automation world is destroyed")
);

template <typename TSubsystemType>
struct FScopeDisableSubsystemCreation
{
	FScopeDisableSubsystemCreation()
		: FScopeDisableSubsystemCreation({})
	{}
	
	FScopeDisableSubsystemCreation(TConstArrayView<UClass*> InEnabledSubsystems)
	{
		DisabledSubsystems = UCommonAutomationSettings::Get()->GetDisabledSubsystems<TSubsystemType>();
		// filter enabled subsystems from default settings
		for (auto ClassIt = DisabledSubsystems.CreateIterator(); ClassIt; ++ClassIt)
		{
			if (InEnabledSubsystems.Contains(*ClassIt))
			{
				ClassIt.RemoveCurrentSwap();
			}
		}

		// apply CLASS_Abstract flag so that subsystems are skipped during subsystem collection initialization
		for (UClass* SubsystemClass: DisabledSubsystems)
		{
			SubsystemClass->ClassFlags |= CLASS_Abstract;
		}
	}

	~FScopeDisableSubsystemCreation()
	{
		// remove applied abstract flag
		for (UClass* SubsystemClass: DisabledSubsystems)
		{
			SubsystemClass->ClassFlags &= ~CLASS_Abstract;
		}
	}

private:
	TArray<UClass*> DisabledSubsystems;
};

template <typename TSubsystemType, typename T>
FObjectSubsystemCollection<TSubsystemType>* GetSubsystemCollection(T* Owner)
{
	static_assert(sizeof(FObjectSubsystemCollection<TSubsystemType>) == sizeof(FSubsystemCollectionBase));
	// @todo: this works on assumption that FSubsystemCollection is the last in object's memory layout
	// we can't detect if it has been changed, although this part of Engine code is stable for the last n revisions
	using TCollectionType = FObjectSubsystemCollection<TSubsystemType>;
	constexpr int32 CollectionOffset = sizeof(T) - sizeof(TCollectionType);
	
	return reinterpret_cast<TCollectionType*>(reinterpret_cast<uint8*>(Owner) + CollectionOffset);
}

const FAutomationWorldInitParams FAutomationWorldInitParams::Minimal{EWorldType::Game, EWorldInitFlags::Minimal};
const FAutomationWorldInitParams FAutomationWorldInitParams::WithBeginPlay{EWorldType::Game, EWorldInitFlags::WithBeginPlay};
const FAutomationWorldInitParams FAutomationWorldInitParams::WithGameInstance{EWorldType::Game, EWorldInitFlags::WithGameInstance};
const FAutomationWorldInitParams FAutomationWorldInitParams::WithLocalPlayer{EWorldType::Game, EWorldInitFlags::WithLocalPlayer};

FAutomationWorldInitParams& FAutomationWorldInitParams::SetWorldPackage(FSoftObjectPath InWorldPath)
{
	UAssetRegistryHelpers::FixupRedirectedAssetPath(InWorldPath);
	WorldPackage = InWorldPath.GetLongPackageName();
	
	return *this;
}

FWorldInitializationValues FAutomationWorldInitParams::CreateWorldInitValues() const
{
	FWorldInitializationValues InitValues{};
	InitValues.InitializeScenes(ShouldInitScene())
			  .AllowAudioPlayback(!!(InitFlags & EWorldInitFlags::InitAudio))
			  .RequiresHitProxies(!!(InitFlags & EWorldInitFlags::InitHitProxy))
			  .CreatePhysicsScene(!!(InitFlags & EWorldInitFlags::InitPhysics))
			  .CreateNavigation(!!(InitFlags & EWorldInitFlags::InitNavigation))
			  .CreateAISystem(!!(InitFlags & EWorldInitFlags::InitAI))
			  .ShouldSimulatePhysics(!!(InitFlags & EWorldInitFlags::InitWeldedBodies))
			  .EnableTraceCollision(!!(InitFlags & EWorldInitFlags::InitCollision))
			  .SetTransactional(false) // @todo: does transactional ever matters for game worlds?
			  .CreateFXSystem(!!(InitFlags & EWorldInitFlags::InitFX))
			  .CreateWorldPartition(ShouldInitWorldPartition())
			  .EnableWorldPartitionStreaming(!(InitFlags & EWorldInitFlags::DisableStreaming));
	if (DefaultGameMode != nullptr)
	{
		// override world settings game mode. If DefaultGM is null and WorldSettings GM null, then project's default GM will be used
		InitValues.SetDefaultGameMode(DefaultGameMode);
	}

	return InitValues;
}

bool FAutomationWorldInitParams::ShouldInitScene() const
{
	constexpr EWorldInitFlags ShouldInitScene = EWorldInitFlags::InitScene | EWorldInitFlags::InitPhysics | EWorldInitFlags::InitWeldedBodies |
												EWorldInitFlags::InitHitProxy | EWorldInitFlags::InitCollision | EWorldInitFlags::InitFX;
	return !!(InitFlags & ShouldInitScene);
}

bool FAutomationWorldInitParams::ShouldInitWorldPartition() const
{
	constexpr EWorldInitFlags ShouldInitWorldPartition = EWorldInitFlags::InitWorldPartition;
	return !!(InitFlags & ShouldInitWorldPartition);
}

FAutomationWorldPtr FAutomationWorldInitParams::Create() const
{
	return FAutomationWorld::CreateWorld(*this);
}

bool FAutomationWorld::bExists = false;
UGameInstance* FAutomationWorld::SharedGameInstance = nullptr;


FAutomationWorld::FAutomationWorld(UWorld* InWorld, const FAutomationWorldInitParams& InitParams)
	: CachedInitParams(InitParams)
{
	bExists = true;
	InitialFrameCounter = GFrameCounter;

	StreamingStateHandle = FLevelStreamingDelegates::OnLevelStreamingStateChanged.AddRaw(this, &FAutomationWorld::HandleLevelStreamingStateChange);

	// create game instance if it was requested by user. Game instance is required for game mode
	// @note: use fallback game instance to create game mode? Don't create game instance if not explicitly specified?
	if (InitParams.CreateGameInstance() || InitParams.DefaultGameMode != nullptr)
	{
		check(InitParams.WorldType == EWorldType::Game);
		CreateGameInstance(InitParams);
	}

	// initialize automation world with new game world
	InitializeNewWorld(InWorld, InitParams);

	// create viewport client if game instance is specified
	if (GameInstance != nullptr && WorldContext != nullptr)
	{
		check(InitParams.WorldType == EWorldType::Game);
		CreateViewportClient();
	}

	// conditionally start play
	if (InitParams.RouteStartPlay())
	{
		RouteStartPlay();
	}

	// conditionally create primary player.
	// @note: create before StartPlay? add an option?
	if (InitParams.CreatePrimaryPlayer())
	{
		GetOrCreatePrimaryPlayer();
	}

	TestCompletedHandle = FAutomationTestFramework::Get().OnTestEndEvent.AddRaw(this, &FAutomationWorld::HandleTestCompleted);
}

void FAutomationWorld::HandleTestCompleted(FAutomationTestBase* Test)
{
	UE_LOG(LogCommonAutomation, Fatal, TEXT("Automation world wasn't destroyed at the end of the test %s"), *Test->GetBeautifiedTestName());
}

void FAutomationWorld::InitializeNewWorld(UWorld* InWorld, const FAutomationWorldInitParams& InitParams)
{
	World = InWorld;
	World->AddToRoot();
	World->SetGameInstance(GameInstance);
	
	// Step 1: swap GWorld to point to a newly created world
	PrevGWorld = GWorld;
	GWorld = World;

	// Step 2: create and initialize world context
	WorldContext = &GEngine->CreateNewWorldContext(InitParams.WorldType);
	WorldContext->SetCurrentWorld(World);
	WorldContext->OwningGameInstance = GameInstance;
	if (GameInstance != nullptr)
	{
		// disable game instance subsystems not required for this automation world
		FScopeDisableSubsystemCreation<UGameInstanceSubsystem> Scope{InitParams.GameSubsystems};
		// notify game instance that it is initialized for automation (primarily to set world context)
		CastChecked<IGameInstanceAutomationSupport>(GameInstance)->InitForAutomation(WorldContext);
	}
	
	// Step 3: initialize world settings
	AWorldSettings* WorldSettings = World->GetWorldSettings();
	// if world package is specified it means world was loaded from existing asset and not created from scratch
	// such worlds can have game mode pre-defined to a specific one, so we don't want to override it, UNLESS DefaultGameMode is nullptr
	if (InitParams.DefaultGameMode != nullptr)
	{
		if ((!InitParams.HasWorldPackage() || WorldSettings->DefaultGameMode == nullptr))
		{
			// override default game mode if the world is created and not loaded
			WorldSettings->DefaultGameMode = InitParams.DefaultGameMode;
		}
		else
		{
			// log an error to fail the test
			// for now it is incorrect to call SetGameMode for a loaded world with already has a set game mode
			UE_LOG(LogCommonAutomation, Error, TEXT("%s: SetGameMode was called for world %s that has a valid game mode"),
				*FAutomationTestFramework::Get().GetCurrentTestFullPath(), *InitParams.GetWorldPackage());	
		}
	}
	const UCommonAutomationSettings* Settings = UCommonAutomationSettings::Get();
	// fixup world settings game mode if we don't want to use heavy project default game mode, for both created and loaded worlds
	if (WorldSettings->DefaultGameMode == nullptr && !Settings->bUseProjectDefaultGameMode)
	{
		WorldSettings->DefaultGameMode = Settings->DefaultGameMode;
	}
	CachedGameMode = WorldSettings->DefaultGameMode;
	
	// Step 4: invoke callbacks that should happen before world is fully initialized
	// @todo: execute delegates after OnWorldInitialized delegate is executed?
	InitParams.InitWorld.ExecuteIfBound(World);
	InitParams.InitWorldSettings.ExecuteIfBound(WorldSettings);
	
	// Step 5: finish world initialization (see FScopedEditorWorld::Init)
	World->WorldType = InitParams.WorldType;
	// tick viewports only in editor worlds
	TickType = InitParams.WorldType == EWorldType::Game ? LEVELTICK_All : LEVELTICK_ViewportsOnly;

	{
		// hack: world partition requires PIE world type to initialize properly for game worlds
		TGuardValue Guard{World->WorldType, World->IsGameWorld() ? EWorldType::PIE : World->WorldType.GetValue()};

		// disable world subsystems not required for this automation world
		FScopeDisableSubsystemCreation<UWorldSubsystem> Scope{InitParams.WorldSubsystems};
		World->InitWorld(InitParams.CreateWorldInitValues());
		
		WorldCollection = GetSubsystemCollection<UWorldSubsystem>(World);
	}
	
	if (GameInstance != nullptr)
	{
		World->SetGameMode({});
	}
	
	World->PersistentLevel->UpdateModelComponents();
	// Register components in the persistent level (current)
	World->UpdateWorldComponents(true, false);
	// Make sure secondary levels are loaded & visible.
	World->FlushLevelStreaming();

	if (IsEditorWorld() && !!(CachedInitParams.InitFlags & EWorldInitFlags::InitNavigation))
	{
		// initialize navigation system for editor worlds
		FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::EditorMode);
	}
}

void FAutomationWorld::CreateGameInstance(const FAutomationWorldInitParams& InitParams)
{
	if (SharedGameInstance == nullptr)
	{
		// @todo: add ability to override game instance class
		const UClass* GameInstanceClass = GetDefault<UGameMapsSettings>()->GameInstanceClass.TryLoadClass<UGameInstance>();
		if (GameInstanceClass == nullptr || !GameInstanceClass->ImplementsInterface(UGameInstanceAutomationSupport::StaticClass()))
		{
			// If an invalid or unsupported class type is specified we fall back to the default.
			GameInstanceClass = UAutomationGameInstance::StaticClass();
		}

#if REUSE_GAME_INSTANCE
		const FString GameInstanceName{TEXT("AutomationWorld_SharedGameInstance")};
#else
		static uint32 GameInstanceCounter = 0;
		const FString GameInstanceName = FString::Printf(TEXT("AutomationWorld_GameInstance_%d"), GameInstanceCounter++);
#endif
		
		// create game instance, either in shared or unique mode
		SharedGameInstance = NewObject<UGameInstance>(GEngine, GameInstanceClass, FName{GameInstanceName}, RF_Transient);
#if REUSE_GAME_INSTANCE
		// add to root so game instance lives between automation worlds
		SharedGameInstance->AddToRoot();
#endif
	}

	GameInstance = SharedGameInstance;
	GameInstanceCollection = GetSubsystemCollection<UGameInstanceSubsystem>(GameInstance);
	check(GameInstance);
}

void FAutomationWorld::CreateViewportClient()
{
	check(WorldContext && GameInstance);
	// create game viewport client to avoid ensures
	UGameViewportClient* NewViewport = NewObject<UGameViewportClient>(GameInstance->GetEngine());

	{
		// this is a stupid way to block UGameViewportClient from creating a new audio device because bCreateAudioDevice is not used
		TGuardValue<UEngine*> _{GEngine, nullptr};

		constexpr bool bCreateAudioDevice = false;
		NewViewport->Init(*WorldContext, GameInstance, bCreateAudioDevice);
	}
	
	// Set the overlay widget, to avoid an ensure
	TSharedRef<SOverlay> DudOverlay = SNew(SOverlay);

	NewViewport->SetViewportOverlayWidget(nullptr, DudOverlay);

	// Set the internal FViewport, for the new game viewport, to avoid another bit of auto-exit code
	NewViewport->Viewport = new FDummyViewport(NewViewport);
	
	// Set the world context game viewport, to match the newly created viewport, in order to prevent crashes
	WorldContext->GameViewport = NewViewport;
}

const TArray<UWorldSubsystem*>& FAutomationWorld::GetWorldSubsystems() const
{
	return WorldCollection->GetSubsystemArray<UWorldSubsystem>(UWorldSubsystem::StaticClass());
}

UPackage* FAutomationWorld::CreateUniqueWorldPackage(const FString& PackageName)
{
	static uint32 PackageNameCounter = 0;
	// create a unique temporary package for world loaded from existing package on disk. Add /Temp/ prefix to avoid "package always doesn't exist" warning
	const FName UniquePackageName = *FString::Printf(TEXT("/Temp/%s_%d"), *PackageName, PackageNameCounter++);
		
	UPackage* WorldPackage = NewObject<UPackage>(nullptr, UniquePackageName, RF_Transient);
	// mark as map package
	WorldPackage->ThisContainsMap();
	// add PlayInEditor flag to disable dirtying world package
	WorldPackage->SetPackageFlags(PKG_PlayInEditor);
	// mark package as transient to avoid it being processed as an asset
	WorldPackage->SetFlags(RF_Transient);

	return WorldPackage;

}

void FAutomationWorld::HandleLevelStreamingStateChange(UWorld* OtherWorld, const ULevelStreaming* LevelStreaming, ULevel* LevelIfLoaded,  ELevelStreamingState PrevState, ELevelStreamingState NewState)
{
	if (World != OtherWorld)
	{
		return;
	}
	
	if (LevelIfLoaded)
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			// Clear RF_Standalone flag that keeps sublevel worlds from being GC'd in editor
			UWorld* LevelOuterWorld = LevelIfLoaded->GetTypedOuter<UWorld>();
			// sanity check that sublevel world is not a main world
			check(LevelOuterWorld != World);
			LevelOuterWorld->ClearFlags(GARBAGE_COLLECTION_KEEPFLAGS);
		}
#endif
	}
}

FAutomationWorld::~FAutomationWorld()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FAutomationWorld_DestroyWorld);
	
	check(IsValid(World));
	FLevelStreamingDelegates::OnLevelStreamingStateChanged.Remove(StreamingStateHandle);
	// remove test completion handle
	FAutomationTestFramework::Get().OnTestEndEvent.Remove(TestCompletedHandle);
	
	if (World->GetBegunPlay())
	{
		RouteEndPlay();
	}

	// shutdown game instance
	if (GameInstance != nullptr)
	{
		GameInstance->Shutdown();
	}

	UPackage* WorldPackage = World->GetPackage();
	
	// destroy world and world context
	GEngine->ShutdownWorldNetDriver(World);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);

	// null pointers to subsystem collections
	WorldCollection = nullptr;
	GameInstanceCollection = nullptr;

	World = nullptr;
	WorldContext = nullptr;
	GameInstance = nullptr;
#if !REUSE_GAME_INSTANCE
	if (SharedGameInstance != nullptr)
	{
		SharedGameInstance->RemoveFromRoot();
		SharedGameInstance = nullptr;
	}
#endif

	// restore globals and garbage collect the world
	GFrameCounter = InitialFrameCounter;
	GWorld = PrevGWorld;
	
	FCommonAutomationModule::RequestGC();
	if (GRunGarbageCollectionForEveryWorld)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FAutomationWorld_RunGC);

		constexpr bool bFullPurge = true;
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, bFullPurge);
	}
	
	bExists = false;
}

FAutomationWorldPtr FAutomationWorld::CreateWorld(const FAutomationWorldInitParams& InitParams)
{
	if (Exists())
	{
		UE_LOG(LogCommonAutomation, Fatal, TEXT("%s: Tring to create second automation world"), *FString(__FUNCTION__));
		return nullptr;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FAutomationWorld_CreateWorld);

	FAutomationTestBase* Test = FAutomationTestFramework::Get().GetCurrentTest();
	check(Test);
	
	const FString CurrentTestName = FAutomationTestFramework::Get().GetCurrentTest()->GetBeautifiedTestName();
	
	UWorld* NewWorld = nullptr;
	// load game world flow
	if (InitParams.HasWorldPackage())
	{
		const FString WorldPackageToLoad = InitParams.GetWorldPackage();
		if (WorldPackageToLoad.IsEmpty())
		{
			// world package name is set by empty
			UE_LOG(LogCommonAutomation, Error, TEXT("%s: Specified package name %s is empty"), *FString(__FUNCTION__), *WorldPackageToLoad);
			return nullptr;
		}
		
		if (!FPackageName::IsValidLongPackageName(WorldPackageToLoad))
		{
			// world package name is ill formed
			UE_LOG(LogCommonAutomation, Error, TEXT("%s: Specified package name %s is not a valid long package name"), *FString(__FUNCTION__), *WorldPackageToLoad);
			return nullptr;
		}

		FPackagePath PackagePath = FPackagePath::FromPackageNameChecked(WorldPackageToLoad);
		if (!FPackageName::DoesPackageExist(PackagePath, &PackagePath))
		{
			// world package doesn't exist on disk
			UE_LOG(LogCommonAutomation, Error, TEXT("%s: Specified package name %s doesn't exist on disk"), *FString(__FUNCTION__), *WorldPackageToLoad);
			return nullptr;
		}
		
		UPackage* WorldPackage = CreateUniqueWorldPackage(FString::Printf(TEXT("%s_%s"), *CurrentTestName, *WorldPackageToLoad));

		const FName WorldPackageName{WorldPackageToLoad};
		UWorld::WorldTypePreLoadMap.FindOrAdd(WorldPackageName) = InitParams.WorldType;

		// load world package as a temporary package with a different name
		WorldPackage  = LoadPackage(WorldPackage, PackagePath, InitParams.LoadFlags);
		
		UWorld::WorldTypePreLoadMap.Remove(WorldPackageName);

		if (WorldPackage == nullptr)
		{
			// failed to load world package for some reason
			UE_LOG(LogCommonAutomation, Error, TEXT("%s: Failed to load package %s"), *FString(__FUNCTION__), *WorldPackageToLoad);
			return nullptr;
		}
		
		NewWorld = UWorld::FindWorldInPackage(WorldPackage);
		if (NewWorld == nullptr)
		{
			NewWorld = UWorld::FollowWorldRedirectorInPackage(WorldPackage);
		}
		check(NewWorld);
	}
	
	if (NewWorld == nullptr)
	{
		// create unique package for an empty world
		UPackage* WorldPackage = CreateUniqueWorldPackage(*CurrentTestName);

		// create an empty world
		FWorldInitializationValues InitValues = InitParams.CreateWorldInitValues();
		NewWorld = UWorld::CreateWorld(InitParams.WorldType, false, TEXT("AutomationWorld"), WorldPackage, true, ERHIFeatureLevel::Num, &InitValues, true);
	}

	if (NewWorld == nullptr)
	{
		UE_LOG(LogCommonAutomation, Error, TEXT("%s: Failed to create world for automation."), *FString(__FUNCTION__));
		return nullptr;
	}

	FAutomationWorldPtr AutomationWorld = MakeShareable(new FAutomationWorld(NewWorld, InitParams));
	
	return AutomationWorld;
}

FAutomationWorldPtr FAutomationWorld::CreateGameWorld(EWorldInitFlags InitFlags)
{
	const FAutomationWorldInitParams InitParams{EWorldType::Game, InitFlags};
	return CreateWorld(InitParams);
}

FAutomationWorldPtr FAutomationWorld::CreateGameWorldWithGameInstance(TSubclassOf<AGameModeBase> DefaultGameMode, EWorldInitFlags InitFlags)
{
	return CreateWorld(FAutomationWorldInitParams{EWorldType::Game, EWorldInitFlags::WithGameInstance | InitFlags}.SetGameMode(DefaultGameMode));
}

FAutomationWorldPtr FAutomationWorld::CreateGameWorldWithPlayer(TSubclassOf<AGameModeBase> DefaultGameMode, EWorldInitFlags InitFlags)
{
	return CreateWorld(FAutomationWorldInitParams{EWorldType::Game, EWorldInitFlags::WithLocalPlayer | InitFlags}.SetGameMode(DefaultGameMode));
}

FAutomationWorldPtr FAutomationWorld::CreateEditorWorld(EWorldInitFlags InitFlags)
{
	const FAutomationWorldInitParams InitParams{EWorldType::Editor, InitFlags};
	return CreateWorld(InitParams);
}

FAutomationWorldPtr FAutomationWorld::LoadGameWorld(const FString& WorldPackage, EWorldInitFlags InitFlags)
{
	if (WorldPackage.IsEmpty())
	{
		return nullptr;
	}
	
	return CreateWorld(FAutomationWorldInitParams{EWorldType::Game, InitFlags}.SetWorldPackage(WorldPackage));
}

FAutomationWorldPtr FAutomationWorld::LoadGameWorld(const FSoftObjectPath& WorldPath, EWorldInitFlags InitFlags)
{
	if (WorldPath.IsNull())
	{
		return nullptr;
	}
	
	return CreateWorld(FAutomationWorldInitParams{EWorldType::Game, InitFlags}.SetWorldPackage(WorldPath));
}

FAutomationWorldPtr FAutomationWorld::LoadEditorWorld(const FString& WorldPackage, EWorldInitFlags InitFlags)
{
	FAutomationWorldInitParams InitParams{EWorldType::Editor, InitFlags};
	InitParams.WorldPackage = WorldPackage;

	return CreateWorld(InitParams);
}

FAutomationWorldPtr FAutomationWorld::LoadEditorWorld(const FSoftObjectPath& WorldPath, EWorldInitFlags InitFlags)
{
	return CreateWorld(FAutomationWorldInitParams{EWorldType::Editor, InitFlags}.SetWorldPackage(WorldPath));
}

bool FAutomationWorld::Exists()
{
	return bExists;
}

UGameInstanceSubsystem* FAutomationWorld::GetOrCreateSubsystem(TSubclassOf<UGameInstanceSubsystem> SubsystemClass)
{
	check(World && World->bIsWorldInitialized);
	check(GameInstanceCollection);
	
	if (GameInstance == nullptr || SubsystemClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}
	
	UGameInstanceSubsystem* Subsystem = GameInstance->GetSubsystemBase(SubsystemClass);
	if (Subsystem == nullptr)
	{
		Subsystem = GameInstanceCollection->GetSubsystem(SubsystemClass);
	}

	if (Subsystem == nullptr)
	{
		// create subsystem only if it should be created
		if (const UGameInstanceSubsystem* CDO = GetDefault<UGameInstanceSubsystem>(SubsystemClass); CDO->ShouldCreateSubsystem(GameInstance))
		{
			Subsystem = CastChecked<UGameInstanceSubsystem>(AddAndInitializeSubsystem(GameInstanceCollection, SubsystemClass, GameInstance));
		}
	}

	return Subsystem;
}

UWorldSubsystem* FAutomationWorld::GetOrCreateSubsystem(TSubclassOf<UWorldSubsystem> SubsystemClass)
{
	check(World && World->bIsWorldInitialized);
	check(WorldCollection);

	if (SubsystemClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}
	
	UWorldSubsystem* Subsystem = World->GetSubsystemBase(SubsystemClass);
	if (Subsystem == nullptr)
	{
		Subsystem = WorldCollection->GetSubsystem(SubsystemClass);
	}
	
	if (Subsystem == nullptr)
	{
		// create subsystem only if it should be created for a given world
		if (const UWorldSubsystem* CDO = GetDefault<UWorldSubsystem>(SubsystemClass); CDO->ShouldCreateSubsystem(World))
		{
			Subsystem = CastChecked<UWorldSubsystem>(AddAndInitializeSubsystem(WorldCollection, SubsystemClass, World));
			
			Subsystem->PostInitialize();
			Subsystem->OnWorldComponentsUpdated(*World);
			if (World->HasBegunPlay())
			{
				Subsystem->OnWorldBeginPlay(*World);
			}
		}
	}
	
	return Subsystem;
}

USubsystem* FAutomationWorld::AddAndInitializeSubsystem(FSubsystemCollectionBase* Collection, TSubclassOf<USubsystem> SubsystemClass, UObject* Outer)
{
	// This relies on FSubsystemCollectionBase having following memory alignment:
	// vpointer
	// FSubsystemMap SubsystemMap
	// FSubsystemArrayMap SubsystemArrayMap
	// Other members..
	// If it crashes, update implementation accordingly
	
	using FSubsystemMap = TMap<TObjectPtr<UClass>, TObjectPtr<USubsystem>>;
	using FSubsystemArrayMap =TMap<UClass*, TArray<USubsystem*>>;

	constexpr int32 SubsystemMapOffset = sizeof(void*); // vpointer offset
	constexpr int32 SubsystemArrayOffset = sizeof(FSubsystemMap) + SubsystemMapOffset;
	
	USubsystem* Subsystem = NewObject<USubsystem>(Outer, SubsystemClass);
	
	// This is a direct implementation from AddAndInitializeSubsystem
	// add subsystem to the collection's subsystem map
	FSubsystemMap* SubsystemMap = (FSubsystemMap*)(reinterpret_cast<uint8*>(Collection) + SubsystemMapOffset);
	FSubsystemArrayMap* SubsystemArrayMap = (FSubsystemArrayMap*)(reinterpret_cast<uint8*>(Collection) + SubsystemArrayOffset);
	
	SubsystemMap->Add(SubsystemClass, Subsystem);

	// initialize subsystem
	Subsystem->Initialize(*Collection);
	
	// Add this new subsystem to any existing maps of base classes to lists of subsystems
	for (TPair<UClass*, TArray<USubsystem*>>& Pair : *SubsystemArrayMap)
	{
		if (SubsystemClass->IsChildOf(Pair.Key))
		{
			Pair.Value.Add(Subsystem);
		}
	}

	return Subsystem;
}

ULocalPlayer* FAutomationWorld::GetOrCreatePrimaryPlayer(bool bSpawnPlayerController)
{
	check(World && WorldContext);
	if (UNLIKELY(World->WorldType == EWorldType::Editor))
	{
		return nullptr;
	}
	
	if (ULocalPlayer* PrimaryPlayer = GEngine->GetFirstGamePlayer(World))
	{
		return PrimaryPlayer;
	}

	return CreateLocalPlayer(bSpawnPlayerController);
}

ULocalPlayer* FAutomationWorld::CreateLocalPlayer(bool bSpawnPlayerController)
{
	check(World && WorldContext);
	if (UNLIKELY(World->WorldType == EWorldType::Editor))
	{
		return nullptr;
	}
	
	if (GameInstance != nullptr)
	{
		// GameInstance, GameMode and GameSession are required to create a LocalPlayer/PlayerController pair
		if (auto GameMode = World->GetAuthGameMode(); GameMode != nullptr && GameMode->GameSession != nullptr)
		{
			// @todo: check World->bBegunPlay as well?

			const TArray<ULocalPlayer*>& LocalPlayers = GEngine->GetGamePlayers(World);
			
			FString Error{};
			FScopeDisableSubsystemCreation<ULocalPlayerSubsystem> Scope{CachedInitParams.PlayerSubsystems};
			ULocalPlayer* LocalPlayer = GameInstance->CreateLocalPlayer(LocalPlayers.Num(), Error, bSpawnPlayerController);

			checkf(Error.IsEmpty(), TEXT("%s"), *Error);

			return LocalPlayer;
		}
	}
	
	return nullptr;
}

void FAutomationWorld::DestroyLocalPlayer(ULocalPlayer* LocalPlayer)
{
	if (UNLIKELY(World->WorldType == EWorldType::Editor))
	{
		return;
	}
	
	const TArray<ULocalPlayer*>& LocalPlayers = GEngine->GetGamePlayers(World);
	if (LocalPlayers.Contains(LocalPlayer))
	{
		World->GetGameInstance()->RemoveLocalPlayer(LocalPlayer);
	}
}

void FAutomationWorld::RouteStartPlay() const
{
	check(World && World->bIsWorldInitialized);
	if (UNLIKELY(IsEditorWorld()))
	{
		return;
	}
	
	if (World->GetBegunPlay())
	{
		return;
	}
	
	FURL URL{};
	World->InitializeActorsForPlay(URL);
	
	if (!!(CachedInitParams.InitFlags & EWorldInitFlags::InitNavigation))
	{
		// initialize navigation system for game worlds if requested
		FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::GameMode);
	}

	// call OnWorldBeginPlay for world subsystems and StartPlay for GameMode
	World->BeginPlay();
	
	if (World->GetAuthGameMode() == nullptr)
	{
		// call BeginPlay for actors
		TActorIterator<AWorldSettings> WorldSettings(World);
		WorldSettings->NotifyBeginPlay();
	}
	
	check(World->GetBegunPlay());
}

void FAutomationWorld::RouteEndPlay() const
{
	check(World && World->bIsWorldInitialized);
	if (UNLIKELY(World->WorldType == EWorldType::Editor))
	{
		return;
	}
	
	if (!World->GetBegunPlay())
	{
		return;
	}
	
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		// don't know which EEndPlayReason to select. Let's assume it mimics PIE
		It->RouteEndPlay(EEndPlayReason::EndPlayInEditor);
	}

	World->SetBegunPlay(false);
}

void FAutomationWorld::TickWorld(int32 NumFrames)
{
	constexpr float DeltaTime = 1.0 / 60.0;
	while (NumFrames > 0)
	{
		World->Tick(TickType, DeltaTime);

		if (IsEditorWorld())
		{
			// Tick any editor FTickableEditorObject derived classes
			FTickableEditorObject::TickObjects(DeltaTime);
		}
		else
		{
			// tick streamable manager and other game tickable objects without world
			// world-related tickable objects are processed during world tick
			FTickableGameObject::TickObjects(nullptr, LEVELTICK_All, false, DeltaTime);
		}
		
		// update level streaming, as we're not drawing viewport which usually updates it
		World->UpdateLevelStreaming();

		// tick for FAsyncMixin
		FTSTicker::GetCoreTicker().Tick(DeltaTime);
		++GFrameCounter;
		--NumFrames;
	}
}

void FAutomationWorld::AbsoluteWorldTravel(TSoftObjectPtr<UWorld> WorldToTravel, TSubclassOf<AGameModeBase> GameModeClass, FString TravelOptions)
{
	check(World && World->bIsWorldInitialized);
	if (IsEditorWorld())
	{
		// no travel for editor worlds
		return;
	}

	if (GameModeClass != nullptr)
	{
		TravelOptions += TEXT("GAME=") + FSoftClassPath{GameModeClass}.ToString();
	}
	
	UGameplayStatics::OpenLevelBySoftObjectPtr(World, WorldToTravel, true, TravelOptions);
	
	FinishWorldTravel();
}

void FAutomationWorld::FinishWorldTravel()
{
	check(World && World->bIsWorldInitialized);

	if (IsEditorWorld())
	{
		// no travel for editor worlds
		return;
	}

	if (!World->HasBegunPlay())
    {
    	// can't travel from the world that hasn't begun play
    	RouteStartPlay();
    }
	
	{
		// hack: world partition requires PIE world type to initialize properly for game worlds
		TGuardValue Guard{World->WorldType, World->IsGameWorld() ? EWorldType::PIE : World->WorldType.GetValue()};

		// disable world subsystems not required for this automation world
		FScopeDisableSubsystemCreation<UWorldSubsystem> Scope{CachedInitParams.WorldSubsystems};
		GEngine->TickWorldTravel(*WorldContext, World->NextSwitchCountdown);
	}
	
	// set new world from a world context
	World = WorldContext->World();
	check(World && World->bIsWorldInitialized);

	// update world collection pointer
	WorldCollection = GetSubsystemCollection<UWorldSubsystem>(World);
}

UWorld* FAutomationWorld::GetWorld() const
{
	return World;
}

FWorldContext* FAutomationWorld::GetWorldContext() const
{
	return WorldContext;
}

UGameInstance* FAutomationWorld::GetGameInstance() const
{
	return GameInstance;
}
