// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PixelShaderUtils.h: Utilities for pixel shaders.
=============================================================================*/

#pragma once

#include "RenderResource.h"
#include "RenderGraphUtils.h"
#include "CommonRenderResources.h"
#include "RHI.h"
#include "RHIStaticStates.h"
#include "PipelineStateCache.h"


/** All utils for pixel shaders. */
struct RENDERCORE_API FPixelShaderUtils
{
	/** Utility vertex shader for rect array based operations. For example for clearing specified parts of an atlas. */
	class FRasterizeToRectsVS : public FGlobalShader
	{
		DECLARE_EXPORTED_SHADER_TYPE(FRasterizeToRectsVS, Global, RENDERCORE_API);
		SHADER_USE_PARAMETER_STRUCT(FRasterizeToRectsVS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint4>, RectMinMaxBuffer)
			SHADER_PARAMETER(FVector2D, InvViewSize)
			SHADER_PARAMETER(uint32, NumRects)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	};

	/** Draw a single triangle on the entire viewport. */
	static void DrawFullscreenTriangle(FRHICommandList& RHICmdList, uint32 InstanceCount = 1);

	/** Draw a two triangle on the entire viewport. */
	static void DrawFullscreenQuad(FRHICommandList& RHICmdList, uint32 InstanceCount = 1);

	/** Initialize a pipeline state object initializer with almost all the basics required to do a full viewport pass. */
	static void InitFullscreenPipelineState(
		FRHICommandList& RHICmdList,
		const FGlobalShaderMap* GlobalShaderMap,
		const TShaderRef<FShader>& PixelShader,
		FGraphicsPipelineStateInitializer& GraphicsPSOInit);

	/** Dispatch a full screen pixel shader to rhi command list with its parameters. */
	template<typename TShaderClass>
	static inline void DrawFullscreenPixelShader(
		FRHICommandList& RHICmdList, 
		const FGlobalShaderMap* GlobalShaderMap,
		const TShaderRef<TShaderClass>& PixelShader,
		const typename TShaderClass::FParameters& Parameters,
		const FIntRect& Viewport,
		FRHIBlendState* BlendState = nullptr,
		FRHIRasterizerState* RasterizerState = nullptr,
		FRHIDepthStencilState* DepthStencilState = nullptr,
		uint32 StencilRef = 0)
	{
		check(PixelShader.IsValid());
		RHICmdList.SetViewport(Viewport.Min.X, Viewport.Min.Y, 0.0f, Viewport.Max.X, Viewport.Max.Y, 1.0f);
		
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		InitFullscreenPipelineState(RHICmdList, GlobalShaderMap, PixelShader, /* out */ GraphicsPSOInit);
		GraphicsPSOInit.BlendState = BlendState ? BlendState : GraphicsPSOInit.BlendState;
		GraphicsPSOInit.RasterizerState = RasterizerState ? RasterizerState : GraphicsPSOInit.RasterizerState;
		GraphicsPSOInit.DepthStencilState = DepthStencilState ? DepthStencilState : GraphicsPSOInit.DepthStencilState;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
		RHICmdList.SetStencilRef(StencilRef);

		SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), Parameters);

		DrawFullscreenTriangle(RHICmdList);
	}

	/** Dispatch a pixel shader to render graph builder with its parameters. */
	template<typename TShaderClass>
	static inline void AddFullscreenPass(
		FRDGBuilder& GraphBuilder,
		const FGlobalShaderMap* GlobalShaderMap,
		FRDGEventName&& PassName,
		const TShaderRef<TShaderClass>& PixelShader,
		typename TShaderClass::FParameters* Parameters,
		const FIntRect& Viewport,
		FRHIBlendState* BlendState = nullptr,
		FRHIRasterizerState* RasterizerState = nullptr,
		FRHIDepthStencilState* DepthStencilState = nullptr,
		uint32 StencilRef = 0)
	{
		check(PixelShader.IsValid());
		ClearUnusedGraphResources(PixelShader, Parameters);

		GraphBuilder.AddPass(
			Forward<FRDGEventName>(PassName),
			Parameters,
			ERDGPassFlags::Raster,
			[Parameters, GlobalShaderMap, PixelShader, Viewport, BlendState, RasterizerState, DepthStencilState, StencilRef](FRHICommandList& RHICmdList)
		{
			FPixelShaderUtils::DrawFullscreenPixelShader(RHICmdList, GlobalShaderMap, PixelShader, *Parameters, Viewport, 
				BlendState, RasterizerState, DepthStencilState, StencilRef);
		});
	}

	/** Rect based pixel shader pass. */
	template<typename TPixelShaderClass, typename TPassParameters>
	static inline void AddRasterizeToRectsPass(
		FRDGBuilder& GraphBuilder,
		const FGlobalShaderMap* GlobalShaderMap,
		FRDGEventName&& PassName,
		const TShaderRef<TPixelShaderClass>& PixelShader,
		TPassParameters* Parameters,
		const FIntPoint& ViewportSize,
		FRDGBufferSRVRef RectMinMaxBufferSRV,
		uint32 NumRects,
		FRHIBlendState* BlendState = nullptr,
		FRHIRasterizerState* RasterizerState = nullptr,
		FRHIDepthStencilState* DepthStencilState = nullptr,
		uint32 StencilRef = 0)
	{
		auto VertexShader = GlobalShaderMap->GetShader<FRasterizeToRectsVS>();

		Parameters->VS.InvViewSize = FVector2D(1.0f / ViewportSize.X, 1.0f / ViewportSize.Y);
		Parameters->VS.RectMinMaxBuffer = RectMinMaxBufferSRV;
		Parameters->VS.NumRects = NumRects;

		ClearUnusedGraphResources(PixelShader, &Parameters->PS);

		GraphBuilder.AddPass(
			Forward<FRDGEventName>(PassName),
			Parameters,
			ERDGPassFlags::Raster,
			[Parameters, GlobalShaderMap, VertexShader, PixelShader, ViewportSize, BlendState, RasterizerState, DepthStencilState, StencilRef](FRHICommandList& RHICmdList)
		{
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			RHICmdList.SetViewport(0.0f, 0.0f, 0.0f, (float)ViewportSize.X, (float)ViewportSize.Y, 1.0f);

			GraphicsPSOInit.BlendState = BlendState ? BlendState : GraphicsPSOInit.BlendState;
			GraphicsPSOInit.RasterizerState = RasterizerState ? RasterizerState : GraphicsPSOInit.RasterizerState;
			GraphicsPSOInit.DepthStencilState = DepthStencilState ? DepthStencilState : GraphicsPSOInit.DepthStencilState;

			GraphicsPSOInit.PrimitiveType = GRHISupportsRectTopology ? PT_RectList : PT_TriangleList;

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GTileVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			RHICmdList.SetStencilRef(StencilRef);

			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Parameters->VS);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), Parameters->PS);

			const uint32 NumPrimitives = GRHISupportsRectTopology ? 1 : 2;
			const uint32 NumInstances = Parameters->VS.NumRects;
			RHICmdList.DrawPrimitive(0, NumPrimitives, NumInstances);
		});
	}

	static void UploadRectMinMaxBuffer(
		FRDGBuilder& GraphBuilder,
		const TArray<FUintVector4, SceneRenderingAllocator>& RectMinMaxArray,
		FRDGBufferRef RectMinMaxBuffer);
};
