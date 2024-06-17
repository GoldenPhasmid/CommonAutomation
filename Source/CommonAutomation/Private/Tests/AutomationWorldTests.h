#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "AutomationWorldTests.generated.h"

UCLASS(HideDropdown, Abstract)
class UTestWorldSubsystem: public UWorldSubsystem
{
	GENERATED_BODY()
public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override
	{
		Super::Initialize(Collection);
		bInitialized = true;
	}
	virtual void PostInitialize() override
	{
		Super::PostInitialize();
		bPostInitialized = true;
	}
	virtual void OnWorldBeginPlay(UWorld& InWorld) override
	{
		Super::OnWorldBeginPlay(InWorld);
		bBeginPlayCalled = true;
	}
	virtual void UpdateStreamingState() override
	{
		Super::UpdateStreamingState();
		bStreamingStateUpdated = true;
	}
	virtual void Deinitialize() override
	{
		Super::Deinitialize();
		(void)DeinitDelegate.ExecuteIfBound();
	}

	FSimpleDelegate DeinitDelegate;
	uint8 bInitialized: 1		= false;
	uint8 bPostInitialized: 1	= false;
	uint8 bBeginPlayCalled: 1	= false;
	uint8 bStreamingStateUpdated: 1 = false;
};

UCLASS(HideDropdown, Abstract)
class UTestGameInstanceSubsystem: public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override
	{
		Super::Initialize(Collection);
		bInitialized = true;
	}
	virtual void Deinitialize() override
	{
		Super::Deinitialize();
		(void)DeinitDelegate.ExecuteIfBound();
	}

	uint8 bInitialized: 1	= false;
	FSimpleDelegate DeinitDelegate;
};