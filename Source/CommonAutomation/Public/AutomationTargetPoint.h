#pragma once

#include "CoreMinimal.h"
#include "Engine/TargetPoint.h"
#include "InstancedStruct.h"

#include "AutomationTargetPoint.generated.h"

class UTextRenderComponent;

UCLASS(HideCategories = (Actor, Input, Rendering, HLOD, Collision, Physics, Navigation))
class COMMONAUTOMATION_API AAutomationTargetPoint: public ATargetPoint
{
	GENERATED_BODY()
public:

	AAutomationTargetPoint(const FObjectInitializer& Initializer);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, Category = "Automation", meta = (Validate))
	FName Label = NAME_None;
	
	UPROPERTY(EditAnywhere, Category = "Automation", meta = (Validate, BaseStruct = "/Script/CommonAutomation.AutomationTestCustomData"))
	TArray<FInstancedStruct> CustomData;

protected:

	void UpdateLabelProperties();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (Validate, ValidateRecursive))
	TObjectPtr<UTextRenderComponent> TextRenderComponent;
};
