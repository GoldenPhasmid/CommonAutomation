#include "AutomationWorld.h"

#include "AutomationGameInstance.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameModeBase.h"
#include "Streaming/LevelStreamingDelegates.h"

bool FAutomationWorld::bRunningAutomationWorld = false;

FAutomationWorldInitParams FAutomationWorldInitParams::Minimal{EWorldType::Game, EWorldInitType::Minimal};
FAutomationWorldInitParams FAutomationWorldInitParams::WithGameInstance{EWorldType::Game, EWorldInitType::WithGameInstance};
FAutomationWorldInitParams FAutomationWorldInitParams::WithLocalPlayer{EWorldType::Game, EWorldInitType::WithLocalPlayer};

FAutomationWorld::FAutomationWorld()
{
	bRunningAutomationWorld = true;
	InitialFrameCounter = GFrameCounter;

	FLevelStreamingDelegates::OnLevelStreamingStateChanged.AddRaw(this, &FAutomationWorld::HandleLevelStreamingStateChange);
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
			LevelOuterWorld->ClearFlags(RF_Standalone);
		}
#endif
	}
}

FAutomationWorld::~FAutomationWorld()
{
	FLevelStreamingDelegates::OnLevelStreamingStateChanged.RemoveAll(this);
	
	if (World->bBegunPlay)
	{
		RouteEndPlay();
	}

	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		GameInstance->Shutdown();
	}
	
	GEngine->ShutdownWorldNetDriver(World);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	
	WorldContext = nullptr;
	World = nullptr;

	GEngine->ForceGarbageCollection();
	GFrameCounter = InitialFrameCounter;

	WorldSubsystems.Empty();
	GameInstanceSubsystems.Empty();
	
	bRunningAutomationWorld = false;
}

FAutomationWorldPtr FAutomationWorld::CreateWorld(const FAutomationWorldInitParams& InitParams)
{
	if (IsRunningAutomationWorld())
	{
		UE_LOG(LogTemp, Fatal, TEXT("Tring to create second automation world"));
	}

	FAutomationWorldPtr AutomationWorld = MakeShareable(new FAutomationWorld());

	AutomationWorld->InitParams = InitParams;
	AutomationWorld->InitValues
	.SetDefaultGameMode(InitParams.DefaultGameMode)
	.InitializeScenes(!!(InitParams.InitValues & EWorldInitType::InitScene))
	.AllowAudioPlayback(!!(InitParams.InitValues & EWorldInitType::InitAudio))
	.RequiresHitProxies(!!(InitParams.InitValues & EWorldInitType::InitHitProxy))
	.CreatePhysicsScene(!!(InitParams.InitValues & EWorldInitType::InitPhysics))
	.CreateNavigation(!!(InitParams.InitValues & EWorldInitType::InitNavigation))
	.CreateAISystem(!!(InitParams.InitValues & EWorldInitType::InitAI))
	.ShouldSimulatePhysics(!!(InitParams.InitValues & EWorldInitType::InitWeldedBodies))
	.EnableTraceCollision(!!(InitParams.InitValues & EWorldInitType::InitCollision))
	.SetTransactional(false) // @todo: does transactional ever matters for game worlds?
	.CreateFXSystem(!!(InitParams.InitValues & EWorldInitType::InitFX))
	.CreateWorldPartition(!!(InitParams.InitValues & EWorldInitType::InitWorldPartition));
	
	if (!!(InitParams.InitValues & EWorldInitType::CreateGameInstance))
	{
		CreateGameInstance(*AutomationWorld);
	}
	else
	{
		UWorld* World = UWorld::CreateWorld(InitParams.WorldType, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &AutomationWorld->InitValues, true);
		AutomationWorld->World = World;

		// invoke callbacks that should happen before world is fully initialized
		if (InitParams.InitWorld)
		{
			Invoke(InitParams.InitWorld, AutomationWorld->GetWorld());
		}
		if (InitParams.InitWorldSettings)
		{
			Invoke(InitParams.InitWorldSettings, AutomationWorld->GetWorld()->GetWorldSettings());
		}

		// finish world initialization
		World->InitWorld(AutomationWorld->InitValues);
		World->PersistentLevel->UpdateModelComponents();
        World->UpdateWorldComponents(true, false);

		AutomationWorld->WorldContext = &GEngine->CreateNewWorldContext(InitParams.WorldType);
		AutomationWorld->WorldContext->SetCurrentWorld(AutomationWorld->World);
	}

	if (!!(InitParams.InitValues & EWorldInitType::StartPlay))
	{
		AutomationWorld->RouteStartPlay();
	}

	if (!!(InitParams.InitValues & EWorldInitType::CreateLocalPlayer))
	{
		AutomationWorld->GetOrCreatePrimaryPlayer();
	}
	
	return AutomationWorld;
}

FAutomationWorldPtr FAutomationWorld::CreateGameWorld(TSubclassOf<AGameModeBase> DefaultGameMode, EWorldInitType InitValues)
{
	if (IsRunningAutomationWorld())
	{
		UE_LOG(LogTemp, Fatal, TEXT("Tring to create second automation world"));
	}

	FAutomationWorldInitParams InitParams;
	InitParams.WorldType = EWorldType::Game;
	InitParams.DefaultGameMode = DefaultGameMode;
	InitParams.InitValues = InitValues;

	return CreateWorld(InitParams);
}

UAutomationGameInstance* FAutomationWorld::CreateGameInstance(FAutomationWorld& MinimalWorld)
{
	UAutomationGameInstance* GameInstance = nullptr;
	for (const FWorldContext& WorldContext: GEngine->GetWorldContexts())
	{
		if (WorldContext.WorldType == EWorldType::Game)
		{
			if (UAutomationGameInstance* OtherGameInstance = Cast<UAutomationGameInstance>(WorldContext.OwningGameInstance))
			{
				GameInstance = OtherGameInstance;
				break;
			}
		}
	}
		
	if (GameInstance == nullptr)
	{
		GameInstance = NewObject<UAutomationGameInstance>(GEngine, NAME_None, RF_Transient);
	}
	check(GameInstance);

	GameInstance->DefaultGameModeClass = MinimalWorld.InitParams.DefaultGameMode;
		
	GameInstance->InitializeForAutomation(MinimalWorld.InitParams, MinimalWorld.InitValues);

	MinimalWorld.World = GameInstance->GetWorld();
	MinimalWorld.WorldContext = GameInstance->GetWorldContext();

	return GameInstance;
}

FAutomationWorldPtr FAutomationWorld::CreateGameWorldWithGameInstance(TSubclassOf<AGameModeBase> DefaultGameMode)
{
	return CreateGameWorld(DefaultGameMode, EWorldInitType::WithGameInstance);
}

FAutomationWorldPtr FAutomationWorld::CreateGameWorldWithPlayer(TSubclassOf<AGameModeBase> DefaultGameMode)
{
	return CreateGameWorld(DefaultGameMode, EWorldInitType::WithGameInstance | EWorldInitType::WithLocalPlayer);
}

bool FAutomationWorld::IsRunningAutomationWorld()
{
	return bRunningAutomationWorld;
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

	UGameInstance* GameInstance = World->GetGameInstance();
	const TArray<ULocalPlayer*>& LocalPlayers = GEngine->GetGamePlayers(World);
	
	FString Error;
	ULocalPlayer* LocalPlayer = GameInstance->CreateLocalPlayer(LocalPlayers.Num(), Error, true);

	checkf(Error.IsEmpty(), TEXT("%s"), *Error);

	return LocalPlayer;
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
	check(World->bIsWorldInitialized);
	if (World->bBegunPlay)
	{
		return;
	}

	FURL URL{};
	if (InitParams.DefaultGameMode != nullptr)
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			World->SetGameMode(URL);
		}
	}

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
