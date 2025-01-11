
#include "AutomationWorldTests.h"

#include "AutomationCommon.h"
#include "AutomationTestDefinition.h"
#include "AutomationWorld.h"
#include "CommonAutomationSettings.h"
#include "EngineUtils.h"
#include "GameInstanceAutomationSupport.h"
#include "NavigationSystem.h"
#include "AI/NavigationSystemBase.h"
#include "GameFramework/GameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/GarbageCollection.h"
#include "WorldPartition/WorldPartition.h"

constexpr auto AutomationTestFlags = EAutomationTestFlags::EngineFilter | EAutomationTestFlags::EditorContext | EAutomationTestFlags::CriticalPriority;

namespace UE::Private
{
	static bool bTestSubsystemEnabled = false;
}

bool UTestWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return UE::Private::bTestSubsystemEnabled;
}

bool UTestGameInstanceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return UE::Private::bTestSubsystemEnabled;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FAutomationWorld_CreateWorldUniqueTest, "CommonAutomation.AutomationWorld.CreateWorld", AutomationTestFlags)

void FAutomationWorld_CreateWorldUniqueTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("Default"));
	OutBeautifiedNames.Add(TEXT("World Partition"));
	OutTestCommands.Add(TEXT("Default"));
	OutTestCommands.Add(TEXT("WorldPartition"));
}

bool FAutomationWorld_CreateWorldUniqueTest::RunTest(const FString& Parameters)
{
	EWorldInitFlags Flags = EWorldInitFlags::WithGameInstance;
	if (Parameters.Contains(TEXT("WorldPartition")))
	{
		Flags |= EWorldInitFlags::InitWorldPartition;
	}
	
	FAutomationWorldPtr ScopedWorld = FAutomationWorld::CreateGameWorld(Flags);
	UTEST_TRUE("Automation world is valid", ScopedWorld.IsValid());

	const FObjectKey WorldKey{ScopedWorld->GetWorld()};
	const FObjectKey GameInstanceKey{ScopedWorld->GetGameInstance()};
	const FObjectKey PackageKey{ScopedWorld->GetWorld()->GetPackage()};

	UTEST_TRUE("World is valid",		 WorldKey != FObjectKey{});
	UTEST_TRUE("World package is valid", PackageKey != FObjectKey{});
	UTEST_TRUE("Game instance is valid", GameInstanceKey != FObjectKey{});

	ScopedWorld.Reset();
	ScopedWorld = FAutomationWorld::CreateGameWorld(Flags);

	UTEST_TRUE("World is unique",			WorldKey		!= FObjectKey{ScopedWorld->GetWorld()});
	UTEST_TRUE("World package is unique",	PackageKey		!= FObjectKey{ScopedWorld->GetWorld()->GetPackage()});
#if !REUSE_GAME_INSTANCE
	UTEST_TRUE("Game instance is unique",	GameInstanceKey	!= FObjectKey{ScopedWorld->GetGameInstance()});
#endif
	
	return !HasAnyErrors();
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FAutomationWorld_LoadWorldUniqueTest, "CommonAutomation.AutomationWorld.LoadWorld", AutomationTestFlags)

void FAutomationWorld_LoadWorldUniqueTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("Default"));
	OutBeautifiedNames.Add(TEXT("World Partition"));
	OutTestCommands.Add(TEXT("/Engine/Maps/Entry"));
	OutTestCommands.Add(TEXT("/CommonAutomation/WPUnitTest"));
}

bool FAutomationWorld_LoadWorldUniqueTest::RunTest(const FString& Parameters)
{
	const FString TestMapPackageName = Parameters;

	const FSoftObjectPath WorldPath = UE::Automation::FindWorldAssetByName(TestMapPackageName);
	UTEST_TRUE("World path is valid", !WorldPath.IsNull());
	
	FAutomationWorldPtr ScopedWorld = FAutomationWorld::LoadGameWorld(WorldPath, EWorldInitFlags::WithGameInstance);
	UTEST_TRUE("Automation world is valid", ScopedWorld.IsValid());
	
	const FObjectKey WorldKey{ScopedWorld->GetWorld()};
	const FObjectKey GameInstanceKey{ScopedWorld->GetGameInstance()};
	const FObjectKey PackageKey{ScopedWorld->GetWorld()->GetPackage()};

	UTEST_TRUE("World is valid",		 WorldKey != FObjectKey{});
	UTEST_TRUE("World package is valid", PackageKey != FObjectKey{});
	UTEST_TRUE("Game instance is valid", GameInstanceKey != FObjectKey{});

	ScopedWorld.Reset();
	ScopedWorld = FAutomationWorld::LoadGameWorld(TestMapPackageName, EWorldInitFlags::WithGameInstance);

	UTEST_TRUE("World is unique",			WorldKey		!= FObjectKey{ScopedWorld->GetWorld()});
	UTEST_TRUE("World package is unique",	PackageKey		!= FObjectKey{ScopedWorld->GetWorld()->GetPackage()});
#if !REUSE_GAME_INSTANCE
	UTEST_TRUE("Game instance is unique",	GameInstanceKey	!= FObjectKey{ScopedWorld->GetGameInstance()});
#endif
	
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationWorldCoreTests, "CommonAutomation.AutomationWorld.Core", AutomationTestFlags)

bool FAutomationWorldCoreTests::RunTest(const FString& Parameters)
{
	{
		// guarantees with a basic game world.
		FAutomationWorldPtr WorldPtr = FAutomationWorld::CreateGameWorld();
		
		const UWorld* World = WorldPtr->GetWorld();
		const UPackage* Package = World->GetPackage();
		UTEST_TRUE("Automation world is running", FAutomationWorld::Exists() == true);
		UTEST_TRUE("Automation world returns a valid world", IsValid(World) && IsValid(Package));
		UTEST_TRUE("World is part of a root set", World->IsRooted() == true);
		UTEST_TRUE("World is initialized", World->bIsWorldInitialized);
		UTEST_TRUE("GWorld equals to new world", GWorld == World);
		UTEST_TRUE("World has world context", GEngine->GetWorldContextFromWorld(World) != nullptr);
		UTEST_TRUE("World doesnt have game instance", WorldPtr->GetWorld()->GetGameInstance() == nullptr);
		UTEST_TRUE("World doesnt have game mode", WorldPtr->GetWorld()->GetAuthGameMode() == nullptr);
		
		const FObjectKey WorldKey{World};
		const FObjectKey PackageKey{Package};
	
		constexpr bool bFullPurge = true;
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, bFullPurge);
		UTEST_TRUE("Automation world is valid after GC", IsValid(World) && IsValid(Package));

		WorldPtr.Reset();
		
		UTEST_TRUE("Automation world is no longer running", FAutomationWorld::Exists() == false);
		UTEST_TRUE("World is not longer a part of a root set", World->IsRooted() == false);
		
		WorldPtr = FAutomationWorld::CreateGameWorld();

		const UWorld* OtherWorld = WorldPtr->GetWorld();
		UTEST_TRUE("New automation world is different", WorldKey != FObjectKey{OtherWorld});
		const UPackage* OtherPackage = WorldPtr->GetWorld()->GetPackage();
		UTEST_TRUE("New automation world has different package", PackageKey != FObjectKey{OtherPackage});
		
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, bFullPurge);
		UTEST_TRUE("Automation world is not valid after GC", !World->IsValidLowLevel() && !Package->IsValidLowLevel());
	}
	
	{
		// game instance creation functionality
		FAutomationWorldPtr WorldPtr = FAutomationWorld::CreateGameWorldWithGameInstance();
		const UGameInstance* GameInstance = WorldPtr->GetWorld()->GetGameInstance();
	
		UTEST_TRUE("World has game instance", IsValid(GameInstance));
		// game instance is guaranteed. If project default GI doesn't implement required interface, UAutomationGameInstance is selected
		UTEST_TRUE("Game instance supports auto tests", GameInstance->Implements<UGameInstanceAutomationSupport>());
	}

	{
		// game mode is created when specified
		FAutomationWorldPtr WorldPtr = FAutomationWorld::CreateGameWorldWithGameInstance<ACommonAutomationGameMode>();
		const AGameModeBase* GameMode = WorldPtr->GetGameMode();
		UTEST_TRUE("World contains game mode", GameMode != nullptr);
		UTEST_TRUE("Game mode matches requested", GameMode->IsA<ACommonAutomationGameMode>());
	}

	{
		// automation world uses DefaultGameMode specified in Project Settings when said so
		UCommonAutomationSettings& Settings = *UCommonAutomationSettings::GetMutable();
		TGuardValue _{Settings.bUseProjectDefaultGameMode, false};
		TGuardValue<TSubclassOf<AGameModeBase>> __{Settings.DefaultGameMode, ACommonAutomationGameMode::StaticClass()};

		FAutomationWorldPtr WorldPtr = FAutomationWorld::CreateGameWorldWithGameInstance();
		const AGameModeBase* GameMode = WorldPtr->GetGameMode();
		UTEST_TRUE("World contains game mode", GameMode != nullptr);
		UTEST_TRUE("Game mode matches default game mode from Project Settings", GameMode->IsA<ACommonAutomationGameMode>());
	}


	{
		// CreateWorldWithPlayer creates a single player controller, local player and player pawn
		FAutomationWorldPtr WorldPtr = FAutomationWorld::CreateGameWorldWithPlayer();
		const APlayerController* PC = UGameplayStatics::GetPlayerController(WorldPtr->GetWorld(), 0);

		UTEST_TRUE("World has first player controller", IsValid(PC));
		UTEST_TRUE("First player controller has a pawn", IsValid(PC->GetPawn()));
		UTEST_TRUE("World has first local player", IsValid(PC->GetLocalPlayer()));
	}
	
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationWorldTest_WorldSubsystem, "CommonAutomation.AutomationWorld.WorldSubsystem", AutomationTestFlags)

bool FAutomationWorldTest_WorldSubsystem::RunTest(const FString& Parameters)
{
	// enable test subsystems
	TGuardValue EnableTestSubsystems{UE::Private::bTestSubsystemEnabled, true};
	
	const FWorldInitParams Params{EWorldType::Game, EWorldInitFlags::InitScene};
	FAutomationWorldPtr WorldPtr = FAutomationWorld::CreateWorld(Params);
	
	UTestWorldSubsystem* Subsystem = WorldPtr->GetWorld()->GetSubsystem<UTestWorldSubsystem>();
	UTEST_FALSE("Test subsystem is not created", IsValid(Subsystem));

	Subsystem = WorldPtr->GetOrCreateSubsystem<UTestWorldSubsystem>();
	UTEST_TRUE("Test subsystem is created", IsValid(Subsystem));
	UTEST_TRUE("Subsystem is initialized", Subsystem->bInitialized);
	UTEST_TRUE("Subsystem is post initialized", Subsystem->bPostInitialized);

	UTestWorldSubsystem* OtherSubsystem = WorldPtr->GetWorld()->GetSubsystem<UTestWorldSubsystem>();
	UTEST_EQUAL("Can receive world subsystem directly from world", Subsystem, OtherSubsystem);
		
	UTEST_TRUE("Subsystem has not begun play", Subsystem->bBeginPlayCalled == false);
	WorldPtr->RouteStartPlay();
	UTEST_TRUE("Subsystem has begun play", Subsystem->bBeginPlayCalled);

	WorldPtr->TickWorld(1);
	UTEST_TRUE("Subsystem updates streaming state", Subsystem->bStreamingStateUpdated);

	bool bDeinitialized = false;
	Subsystem->DeinitDelegate = FSimpleDelegate::CreateLambda([&bDeinitialized] { bDeinitialized = true; });

	WorldPtr.Reset();
	UTEST_TRUE("Subsystem is deinitialized", bDeinitialized);

	// project world subsystem can be explicitly enabled for automation world 
	WorldPtr = Init(Params).EnableSubsystem<UTestWorldSubsystem>().Create();
	Subsystem = WorldPtr->GetSubsystem<UTestWorldSubsystem>();
	// subsystem is created during world initialization
	UTEST_TRUE("Test subsystem is created", IsValid(Subsystem));
	UTEST_TRUE("Subsystem is initialized", Subsystem->bInitialized);
	
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationWorldTest_GameInstanceSubsystem, "CommonAutomation.AutomationWorld.GameInstanceSubsystem", AutomationTestFlags)

bool FAutomationWorldTest_GameInstanceSubsystem::RunTest(const FString& Parameters)
{
	// enable test subsystems
	TGuardValue EnableTestSubsystems{UE::Private::bTestSubsystemEnabled, true};
	
	FWorldInitParams Params{EWorldType::Game, EWorldInitFlags::InitScene | EWorldInitFlags::CreateGameInstance};
	FAutomationWorldPtr WorldPtr = FAutomationWorld::CreateWorld(Params);

	// project game instance subsystem is not created by default
	UTestGameInstanceSubsystem* Subsystem = WorldPtr->GetGameInstance()->GetSubsystem<UTestGameInstanceSubsystem>();
	UTEST_FALSE("Test subsystem is not created", IsValid(Subsystem));
	
	// project game instance subsystem can be created when world is already active
	Subsystem = WorldPtr->GetOrCreateSubsystem<UTestGameInstanceSubsystem>();
	UTEST_TRUE("Test subsystem is created", IsValid(Subsystem));
	UTEST_TRUE("Subsystem is initialized", Subsystem->bInitialized);

	// other systems will be able to find newly created subsystem by using default API
	UTestGameInstanceSubsystem* OtherSubsystem = WorldPtr->GetGameInstance()->GetSubsystem<UTestGameInstanceSubsystem>();
	UTEST_EQUAL("Can receive game instance subsystem directly from game instance", Subsystem, OtherSubsystem);

	bool bDeinitialized = false;
	Subsystem->DeinitDelegate = FSimpleDelegate::CreateLambda([&bDeinitialized] { bDeinitialized = true; });

	WorldPtr.Reset();
	UTEST_TRUE("Subsystem is deinitialized", bDeinitialized);

	// project game instance subsystem can be explicitly enabled
	WorldPtr = Init(Params).EnableSubsystem<UTestGameInstanceSubsystem>().Create();
	Subsystem = WorldPtr->GetSubsystem<UTestGameInstanceSubsystem>();
	// subsystem is created during world initialization
	UTEST_TRUE("Test subsystem is created", IsValid(Subsystem));
	UTEST_TRUE("Subsystem is initialized", Subsystem->bInitialized);
	
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationWorld_WorldTravel, "CommonAutomation.AutomationWorld.Travel", AutomationTestFlags)

bool FAutomationWorld_WorldTravel::RunTest(const FString& Parameters)
{
	// enable test subsystems to check for unwanted subsystem initialization
	TGuardValue EnableTestSubsystems{UE::Private::bTestSubsystemEnabled, true};
	
	// load default engine map as a game world
	const FString TestMapPackageName{TEXT("/Engine/Maps/Entry")};
	const FSoftObjectPath WorldPath = UE::Automation::FindAssetDataByPath(TestMapPackageName).ToSoftObjectPath();
	FWorldInitParams Params = FWorldInitParams{EWorldType::Game, EWorldInitFlags::WithGameInstance}
	.SetWorldPackage(WorldPath)
	.SetGameMode<ACommonAutomationGameMode>();
	
	{
		FAutomationWorldPtr ScopedWorld = FAutomationWorld::CreateWorld(Params);
		UTEST_TRUE("Automation world is valid", IsValid(*ScopedWorld));
		UTEST_TRUE("Test world subsystem is not created", ScopedWorld->GetSubsystem<UTestWorldSubsystem>() == nullptr);
		UTEST_TRUE("Test game instance subsystem is not created", ScopedWorld->GetSubsystem<UTestGameInstanceSubsystem>() == nullptr);

		ACommonAutomationGameMode* GameMode = ScopedWorld->GetGameMode<ACommonAutomationGameMode>();
		UTEST_TRUE("GameMode is valid", GameMode != nullptr);

		const FObjectKey World{ScopedWorld->GetWorld()};
		const FObjectKey GameInstance{ScopedWorld->GetGameInstance()};
		const FObjectKey GameModeKey{GameMode};
		
		ScopedWorld->AbsoluteWorldTravel(TSoftObjectPtr<UWorld>{WorldPath}, ACommonAutomationGameMode::StaticClass());
		UTEST_TRUE("AFTER WORLD TRAVEL: Automation world is valid", IsValid(*ScopedWorld));
		UTEST_TRUE("AFTER WORLD TRAVEL: world test subsystem is not created", ScopedWorld->GetSubsystem<UTestWorldSubsystem>() == nullptr);
		UTEST_TRUE("AFTER WORLD TRAVEL: world test subsystem is not created", ScopedWorld->GetSubsystem<UTestGameInstanceSubsystem>() == nullptr);
		
		const FObjectKey NewWorld{ScopedWorld->GetWorld()};
		UTEST_TRUE("AFTER WORLD TRAVEL: World is different",	World != NewWorld);
		const FObjectKey NewGameInstance{ScopedWorld->GetGameInstance()};
		UTEST_TRUE("AFTER WORLD TRAVEL: GameInstance is same",	GameInstance == NewGameInstance);

		ACommonAutomationGameMode* NewGameMode = ScopedWorld->GetGameMode<ACommonAutomationGameMode>();
		UTEST_TRUE("AFTER WORLD TRAVEL: GameMode is valid", NewGameMode != nullptr);
		const FObjectKey NewGameModeKey{NewGameMode};
		UTEST_TRUE("AFTER WORLD TRAVEL: GameMode object is different", GameModeKey != NewGameModeKey);
	}

	// enable test subsystems
	Params.EnableSubsystem<UTestWorldSubsystem>().EnableSubsystem<UTestGameInstanceSubsystem>();

	{
		FAutomationWorldPtr ScopedWorld = FAutomationWorld::CreateWorld(Params);
		UTEST_TRUE("Test world subsystem is created", ScopedWorld->GetSubsystem<UTestWorldSubsystem>() != nullptr);
		UTEST_TRUE("Test game instance subsystem is created", ScopedWorld->GetSubsystem<UTestGameInstanceSubsystem>() != nullptr);
		
		const FObjectKey WorldSubsystem{ScopedWorld->GetSubsystem<UTestWorldSubsystem>()};
		const FObjectKey GameInstanceSubsystem{ScopedWorld->GetSubsystem<UTestGameInstanceSubsystem>()};

		ScopedWorld->AbsoluteWorldTravel(TSoftObjectPtr<UWorld>{WorldPath}, ACommonAutomationGameMode::StaticClass());
		UTEST_TRUE("AFTER WORLD TRAVEL: Test world subsystem is created", ScopedWorld->GetSubsystem<UTestWorldSubsystem>() != nullptr);
		UTEST_TRUE("AFTER WORLD TRAVEL: Test game instance subsystem is created", ScopedWorld->GetSubsystem<UTestGameInstanceSubsystem>() != nullptr);

		const FObjectKey NewWorldSubsystem{ScopedWorld->GetSubsystem<UTestWorldSubsystem>()};
		UTEST_TRUE("WorldSubsystem is different", WorldSubsystem != NewWorldSubsystem);
		const FObjectKey NewGameInstanceSubsystem{ScopedWorld->GetSubsystem<UTestGameInstanceSubsystem>()};
		UTEST_TRUE("GameInstanceSubsystem is same",	GameInstanceSubsystem == NewGameInstanceSubsystem);
	}
	
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationWorld_NavigationSystemTest, "CommonAutomation.AutomationWorld.NavigationSystem", AutomationTestFlags)

bool FAutomationWorld_NavigationSystemTest::RunTest(const FString& Parameters)
{
	{
		FAutomationWorldPtr ScopedWorld = FAutomationWorld::CreateEditorWorld(EWorldInitFlags::InitScene | EWorldInitFlags::InitNavigation);

		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(*ScopedWorld);
		UTEST_TRUE("Navigation system is created for editor world", IsValid(NavSys));
		UTEST_TRUE("Navigation system is initialized for world", NavSys->IsInitialized() && NavSys->IsWorldInitDone());
	}

	{
		FAutomationWorldPtr ScopedWorld = FAutomationWorld::CreateGameWorld(EWorldInitFlags::InitScene | EWorldInitFlags::InitNavigation);

		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(*ScopedWorld);
		UTEST_TRUE("Navigation system is not initialized before BeginPlay", !NavSys->IsInitialized() && !NavSys->IsWorldInitDone());
		ScopedWorld->RouteStartPlay();
		UTEST_TRUE("Navigation system is initialized for world", NavSys->IsInitialized() && NavSys->IsWorldInitDone());
	}

	return !HasAnyErrors();
}

BEGIN_SIMPLE_AUTOMATION_TEST(FAutomationWorldFlagsTests, "CommonAutomation.AutomationWorld.Flags", AutomationTestFlags)
	void TestFlag(EWorldInitFlags Flag, TFunction<bool(UWorld*)> Pred);
	void TestEditorFlag(EWorldInitFlags Flag, TFunction<bool(UWorld*)> Pred);
END_SIMPLE_AUTOMATION_TEST(FAutomationWorldFlagsTests)

void FAutomationWorldFlagsTests::TestFlag(EWorldInitFlags Flag, TFunction<bool(UWorld*)> Pred)
{
	{
		auto WorldPtr = FAutomationWorld::CreateGameWorld(Flag);
		TestTrue(FString::Printf(TEXT("Flag %d works as expected"), static_cast<uint32>(Flag)), Pred(WorldPtr->GetWorld()));
	}

	{
		auto WorldPtr = FAutomationWorld::CreateGameWorld(EWorldInitFlags::None);
		TestFalse(FString::Printf(TEXT("Flag %d not specified"), static_cast<uint32>(Flag)), Pred(WorldPtr->GetWorld()));
	}
}

void FAutomationWorldFlagsTests::TestEditorFlag(EWorldInitFlags Flag, TFunction<bool(UWorld*)> Pred)
{
	{
		auto WorldPtr = FAutomationWorld::CreateEditorWorld(Flag);
		TestTrue(FString::Printf(TEXT("Flag %d works as expected"), static_cast<uint32>(Flag)), Pred(WorldPtr->GetWorld()));
	}

	{
		auto WorldPtr = FAutomationWorld::CreateEditorWorld(EWorldInitFlags::None);
		TestFalse(FString::Printf(TEXT("Flag %d not specified"), static_cast<uint32>(Flag)), Pred(WorldPtr->GetWorld()));
	}
}

bool FAutomationWorldFlagsTests::RunTest(const FString& Parameters)
{
	TestFlag(EWorldInitFlags::StartPlay, [](UWorld* World) -> bool {
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->HasActorBegunPlay())
			{
				return false;
			}
		}
		return World->HasBegunPlay();
	});
	TestFlag(EWorldInitFlags::InitScene, [](UWorld* World) {
		return World->Scene != nullptr;
	});
	TestFlag(EWorldInitFlags::InitPhysics, [](UWorld* World) {
		return World->GetPhysicsScene() != nullptr;
	});
	TestFlag(EWorldInitFlags::InitHitProxy, [](UWorld* World) {
    	return World->Scene != nullptr && World->RequiresHitProxies();
    });
	TestFlag(EWorldInitFlags::InitCollision, [](UWorld* World) -> bool {
		return World->bEnableTraceCollision;
	});
	TestFlag(EWorldInitFlags::InitWeldedBodies, [](UWorld* World) -> bool {
		return World->bShouldSimulatePhysics;
	});
	TestFlag(EWorldInitFlags::InitNavigation, [](UWorld* World) {
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		return NavSys != nullptr;
	});
	TestFlag(EWorldInitFlags::InitAI, [](UWorld* World) {
		return World->GetAISystem() != nullptr;
	});
	TestFlag(EWorldInitFlags::InitAudio, [](UWorld* World) {
		return World->AllowAudioPlayback();
	});
	TestFlag(EWorldInitFlags::InitFX, [](UWorld* World) {
		return World->FXSystem != nullptr;
	});
	TestFlag(EWorldInitFlags::InitWorldPartition, [](UWorld* World) {
		UWorldPartition* WorldPartition = World->GetWorldPartition();
		return WorldPartition && WorldPartition->IsInitialized() && WorldPartition->AlwaysLoadedActors == nullptr;
	});
	TestEditorFlag(EWorldInitFlags::InitWorldPartition, [](UWorld* World) {
		UWorldPartition* WorldPartition = World->GetWorldPartition();
		return WorldPartition && WorldPartition->IsInitialized() && WorldPartition->AlwaysLoadedActors != nullptr;
	});
	TestFlag(EWorldInitFlags::InitWorldPartition | EWorldInitFlags::DisableStreaming, [](UWorld* World) {
		UWorldPartition* WorldPartition = World->GetWorldPartition();
		return WorldPartition && WorldPartition->bEnableStreaming == false;
	});

	return !HasAnyErrors();
}
