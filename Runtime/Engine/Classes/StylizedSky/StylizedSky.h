// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Info.h"
#include "StylizedSky.generated.h"


UCLASS(showcategories = (Movement, Rendering, "Utilities|Transformation", "Input|MouseInput", "Input|TouchInput"), ClassGroup = Fog, hidecategories = (Info, Object, Input), MinimalAPI)
class AStylizedSky : public AInfo
{
	GENERATED_UCLASS_BODY()

private:
	/** Main fog component */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Atmosphere, meta = (AllowPrivateAccess = "true"))
		class UStylizedSkyComponent* StylizedSkyComponent;

#if WITH_EDITORONLY_DATA
	/** Arrow component to indicate default sun rotation */
	UPROPERTY()
		class UArrowComponent* ArrowComponent;
#endif

public:

#if WITH_EDITOR
	virtual void PostActorCreated() override;
#endif

	/** Returns UStylizedSkyComponent subobject **/
	ENGINE_API class UStylizedSkyComponent* GetStylizedSkyComponent() { return StylizedSkyComponent; }
#if WITH_EDITORONLY_DATA
	/** Returns ArrowComponent subobject **/
	ENGINE_API class UArrowComponent* GetArrowComponent() { return ArrowComponent; }
#endif
};





