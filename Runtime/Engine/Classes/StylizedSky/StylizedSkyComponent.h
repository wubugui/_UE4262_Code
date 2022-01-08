// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "UObject/ObjectMacros.h"
#include "Components/SceneComponent.h"
#include "Serialization/BulkData.h"
#include "StylizedSkyComponent.generated.h"

#if 0
/** Structure storing Data for pre-computation */
USTRUCT(BlueprintType)
struct FAtmospherePrecomputeParameters
{
	GENERATED_USTRUCT_BODY()

		/** Rayleigh scattering density height scale, ranges from [0...1] */
		UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AtmosphereParam)
		float DensityHeight;

	UPROPERTY()
		float DecayHeight_DEPRECATED;

	/** Maximum scattering order */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AtmosphereParam)
		int32 MaxScatteringOrder;

	/** Transmittance Texture Width */
	UPROPERTY()
		int32 TransmittanceTexWidth;

	/** Transmittance Texture Height */
	UPROPERTY()
		int32 TransmittanceTexHeight;

	/** Irradiance Texture Width */
	UPROPERTY()
		int32 IrradianceTexWidth;

	/** Irradiance Texture Height */
	UPROPERTY()
		int32 IrradianceTexHeight;

	/** Number of different altitudes at which to sample inscatter color (size of 3D texture Z dimension)*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AtmosphereParam)
		int32 InscatterAltitudeSampleNum;

	/** Inscatter Texture Height */
	UPROPERTY()
		int32 InscatterMuNum;

	/** Inscatter Texture Width */
	UPROPERTY()
		int32 InscatterMuSNum;

	/** Inscatter Texture Width */
	UPROPERTY()
		int32 InscatterNuNum;

	FAtmospherePrecomputeParameters();

	bool operator == (const FAtmospherePrecomputeParameters& Other) const
	{
		return (DensityHeight == Other.DensityHeight)
			&& (MaxScatteringOrder == Other.MaxScatteringOrder)
			&& (TransmittanceTexWidth == Other.TransmittanceTexWidth)
			&& (TransmittanceTexHeight == Other.TransmittanceTexHeight)
			&& (IrradianceTexWidth == Other.IrradianceTexWidth)
			&& (IrradianceTexHeight == Other.IrradianceTexHeight)
			&& (InscatterAltitudeSampleNum == Other.InscatterAltitudeSampleNum)
			&& (InscatterMuNum == Other.InscatterMuNum)
			&& (InscatterMuSNum == Other.InscatterMuSNum)
			&& (InscatterNuNum == Other.InscatterNuNum);
	}

	bool operator != (const FAtmospherePrecomputeParameters& Other) const
	{
		return (DensityHeight != Other.DensityHeight)
			|| (MaxScatteringOrder != Other.MaxScatteringOrder)
			|| (TransmittanceTexWidth != Other.TransmittanceTexWidth)
			|| (TransmittanceTexHeight != Other.TransmittanceTexHeight)
			|| (IrradianceTexWidth != Other.IrradianceTexWidth)
			|| (IrradianceTexHeight != Other.IrradianceTexHeight)
			|| (InscatterAltitudeSampleNum != Other.InscatterAltitudeSampleNum)
			|| (InscatterMuNum != Other.InscatterMuNum)
			|| (InscatterMuSNum != Other.InscatterMuSNum)
			|| (InscatterNuNum != Other.InscatterNuNum);
	}

	float GetRHeight() const
	{
		return DensityHeight * DensityHeight * DensityHeight * 64.f; // This formula is used for legacy conversion reason. In itself it does not make any sense.
	}
};
#endif

/**
 *	Used to create fogging effects such as clouds.
 */
UCLASS(ClassGroup = Rendering, collapsecategories, hidecategories = (Object, Mobility, Activation, "Components|Activation"), editinlinenew, meta = (BlueprintSpawnableComponent), MinimalAPI)
class UStylizedSkyComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	~UStylizedSkyComponent();

	// stylized sky 参数
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = Sky)
		FColor HorizonLineColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = Sky)
		float HorizonLineContribution;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = Sky)
		FColor HorizonLineSunsetColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = Sky)
		float ToSunSetOff;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = Sky)
		float ToNightSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = Sky)
		FColor SkyNightGradient;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = Sky)
		FColor SkyGradientTop;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = Sky)
		float HorizonLineExponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = Sky)
		FColor SkyGradientBottom;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = Sky)
		float SkyGradientExponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = Sky)
		FColor NightBtColor;


public:
	// When non-zero, the component should flush rendering commands and see if there is any atmosphere stuff to deal with, then decrement it
	mutable FThreadSafeCounter GameThreadServiceRequest;

	//UFUNCTION(BlueprintCallable, Category = "Rendering|Components|AtmosphericFog")
		//void StartPrecompute();

protected:
	//~ Begin UActorComponent Interface.
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;
	virtual void SendRenderTransform_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
	//~ End UActorComponent Interface.


public:
#if 0
	// Stores colored transmittance from outer space to point in atmosphere.
	class FAtmosphereTextureResource* TransmittanceResource;
	// Stores ground illuminance as a function of sun direction and atmosphere radius.
	class FAtmosphereTextureResource* IrradianceResource;
	// Stores in-scattered luminance toward a point according to height and sun direction.
	class FAtmosphereTextureResource* InscatterResource;

	/** Source vector data. */
	mutable FByteBulkData TransmittanceData;
	mutable FByteBulkData IrradianceData;
	mutable FByteBulkData InscatterData;
#endif
	//~ Begin UObject Interface. 
	virtual void PostLoad() override;
	virtual bool IsPostLoadThreadSafe() const override;
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void PostInterpChange(FProperty* PropertyThatChanged) override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	ENGINE_API void InitResource();
	ENGINE_API void ReleaseResource();

	//~ Begin UActorComponent Interface.
	//virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;
	//~ End UActorComponent Interface.

	//void ApplyComponentInstanceData(struct FAtmospherePrecomputeInstanceData* ComponentInstanceData);
	//const FAtmospherePrecomputeParameters& GetPrecomputeParameters() const { return PrecomputeParams; }

	
	

private:
#if WITH_EDITORONLY_DATA
	//class FAtmospherePrecomputeDataHandler* PrecomputeDataHandler;

private:
#endif

	friend class FAtmosphericFogSceneInfo;
};

#if 0

PRAGMA_DISABLE_DEPRECATION_WARNINGS
/** Used to store data during RerunConstructionScripts */
USTRUCT()
struct FAtmospherePrecomputeInstanceData : public FSceneComponentInstanceData
{
	GENERATED_BODY()
public:
	FAtmospherePrecomputeInstanceData() = default;
	FAtmospherePrecomputeInstanceData(const UAtmosphericFogComponent* SourceComponent)
		: FSceneComponentInstanceData(SourceComponent)
	{}

	virtual ~FAtmospherePrecomputeInstanceData() = default;

	virtual bool ContainsData() const override
	{
		return true;
	}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<UAtmosphericFogComponent>(Component)->ApplyComponentInstanceData(this);
	}

	struct FAtmospherePrecomputeParameters PrecomputeParameter;

	FByteBulkData TransmittanceData;
	FByteBulkData IrradianceData;
	FByteBulkData InscatterData;
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif