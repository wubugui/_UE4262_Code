// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interface_BoneReferenceSkeletonProvider.generated.h"

UINTERFACE()
class ENGINE_API UBoneReferenceSkeletonProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for objects to provide skeletons that can be used with FBoneReference's details customization.
 */
class ENGINE_API IBoneReferenceSkeletonProvider
{
	GENERATED_BODY()

public:

	/**
	 * Called to get the skeleton that FBoneReference's details customization will use to populate
	 * bone names.
	 *
	 * @param [out] bInvalidSkeletonIsError		When true, returning an invalid skeleton will be treated as an error.
	 *
	 * @return The skeleton we should use.
	 */
	virtual class USkeleton* GetSkeleton(bool& bInvalidSkeletonIsError) PURE_VIRTUAL(IBoneReferenceSkeletonProvider::GetSkeleton, return nullptr; );
};
