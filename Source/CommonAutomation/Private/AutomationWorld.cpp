#include "AutomationWorld.h"

#include "AutomationGameInstance.h"
#include "DummyViewport.h"
#include "EngineUtils.h"
#include "GameInstanceAutomationSupport.h"
#include "GameMapsSettings.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameModeBase.h"
#include "Streaming/LevelStreamingDelegates.h"

DEFINE_LOG_CATEGORY_STATIC(LogAutomationWorld, Log, Log);

#define ALLOW_GAME_INSTANCE_REUSE 0

bool FAutomationWorld::bExists = false;
UGameInstance* FAutomationWorld::SharedGameInstance = nullptr;

FAutomationWorldInitParams FAutomationWorldInitParams::Minimal{EWorldType::Game, EWorldInitFlags::Minimal};
FAutomationWorldInitParams FAutomationWorldInitParams::WithGameInstance{EWorldType::Game, EWorldInitFlags::WithGameInstance};
FAutomationWorldInitParams FAutomationWorldInitParams::WithLocalPlayer{EWorldType::Game, EWorldInitFlags::WithLocalPlayer};

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
			  .CreateWorldPartition(!!(InitFlags & EWorldInitFlags::InitWorldPartition));
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

FAutomationWorld::FAutomationWorld(UWorld* InWorld, const FAutomationWorldInitParams& InitParams)
{
	bExists = true;
	InitialFrameCounter = GFrameCounter;

	FLevelStreamingDelegates::OnLevelStreamingStateChanged.AddRaw(this, &FAutomationWorld::HandleLevelStreamingStateChange);

	// create game instance if it was requested by user. Game instance is required for game mode
	// @note: use fallback game instance to create game mode? Don't create game instance if not explicitly specified?
	if (InitParams.CreateGameInstance() || InitParams.DefaultGameMode != nullptr)
	{
		CreateGameInstance();
	}

	// initialize automation world with new game world
	InitializeNewWorld(InWorld, InitParams);

	// create viewport client if game instance is specified
	if (GameInstance != nullptr && WorldContext != nullptr)
	{
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
}

void FAutomationWorld::InitializeNewWorld(UWorld* InWorld, const FAutomationWorldInitParams& InitParams)
{
	World = InWorld;
	World->SetGameInstance(GameInstance);
	
	// Step 1: swap GWorld to point to a newly created world
	PrevGWorld = GWorld;
	GWorld = World;

	// Step 2: create and initialize world context
	WorldContext = &GEngine->CreateNewWorldContext(InitParams.WorldType);
	WorldContext->SetCurrentWorld(World);
	WorldContext->OwningGameInstance = GameInstance;
	if (GameInstance)
	{
		// notify game instance that it is initialized for automation (primarily to set world context)
		CastChecked<IGameInstanceAutomationSupport>(GameInstance)->InitForAutomation(WorldContext);
	}
	
	// Step 3: initialize world settings
	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (InitParams.WorldPackage.IsEmpty() && InitParams.DefaultGameMode)
	{
		// override default game mode if the world is created and not loaded
		WorldSettings->DefaultGameMode = InitParams.DefaultGameMode;
	}
	
	// Step 4: invoke callbacks that should happen before world is fully initialized
	InitParams.InitWorld.ExecuteIfBound(World);
	InitParams.InitWorldSettings.ExecuteIfBound(WorldSettings);
	
	// Step 5: finish world initialization (see FScopedEditorWorld::Init)
	World->InitWorld(InitParams.CreateWorldInitValues());
	if (GameInstance != nullptr)
	{
		World->SetGameMode({});
	}
	World->PersistentLevel->UpdateModelComponents();
	World->UpdateWorldComponents(true, false);
	World->UpdateLevelStreaming();
}

void FAutomationWorld::CreateGameInstance()
{
	if (SharedGameInstance == nullptr)
	{
		const UClass* GameInstanceClass = GetDefault<UGameMapsSettings>()->GameInstanceClass.TryLoadClass<UGameInstance>();
		if (GameInstanceClass == nullptr || !GameInstanceClass->ImplementsInterface(UGameInstanceAutomationSupport::StaticClass()))
		{
			// If an invalid or unsupported class type is specified we fall back to the default.
			GameInstanceClass = UAutomationGameInstance::StaticClass();
		}

		// create shared game instance
		SharedGameInstance = NewObject<UGameInstance>(GEngine, GameInstanceClass, TEXT("SharedGameInstance"), RF_Transient);
#if ALLOW_GAME_INSTANCE_REUSE
		// add to root so game instance lives between automation worlds
		SharedGameInstance->AddToRoot();
#endif
	}

	GameInstance = SharedGameInstance;
	check(GameInstance);
}

void FAutomationWorld::CreateViewportClient()
{
	check(WorldContext && GameInstance);
	// create game viewport client to avoid ensures
	UGameViewportClient* NewViewport = NewObject<UGameViewportClient>(GameInstance->GetEngine());
	NewViewport->Init(*WorldContext, GameInstance, false);

	// Set the overlay widget, to avoid an ensure
	TSharedRef<SOverlay> DudOverlay = SNew(SOverlay);

	NewViewport->SetViewportOverlayWidget(nullptr, DudOverlay);

	// Set the internal FViewport, for the new game viewport, to avoid another bit of auto-exit code
	NewViewport->Viewport = new FDummyViewport(NewViewport);
	
	// Set the world context game viewport, to match the newly created viewport, in order to prevent crashes
	WorldContext->GameViewport = NewViewport;
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
			// Clear RF_Standalone flag that keeps tile worlds from being GC'd in editor
			UWorld* LevelOuterWorld = LevelIfLoaded->GetTypedOuter<UWorld>();
			// sanity check that tile world is not our main world
			check(LevelOuterWorld != World);
			LevelOuterWorld->ClearFlags(GARBAGE_COLLECTION_KEEPFLAGS);
		}
#endif
	}
}

FAutomationWorld::~FAutomationWorld()
{
	check(IsValid(World));
	FLevelStreamingDelegates::OnLevelStreamingStateChanged.RemoveAll(this);
	
	if (World->bBegunPlay)
	{
		RouteEndPlay();
	}

	if (GameInstance != nullptr)
	{
		GameInstance->Shutdown();
	}
	
	GEngine->ShutdownWorldNetDriver(World);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);

	World = nullptr;
	WorldContext = nullptr;
	GameInstance = nullptr;
#if !ALLOW_GAME_INSTANCE_REUSE
	if (SharedGameInstance != nullptr)
	{
		SharedGameInstance->RemoveFromRoot();
		SharedGameInstance = nullptr;
	}
#endif

	GEngine->ForceGarbageCollection();
	GFrameCounter = InitialFrameCounter;
	GWorld = PrevGWorld;

	WorldSubsystems.Empty();
	GameInstanceSubsystems.Empty();
	
	bExists = false;
}

FAutomationWorldPtr FAutomationWorld::CreateWorld(const FAutomationWorldInitParams& InitParams)
{
	if (Exists())
	{
		UE_LOG(LogAutomationWorld, Fatal, TEXT("%s: Tring to create second automation world"), *FString(__FUNCTION__));
		return nullptr;
	}

	UWorld* NewWorld = nullptr;
	if (InitParams.HasWorldPackage())
	{
		if (!FPackageName::IsValidLongPackageName(InitParams.WorldPackage))
		{
			// world package name is ill formed
			UE_LOG(LogAutomationWorld, Error, TEXT("%s: Specified package name %s is not a valid long package name"), *FString(__FUNCTION__), *InitParams.WorldPackage);
			return nullptr;
		}
		
		if (!FPackageName::DoesPackageExist(InitParams.WorldPackage))
		{
			// world package doesn't exist on disk
			UE_LOG(LogAutomationWorld, Error, TEXT("%s: Specified package name %s doesn't exist on disk"), *FString(__FUNCTION__), *InitParams.WorldPackage);
			return nullptr;
		}

		FName WorldPackageName{InitParams.WorldPackage};
		UWorld::WorldTypePreLoadMap.FindOrAdd(WorldPackageName) = InitParams.WorldType;
		
		// const uint32 LoadFlags = InitParams.LoadFlags & (InitParams.WorldPackage ? LOAD_PackageForPIE : LOAD_None);
		const uint32 LoadFlags = LOAD_None;
		UPackage* WorldPackage = LoadPackage(nullptr, *InitParams.WorldPackage, LoadFlags);

		UWorld::WorldTypePreLoadMap.Remove(WorldPackageName);

		if (WorldPackage == nullptr)
		{
			UE_LOG(LogAutomationWorld, Error, TEXT("%s: Failed to load package %s"), *FString(__FUNCTION__), *InitParams.WorldPackage);
		}
		
		NewWorld = UWorld::FindWorldInPackage(WorldPackage);
	}
	
	bool bWithGameInstance = !!(InitParams.InitFlags & EWorldInitFlags::CreateGameInstance);
	if (NewWorld == nullptr)
	{
		UPackage* WorldPackage = nullptr;
		if (bWithGameInstance)
		{
			static uint32 PackageNameCounter = 0;
			// create unique package name for an empty world. Add /Temp/ prefix to avoid "package always doesn't exist" warning
			const FName PackageName = *FString::Printf(TEXT("/Temp/%s%d"), TEXT("AutomationWorld"), PackageNameCounter++);
			/** */
			WorldPackage = NewObject<UPackage>(nullptr, PackageName, RF_Transient);
			// mark as map package
			WorldPackage->ThisContainsMap();
			// add PlayInEditor flag to disable dirtying world package
			WorldPackage->SetPackageFlags(PKG_PlayInEditor);
		}

		FWorldInitializationValues InitValues = InitParams.CreateWorldInitValues();
		NewWorld = UWorld::CreateWorld(InitParams.WorldType, false, NAME_None, WorldPackage, true, ERHIFeatureLevel::Num, &InitValues, true);
	}

	if (NewWorld == nullptr)
	{
		UE_LOG(LogAutomationWorld, Error, TEXT("%s: Failed to create world for automation."), *FString(__FUNCTION__));
		return nullptr;
	}

	FAutomationWorldPtr AutomationWorld = MakeShareable(new FAutomationWorld(NewWorld, InitParams));
	
	return AutomationWorld;
}

FAutomationWorldPtr FAutomationWorld::CreateGameWorld(EWorldInitFlags InitFlags)
{
	FAutomationWorldInitParams InitParams{EWorldType::Game, InitFlags};
	return CreateWorld(InitParams);
}

FAutomationWorldPtr FAutomationWorld::CreateGameWorldWithGameInstance(TSubclassOf<AGameModeBase> DefaultGameMode, EWorldInitFlags InitFlags)
{
	FAutomationWorldInitParams InitParams{EWorldType::Game, EWorldInitFlags::WithGameInstance | InitFlags, DefaultGameMode};
	return CreateWorld(InitParams);
}

FAutomationWorldPtr FAutomationWorld::CreateGameWorldWithPlayer(TSubclassOf<AGameModeBase> DefaultGameMode, EWorldInitFlags InitFlags)
{
	FAutomationWorldInitParams InitParams{EWorldType::Game, EWorldInitFlags::WithLocalPlayer | InitFlags, DefaultGameMode};
	return CreateWorld(InitParams);
}

bool FAutomationWorld::Exists()
{
	return bExists;
}

UGameInstanceSubsystem* FAutomationWorld::CreateSubsystem(TSubclassOf<UGameInstanceSubsystem> SubsystemClass)
{
	static FSubsystemCollection<UGameInstanceSubsystem> DummyCollection;
	
	UGameInstance* Outer = World->GetGameInstance();
	UGameInstanceSubsystem* Subsystem = NewObject<UGameInstanceSubsystem>(Outer, SubsystemClass);
	Subsystem->Initialize(DummyCollection);

	GameInstanceSubsystems.Add(Subsystem);

	return Subsystem;
}

UWorldSubsystem* FAutomationWorld::CreateSubsystem(TSubclassOf<UWorldSubsystem> SubsystemClass)
{
	static FSubsystemCollection<UWorldSubsystem> DummyCollection;

	UWorld* Outer = World;
	UWorldSubsystem* Subsystem = NewObject<UWorldSubsystem>(Outer, SubsystemClass);
	Subsystem->Initialize(DummyCollection);
	Subsystem->PostInitialize();
	Subsystem->OnWorldComponentsUpdated(*World);

	WorldSubsystems.Add(Subsystem);

	return Subsystem;
}

ULocalPlayer* FAutomationWorld::GetOrCreatePrimaryPlayer()
{
	check(World && WorldContext);
	if (ULocalPlayer* PrimaryPlayer = GEngine->GetFirstGamePlayer(World))
	{
		return PrimaryPlayer;
	}

	return CreateLocalPlayer();
}

ULocalPlayer* FAutomationWorld::CreateLocalPlayer()
{
	check(World && WorldContext);
	
	if (GameInstance != nullptr)
	{
		// GameInstance, GameMode and GameSession are required to create a LocalPlayer/PlayerController pair
		if (auto GameMode = World->GetAuthGameMode(); GameMode != nullptr && GameMode->GameSession != nullptr)
		{
			// @todo: check World->bBegunPlay as well?

			const TArray<ULocalPlayer*>& LocalPlayers = GEngine->GetGamePlayers(World);
	
			FString Error;
			ULocalPlayer* LocalPlayer = GameInstance->CreateLocalPlayer(LocalPlayers.Num(), Error, true);

			checkf(Error.IsEmpty(), TEXT("%s"), *Error);

			return LocalPlayer;
		}
	}
	
	return nullptr;
}

void FAutomationWorld::DestroyLocalPlayer(ULocalPlayer* LocalPlayer)
{
	const TArray<ULocalPlayer*>& LocalPlayers = GEngine->GetGamePlayers(World);
	if (LocalPlayers.Contains(LocalPlayer))
	{
		World->GetGameInstance()->RemoveLocalPlayer(LocalPlayer);
	}
}

void FAutomationWorld::RouteStartPlay() const
{
	check(World && World->bIsWorldInitialized);
	if (World->bBegunPlay)
	{
		return;
	}
	
	FURL URL{};
	World->InitializeActorsForPlay(URL);

	// call OnWorldBeginPlay for world subsystems and StartPlay for GameMode
	World->BeginPlay();

	// call OnWorldBeginPlay for additional world subsystems
	for (UWorldSubsystem* WorldSubsystem : WorldSubsystems)
	{
		WorldSubsystem->OnWorldBeginPlay(*World);
	}
	
	if (World->GetAuthGameMode() == nullptr)
	{
		// call BeginPlay for actors
		TActorIterator<AWorldSettings> WorldSettings(World);
		WorldSettings->NotifyBeginPlay();
	}
	
	check(World->bBegunPlay);
}

void FAutomationWorld::RouteEndPlay() const
{
	if (!World->bBegunPlay)
	{
		return;
	}
	
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		// don't know which EEndPlayReason to select. Let's assume it mimics PIE
		It->RouteEndPlay(EEndPlayReason::EndPlayInEditor);
	}

	World->bBegunPlay = false;
}

void FAutomationWorld::TickWorld(int32 NumFrames)
{
	constexpr float DeltaTime = 1.0 / 60.0;
	while (NumFrames > 0)
	{
		World->Tick(ELevelTick::LEVELTICK_All, DeltaTime);

		// update external world subsystems streaming state
		for (UWorldSubsystem* WorldSubsystem: WorldSubsystems)
		{
			WorldSubsystem->UpdateStreamingState();
		}
		// tick streamable manager
		FTickableGameObject::TickObjects(nullptr, LEVELTICK_All, false, DeltaTime);
		// update level streaming, as we're not drawing viewport which usually updates it
		World->UpdateLevelStreaming();

		// tick for FAsyncMixin
		FTSTicker::GetCoreTicker().Tick(DeltaTime);
		++GFrameCounter;
		--NumFrames;
	}
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
