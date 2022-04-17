#include "..\..\Classes\StylizedSky\StylizedSky.h"
//#include "StylizedSky/StylizedSky.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineDefines.h"
#include "RenderingThread.h"
#include "Components/ArrowComponent.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Texture2D.h"
#include "ComponentReregisterContext.h"
#include "Components/BillboardComponent.h"
#include "Runtime/Renderer/Private/ScenePrivate.h"
#include "StylizedSky/StylizedSkyComponent.h"

#if WITH_EDITOR
#include "ObjectEditorUtils.h"
#endif

AStylizedSky::AStylizedSky(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StylizedSkyComponent = CreateDefaultSubobject<UStylizedSkyComponent>(TEXT("AtmosphericFogComponent0"));
	RootComponent = StylizedSkyComponent;

#if WITH_EDITORONLY_DATA
	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("ArrowComponent0"));

	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> FogTextureObject;
			FName ID_Fog;
			FText NAME_Fog;
			FConstructorStatics()
				: FogTextureObject(TEXT("/Engine/EditorResources/S_StylizedSky"))
				, ID_Fog(TEXT("Fog"))
				, NAME_Fog(NSLOCTEXT("SpriteCategory", "Fog", "Fog"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;


		if (GetSpriteComponent())
		{
			GetSpriteComponent()->Sprite = ConstructorStatics.FogTextureObject.Get();
			GetSpriteComponent()->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
			GetSpriteComponent()->SpriteInfo.Category = ConstructorStatics.ID_Fog;
			GetSpriteComponent()->SpriteInfo.DisplayName = ConstructorStatics.NAME_Fog;
			GetSpriteComponent()->SetupAttachment(StylizedSkyComponent);
		}

		if (ArrowComponent)
		{
			ArrowComponent->ArrowColor = FColor(150, 200, 255);

			ArrowComponent->bTreatAsASprite = true;
			ArrowComponent->SpriteInfo.Category = ConstructorStatics.ID_Fog;
			ArrowComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Fog;
			ArrowComponent->SetupAttachment(StylizedSkyComponent);
			ArrowComponent->bLightAttachment = true;
			ArrowComponent->bIsScreenSizeScaled = true;
		}
	}
#endif // WITH_EDITORONLY_DATA


	PrimaryActorTick.bCanEverTick = true;
	SetHidden(false);
}

void AStylizedSky::PostActorCreated()
{
}
