#include "AutomationTargetPoint.h"

#include "Components/TextRenderComponent.h"

AAutomationTargetPoint::AAutomationTargetPoint(const FObjectInitializer& Initializer): Super(Initializer)
{
	TextRenderComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Target Label"));
	TextRenderComponent->SetupAttachment(GetRootComponent());
	TextRenderComponent->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	TextRenderComponent->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	TextRenderComponent->SetTextRenderColor(FColor::Black);
	TextRenderComponent->SetWorldSize(150.f);

	if (!IsRunningCommandlet())
	{
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> TextMaterial;
			FConstructorStatics()
				: TextMaterial(TEXT("/CommonAutomation/M_Text_FaceCamera"))
			{}
		};

		static FConstructorStatics Statics;

		TextRenderComponent->SetMaterial(0, Statics.TextMaterial.Get());
	}

	SetHidden(true);
	SetCanBeDamaged(false);
}

void AAutomationTargetPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UpdateLabelProperties();
}

void AAutomationTargetPoint::UpdateLabelProperties()
{
	TextRenderComponent->SetText(FText::FromName(Label).ToUpper());
	SetActorLabel(Label.ToString(), true);
		
	Tags.AddUnique(Label);
}

#if WITH_EDITOR
void AAutomationTargetPoint::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(AAutomationTargetPoint, Label))
	{
		Tags.Remove(Label);
	}
}

void AAutomationTargetPoint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AAutomationTargetPoint, Label))
	{
		UpdateLabelProperties();
	}
}
#endif

void AAutomationTargetPoint::BeginPlay()
{
	Super::BeginPlay();

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	Destroy();
#endif
}


