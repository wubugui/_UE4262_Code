#include "ForwardPlusShadingRenderer.h"
#include "ProfilingDebugging/ProfilingHelpers.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "EngineGlobals.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Components/ReflectionCaptureComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneCaptureComponentCube.h"
#include "DeferredShadingRenderer.h"
#include "DynamicPrimitiveDrawing.h"
#include "RenderTargetTemp.h"
#include "RendererModule.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "PostProcess/PostProcessEyeAdaptation.h"
#include "PostProcess/PostProcessSubsurface.h"
#include "PostProcess/TemporalAA.h"
#include "PostProcess/PostProcessing.h"
#include "CompositionLighting/CompositionLighting.h"
#include "LegacyScreenPercentageDriver.h"
#include "SceneViewExtension.h"
#include "AtmosphereRendering.h"
#include "Matinee/MatineeActor.h"
#include "ComponentRecreateRenderStateContext.h"
#include "PostProcess/PostProcessSubsurface.h"
#include "HdrCustomResolveShaders.h"
#include "WideCustomResolveShaders.h"
#include "PipelineStateCache.h"
#include "GPUSkinCache.h"
#include "PrecomputedVolumetricLightmap.h"
#include "RenderUtils.h"
#include "SceneUtils.h"
#include "ResolveShader.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "PostProcess/PostProcessing.h"
#include "SceneSoftwareOcclusion.h"
#include "VirtualTexturing.h"
#include "VisualizeTexturePresent.h"
#include "GPUScene.h"
#include "TranslucentRendering.h"
#include "VisualizeTexture.h"
#include "VisualizeTexturePresent.h"
#include "MeshDrawCommands.h"
#include "VT/VirtualTextureSystem.h"
#include "HAL/LowLevelMemTracker.h"
#include "IXRTrackingSystem.h"
#include "IXRCamera.h"
#include "IHeadMountedDisplay.h"
#include "DiaphragmDOF.h" 
#include "SingleLayerWaterRendering.h"
#include "HairStrands/HairStrandsVisibility.h"
#include "SystemTextures.h"
#include "ShaderParameterStruct.h"
#if WITH_EDITOR
#include "Rendering/StaticLightingSystemInterface.h"
#endif
#include "FXSystem.h"
DECLARE_GPU_STAT_NAMED(ForwardPlusShadingSceneRenderer, TEXT("Forward+ Scene Render"));

DECLARE_CYCLE_STAT(TEXT("SceneStart"), STAT_CLMM_SceneStart, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("SceneEnd"), STAT_CLMM_SceneEnd, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("InitViews"), STAT_CLMM_InitViews, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Opaque"), STAT_CLMM_Opaque, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Occlusion"), STAT_CLMM_Occlusion, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Post"), STAT_CLMM_Post, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Translucency"), STAT_CLMM_Translucency, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Shadows"), STAT_CLMM_Shadows, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("SceneSimulation"), STAT_CLMM_SceneSim, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("PrePass"), STAT_CLM_MobilePrePass, STATGROUP_CommandListMarkers);



BEGIN_SHADER_PARAMETER_STRUCT(FUpscaleParameters1, )
SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PointSceneColorTexture)
SHADER_PARAMETER_SAMPLER(SamplerState, PointSceneColorSampler)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()
class FUpscalePS1 : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FUpscalePS1);
	SHADER_USE_PARAMETER_STRUCT(FUpscalePS1, FGlobalShader);
	using FParameters = FUpscaleParameters1;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

//IMPLEMENT_GLOBAL_SHADER(FUpscalePS1, "/Engine/Shaders/Private/Second/StylizedSkyShader.usf", "MainPS", SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FUpscalePS1, TEXT("/Engine/Private/ForwardPlusPip/SimpleCopyRT.usf"), TEXT("MainPS"), SF_Pixel);

// 搜索FPlus来找到尚未处理的地方

FForwardPlusShadingSceneRenderer::FForwardPlusShadingSceneRenderer(const FSceneViewFamily* InViewFamily, FHitProxyConsumer* HitProxyConsumer)
	: FSceneRenderer(InViewFamily, HitProxyConsumer)
{
	// Don't do occlusion queries when doing scene captures
	for (FViewInfo& View : Views)
	{
		if (View.bIsSceneCapture)
		{
			View.bDisableQuerySubmissions = true;
			View.bIgnoreExistingQueries = true;
		}
	}
}

void FForwardPlusShadingSceneRenderer::Render(FRHICommandListImmediate& RHICmdList)
{
	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_SceneStart));

	SCOPED_DRAW_EVENT(RHICmdList, ForwardPlusShadingSceneRenderer);
	SCOPED_GPU_STAT(RHICmdList, ForwardPlusShadingSceneRenderer);

	Scene->UpdateAllPrimitiveSceneInfos(RHICmdList);

	PrepareViewRectsForRendering();

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RenderOther);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ForwardPlusShadingSceneRenderer_Render);
	//FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

	if (!ViewFamily.EngineShowFlags.Rendering)
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, ForwardPlusShadingSceneRenderer);
	SCOPED_GPU_STAT(RHICmdList, ForwardPlusShadingSceneRenderer);

	WaitOcclusionTests(RHICmdList);
	FRHICommandListExecutor::GetImmediateCommandList().PollOcclusionQueries();
	RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

	// Find the visible primitives and prepare targets and buffers for rendering
	InitViews(RHICmdList);

	TArray<const FViewInfo*> ViewList;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		ViewList.Add(&Views[ViewIndex]);
	}
	const FViewInfo& View = *ViewList[0];
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FRHITexture* SceneColor = nullptr;
	FRHITexture* SceneDepth = nullptr;
	ERenderTargetActions ColorTargetAction = ERenderTargetActions::Clear_Store;
	EDepthStencilTargetActions DepthTargetAction = EDepthStencilTargetActions::ClearDepthStencil_DontStoreDepthStencil;
	ColorTargetAction = ERenderTargetActions::Clear_Store;
	SceneColor = SceneContext.GetSceneColorSurface();
	RHICmdList.Transition(FRHITransitionInfo(SceneColor, ERHIAccess::Unknown, ERHIAccess::RTV));
	SceneDepth = SceneContext.GetSceneDepthSurface();
	FRHITexture* SceneColorResolve = nullptr;
	// 控制可变分辨率渲染的贴图
	FRHITexture* FoveationTexture = nullptr;
#if 1
	FRHIRenderPassInfo SceneColorRenderPassInfo(
		SceneColor,
		ColorTargetAction,
		SceneColorResolve,
		SceneDepth,
		DepthTargetAction,
		nullptr, // we never resolve scene depth on mobile
		FoveationTexture,
		FExclusiveDepthStencil::DepthWrite_StencilWrite
	);
#else
	FRHIRenderPassInfo SceneColorRenderPassInfo(
		SceneDepth,
		DepthTargetAction
	);
#endif
	SceneColorRenderPassInfo.SubpassHint = ESubpassHint::DepthReadSubpass;
	SceneColorRenderPassInfo.NumOcclusionQueries = 0;
	SceneColorRenderPassInfo.bOcclusionQueries = SceneColorRenderPassInfo.NumOcclusionQueries != 0;
	bool bIsMultiViewApplication = false;
	//if the scenecolor isn't multiview but the app is, need to render as a single-view multiview due to shaders
	SceneColorRenderPassInfo.MultiViewCount = View.bIsMobileMultiViewEnabled ? 2 : (bIsMultiViewApplication ? 1 : 0);

	RHICmdList.BeginRenderPass(SceneColorRenderPassInfo, TEXT("SceneColorRendering"));
	if (GIsEditor && !View.bIsSceneCapture)
	{
		// DrawClearQuad(RHICmdList, Views[0].BackgroundColor);
	}
	FMemMark Mark(FMemStack::Get());
	FRDGBuilder GraphBuilder(RHICmdList);
	FRDGTextureRef ViewFamilyTexture = TryCreateViewFamilyTexture(GraphBuilder, ViewFamily);
	// Note that we should move this uniform buffer set up process right after the InitView to avoid any uniform buffer creation during the rendering after we porting all the passes to the RDG.
				// We couldn't do it right now because the ResolveSceneDepth has another GraphicBuilder and it will re-register SceneDepthZ and that will cause crash.
	TArray<TRDGUniformBufferRef<FMobileSceneTextureUniformParameters>, TInlineAllocator<1, SceneRenderingAllocator>> MobileSceneTexturesPerView;
	MobileSceneTexturesPerView.SetNumZeroed(Views.Num());

	const auto SetupMobileSceneTexturesPerView = [&]()
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			EMobileSceneTextureSetupMode SetupMode = EMobileSceneTextureSetupMode::SceneColor;
			if (Views[ViewIndex].bCustomDepthStencilValid)
			{
				SetupMode |= EMobileSceneTextureSetupMode::CustomDepth;
			}

			MobileSceneTexturesPerView[ViewIndex] = CreateMobileSceneTextureUniformBuffer(GraphBuilder, SetupMode);
		}
	};

	SetupMobileSceneTexturesPerView();

	FScreenPassRenderTarget ViewFamilyOutput = FScreenPassRenderTarget::CreateViewFamilyOutput(ViewFamilyTexture, View);
	ViewFamilyOutput.LoadAction = View.IsFirstInFamily() ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad;
	FUpscaleParameters1* PassParameters = GraphBuilder.AllocParameters<FUpscaleParameters1>();
	PassParameters->RenderTargets[0] = ViewFamilyOutput.GetRenderTargetBinding();
	FScreenPassTexture SceneColorRDG((*MobileSceneTexturesPerView[0])->SceneColorTexture, View.ViewRect);
	PassParameters->PointSceneColorTexture = SceneColorRDG.Texture;
	PassParameters->PointSceneColorSampler = TStaticSamplerState<SF_Point, AM_Border, AM_Border, AM_Border>::GetRHI();
	PassParameters->View = View.ViewUniformBuffer;
	const TCHAR* const StageNames[] = { TEXT("PrimaryToSecondary"), TEXT("PrimaryToOutput"), TEXT("SecondaryToOutput") };

	const TCHAR* StageName = TEXT("SecondaryToOutput");// StageNames[static_cast<uint32>(SecondaryToOutput)];
	const auto& OutputViewport = Views[0].ViewRect;
	TShaderRef<FUpscalePS1> PixelShader = TShaderMapRef<FUpscalePS1>(View.ShaderMap);
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Upscale (%s) %dx%d", StageName, OutputViewport.Width(), OutputViewport.Height()),
		PassParameters,
		ERDGPassFlags::Raster,
		[&View, PixelShader, PassParameters, OutputViewport, &SceneColorRDG](FRHICommandList& RHICmdList)
		{
			RHICmdList.SetViewport(OutputViewport.Min.X, OutputViewport.Min.Y, 0.0f, OutputViewport.Max.X, OutputViewport.Max.Y, 1.0f);

			TShaderRef<FShader> VertexShader;
			
				TShaderMapRef<FScreenPassVS> TypedVertexShader(View.ShaderMap);
				SetScreenPassPipelineState(RHICmdList, FScreenPassPipelineState(TypedVertexShader, PixelShader));
				VertexShader = TypedVertexShader;
			
			check(VertexShader.IsValid());

			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

			DrawRectangle(
				RHICmdList,
				// Output Rect (RHI viewport relative).
				0, 0, OutputViewport.Width(), OutputViewport.Height(),
				// Input Rect
				SceneColorRDG.ViewRect.Min.X, SceneColorRDG.ViewRect.Min.Y, SceneColorRDG.ViewRect.Width(), SceneColorRDG.ViewRect.Height(),
				OutputViewport.Size(),
				SceneColorRDG.Texture->Desc.Extent,
				VertexShader,
				EDRF_UseTriangleOptimization);
		});
	RHICmdList.EndRenderPass();
	// execute会再次调用beginrenderPass，所以需要先endpass
	GraphBuilder.Execute();
#if 0
	
	RHICmdList.SetViewport(OutputViewport.Min.X, OutputViewport.Min.Y, 0.0f, OutputViewport.Max.X, OutputViewport.Max.Y, 1.0f);
	
	TShaderRef<FShader> VertexShader;
	
	{
		TShaderMapRef<FScreenPassVS> TypedVertexShader(View.ShaderMap);
		SetScreenPassPipelineState(RHICmdList, FScreenPassPipelineState(TypedVertexShader, PixelShader));
		VertexShader = TypedVertexShader;
	}
	check(VertexShader.IsValid());
	const FShaderParameterBindings& Bindings = PixelShader->Bindings;
	//RHICmdList.SetShaderTexture(PixelShader.GetShader(), )
	SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetShader(), *PassParameters);
#endif
	

}

void FForwardPlusShadingSceneRenderer::RenderHitProxies(FRHICommandListImmediate& RHICmdList)
{
	Scene->UpdateAllPrimitiveSceneInfos(RHICmdList);

	PrepareViewRectsForRendering();
	/*
#if WITH_EDITOR
	TRefCountPtr<IPooledRenderTarget> HitProxyRT;
	TRefCountPtr<IPooledRenderTarget> HitProxyDepthRT;
	InitHitProxyRender(RHICmdList, this, HitProxyRT, HitProxyDepthRT);
	// HitProxyRT==0 should never happen but better we don't crash
	if (HitProxyRT)
	{
		// Find the visible primitives.
		InitViews(RHICmdList);

		GEngine->GetPreRenderDelegate().Broadcast();

		// Global dynamic buffers need to be committed before rendering.
		DynamicIndexBuffer.Commit();
		DynamicVertexBuffer.Commit();
		DynamicReadBuffer.Commit();

		::DoRenderHitProxies(RHICmdList, this, HitProxyRT, HitProxyDepthRT);

		if (bDeferredShading)
		{
			// Release the original reference on the scene render targets
			FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
			SceneContext.AdjustGBufferRefCount(RHICmdList, -1);
		}

		GEngine->GetPostRenderDelegate().Broadcast();
	}

	check(RHICmdList.IsOutsideRenderPass());

#endif
*/
}

void FForwardPlusShadingSceneRenderer::InitViews(FRHICommandListImmediate& RHICmdList)
{
	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_InitViews));
	SCOPED_DRAW_EVENT(RHICmdList, InitViews);

	SCOPE_CYCLE_COUNTER(STAT_InitViewsTime);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(InitViews_Scene);

	check(Scene);

	bool bUseVirtualTexturing = false;
	if (bUseVirtualTexturing)
	{
		SCOPED_GPU_STAT(RHICmdList, VirtualTextureUpdate);
		// AllocateResources needs to be called before RHIBeginScene
		FVirtualTextureSystem::Get().AllocateResources(RHICmdList, FeatureLevel);
		FVirtualTextureSystem::Get().CallPendingCallbacks();
	}

	FILCUpdatePrimTaskData ILCTaskData;
	FViewVisibleCommandsPerView ViewCommandsPerView;
	ViewCommandsPerView.SetNum(Views.Num());

	const FExclusiveDepthStencil::Type BasePassDepthStencilAccess = FExclusiveDepthStencil::DepthWrite_StencilWrite;

	//PreVisibilityFrameSetup(RHICmdList);
	//ComputeViewVisibility(RHICmdList, BasePassDepthStencilAccess, ViewCommandsPerView, DynamicIndexBuffer, DynamicVertexBuffer, DynamicReadBuffer);
	//PostVisibilityFrameSetup(ILCTaskData);

	const FIntPoint RenderTargetSize = (ViewFamily.RenderTarget->GetRenderTargetTexture().IsValid()) ? ViewFamily.RenderTarget->GetRenderTargetTexture()->GetSizeXY() : ViewFamily.RenderTarget->GetSizeXY();
	const bool bRequiresUpscale = ((int32)RenderTargetSize.X > FamilySize.X || (int32)RenderTargetSize.Y > FamilySize.Y);
	// ES requires that the back buffer and depth match dimensions.
	// For the most part this is not the case when using scene captures. Thus scene captures always render to scene color target.
	//const bool bStereoRenderingAndHMD = ViewFamily.EngineShowFlags.StereoRendering && ViewFamily.EngineShowFlags.HMDDistortion;
	//bRenderToSceneColor = !bGammaSpace || bStereoRenderingAndHMD || bRequiresUpscale || FSceneRenderer::ShouldCompositeEditorPrimitives(Views[0]) || Views[0].bIsSceneCapture || Views[0].bIsReflectionCapture;
	//const FPlanarReflectionSceneProxy* PlanarReflectionSceneProxy = Scene ? Scene->GetForwardPassGlobalPlanarReflection() : nullptr;
	/*
	bRequiresPixelProjectedPlanarRelfectionPass = IsUsingMobilePixelProjectedReflection(ShaderPlatform)
		&& PlanarReflectionSceneProxy != nullptr
		&& PlanarReflectionSceneProxy->RenderTarget != nullptr
		&& !Views[0].bIsReflectionCapture
		&& !ViewFamily.EngineShowFlags.HitProxies
		&& ViewFamily.EngineShowFlags.Lighting
		&& !ViewFamily.EngineShowFlags.VisualizeLightCulling
		&& !ViewFamily.UseDebugViewPS()
		// Only support forward shading, we don't want to break tiled deferred shading.
		&& !bDeferredShading;


	bRequriesAmbientOcclusionPass = IsUsingMobileAmbientOcclusion(ShaderPlatform)
		&& Views[0].FinalPostProcessSettings.AmbientOcclusionIntensity > 0
		&& Views[0].FinalPostProcessSettings.AmbientOcclusionStaticFraction >= 1 / 100.0f
		&& ViewFamily.EngineShowFlags.Lighting
		&& !Views[0].bIsReflectionCapture
		&& !Views[0].bIsPlanarReflection
		&& !ViewFamily.EngineShowFlags.HitProxies
		&& !ViewFamily.EngineShowFlags.VisualizeLightCulling
		&& !ViewFamily.UseDebugViewPS()
		// Only support forward shading, we don't want to break tiled deferred shading.
		&& !bDeferredShading;
		*/
	// Whether we need to store depth for post-processing
	// On PowerVR we see flickering of shadows and depths not updating correctly if targets are discarded.
	// See CVarMobileForceDepthResolve use in ConditionalResolveSceneDepth.
	/*
	const bool bForceDepthResolve = (CVarMobileForceDepthResolve.GetValueOnRenderThread() == 1);
	const bool bSeparateTranslucencyActive = IsMobileSeparateTranslucencyActive(Views.GetData(), Views.Num());
	bRequiresMultiPass = RequiresMultiPass(RHICmdList, Views[0]);
	bKeepDepthContent =
		bRequiresMultiPass ||
		bForceDepthResolve ||
		bRequriesAmbientOcclusionPass ||
		bRequiresPixelProjectedPlanarRelfectionPass ||
		bSeparateTranslucencyActive ||
		Views[0].bIsReflectionCapture;
	// never keep MSAA depth
	bKeepDepthContent = (NumMSAASamples > 1 ? false : bKeepDepthContent);
	*/

	// Initialize global system textures (pass-through if already initialized).
	GSystemTextures.InitializeTextures(RHICmdList, FeatureLevel);
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	// Allocate the maximum scene render target space for the current view family.
	SceneContext.SetKeepDepthContent(false);
	SceneContext.Allocate(RHICmdList, this);
	bool bDeferredShading = false;
	//if (bDeferredShading)
//	{
	//	ETextureCreateFlags AddFlags = bRequiresMultiPass ? TexCreate_InputAttachmentRead : (TexCreate_InputAttachmentRead | TexCreate_Memoryless);
		//SceneContext.AllocGBufferTargets(RHICmdList, AddFlags);
	//}

	// Initialise Sky/View resources before the view global uniform buffer is built.
	if (ShouldRenderSkyAtmosphere(Scene, ViewFamily.EngineShowFlags))
	{
		InitSkyAtmosphereForViews(RHICmdList);
	}

#if 0
	if (bRequiresPixelProjectedPlanarRelfectionPass)
	{
		InitPixelProjectedReflectionOutputs(RHICmdList, PlanarReflectionSceneProxy->RenderTarget->GetSizeXY());
	}
	else
	{
		ReleasePixelProjectedReflectionOutputs();
	}

	if (bRequriesAmbientOcclusionPass)
	{
		InitAmbientOcclusionOutputs(RHICmdList, SceneContext.SceneDepthZ);
	}
	else
	{
		ReleaseAmbientOcclusionOutputs();
	}
#endif
	//make sure all the targets we're going to use will be safely writable.
	GRenderTargetPool.TransitionTargetsWritable(RHICmdList);

#if 0
	// Find out whether custom depth pass should be rendered.
	{
		bool bCouldUseCustomDepthStencil = !bGammaSpace && (!Scene->World || (Scene->World->WorldType != EWorldType::EditorPreview && Scene->World->WorldType != EWorldType::Inactive));
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			Views[ViewIndex].bCustomDepthStencilValid = bCouldUseCustomDepthStencil && UsesCustomDepthStencilLookup(Views[ViewIndex]);
			bShouldRenderCustomDepth |= Views[ViewIndex].bCustomDepthStencilValid;
		}
	}

	const bool bDynamicShadows = ViewFamily.EngineShowFlags.DynamicShadows;

	if (bDynamicShadows && !IsSimpleForwardShadingEnabled(ShaderPlatform))
	{
		// Setup dynamic shadows.
		InitDynamicShadows(RHICmdList);
	}
	else
	{
		// TODO: only do this when CSM + static is required.
		PrepareViewVisibilityLists();
	}

	/** Before SetupMobileBasePassAfterShadowInit, we need to update the uniform buffer and shadow info for all movable point lights.*/
	UpdateMovablePointLightUniformBufferAndShadowInfo();

	SetupMobileBasePassAfterShadowInit(BasePassDepthStencilAccess, ViewCommandsPerView);
#endif
	// if we kicked off ILC update via task, wait and finalize.
	if (ILCTaskData.TaskRef.IsValid())
	{
		Scene->IndirectLightingCache.FinalizeCacheUpdates(Scene, *this, ILCTaskData);
	}

	// initialize per-view uniform buffer.  Pass in shadow info as necessary.
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		if (bDeferredShading)
		{
			if (View.ViewState)
			{
				if (!View.ViewState->ForwardLightingResources)
				{
					View.ViewState->ForwardLightingResources.Reset(new FForwardLightingViewResources());
				}
				View.ForwardLightingResources = View.ViewState->ForwardLightingResources.Get();
			}
			else
			{
				View.ForwardLightingResourcesStorage.Reset(new FForwardLightingViewResources());
				View.ForwardLightingResources = View.ForwardLightingResourcesStorage.Get();
			}
		}

		if (View.ViewState)
		{
			View.ViewState->UpdatePreExposure(View);
		}

		// Initialize the view's RHI resources.
		View.InitRHIResources();
#if 0
		// TODO: remove when old path is removed
		// Create the directional light uniform buffers
		CreateDirectionalLightUniformBuffers(View);

		// Get the custom 1x1 target used to store exposure value and Toggle the two render targets used to store new and old.
		if (IsMobileEyeAdaptationEnabled(View))
		{
			View.SwapEyeAdaptationBuffers();
		}
#endif
	}

	UpdateGPUScene(RHICmdList, *Scene);
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		UploadDynamicPrimitiveShaderDataForView(RHICmdList, *Scene, Views[ViewIndex]);
	}

	extern TSet<IPersistentViewUniformBufferExtension*> PersistentViewUniformBufferExtensions;

	for (IPersistentViewUniformBufferExtension* Extension : PersistentViewUniformBufferExtensions)
	{
		Extension->BeginFrame();

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			// Must happen before RHI thread flush so any tasks we dispatch here can land in the idle gap during the flush
			Extension->PrepareView(&Views[ViewIndex]);
		}
	}

	// update buffers used in cached mesh path
	// in case there are multiple views, these buffers will be updated before rendering each view
	if (Views.Num() > 0)
	{
		const FViewInfo& View = Views[0];
		// We want to wait for the extension jobs only when the view is being actually rendered for the first time
		Scene->UniformBuffers.UpdateViewUniformBuffer(View, false);
		//UpdateOpaqueBasePassUniformBuffer(RHICmdList, View);
		//UpdateTranslucentBasePassUniformBuffer(RHICmdList, View);
		//UpdateDirectionalLightUniformBuffers(RHICmdList, View);
	}
	if (bDeferredShading)
	{
		SetupSceneReflectionCaptureBuffer(RHICmdList);
	}
	//UpdateSkyReflectionUniformBuffer();

	// Now that the indirect lighting cache is updated, we can update the uniform buffers.
	UpdatePrimitiveIndirectLightingCacheBuffers();

	OnStartRender(RHICmdList);

	// Whether to submit cmdbuffer with offscreen rendering before doing post-processing
	//bSubmitOffscreenRendering = (!bGammaSpace || bRenderToSceneColor) && CVarMobileFlushSceneColorRendering.GetValueOnAnyThread() != 0;
}
