#include "StylizedSky/StylizedSkyComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineDefines.h"
#include "RenderingThread.h"
#include "Components/ArrowComponent.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Texture2D.h"
#include "ComponentReregisterContext.h"
#include "Runtime/Renderer/Private/ScenePrivate.h"


UStylizedSkyComponent::UStylizedSkyComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

UStylizedSkyComponent::~UStylizedSkyComponent()
{
}

void UStylizedSkyComponent::CreateRenderState_Concurrent(FRegisterComponentContext * Context)
{
	Super::CreateRenderState_Concurrent(Context);
}

void UStylizedSkyComponent::SendRenderTransform_Concurrent()
{
	//...
	Super::SendRenderTransform_Concurrent();
}

void UStylizedSkyComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();
	//...
}

void UStylizedSkyComponent::PostLoad()
{
	Super::PostLoad();
}

bool UStylizedSkyComponent::IsPostLoadThreadSafe() const
{
	return true;
}

void UStylizedSkyComponent::BeginDestroy()
{
	// ADD CODE...

	Super::BeginDestroy();
}
#if WITH_EDITOR
void UStylizedSkyComponent::PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

void UStylizedSkyComponent::PostInterpChange(FProperty * PropertyThatChanged)
{
}
void UStylizedSkyComponent::Serialize(FArchive & Ar)
{
}
ENGINE_API void UStylizedSkyComponent::InitResource()
{
	
}
ENGINE_API void UStylizedSkyComponent::ReleaseResource()
{
	
}