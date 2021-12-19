// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

namespace GranularNetworkMemoryTrackingPrivate
{
	struct ENGINE_API FScopeMarker
	{
		FScopeMarker(FArchive& InAr, FString&& InScopeName);
		~FScopeMarker();

		void BeginWork();

		void EndWork(const FString& WorkName);

		void LogCustomWork(const FString& WorkName, const uint64 Bytes) const;

		const bool IsEnabled() const
		{
			return ScopeStack != nullptr;
		}

		const FString& GetScopeName()
		{
			return ScopeName;
		}

	private:

		friend struct FNetworkMemoryTrackingScopeStack;

		uint64 PreWorkPos = 0;

		const FArchive& Ar;
		const FString ScopeName;
		struct FNetworkMemoryTrackingScopeStack* ScopeStack;
	};
}

#define GRANULAR_NETWORK_MEMORY_TRACKING_INIT(Archive, ScopeName) GranularNetworkMemoryTrackingPrivate::FScopeMarker GranularNetworkMemoryScope(Archive, ScopeName);
#define GRANULAR_NETWORK_MEMORY_TRACKING_TRACK(Id, Work) \
	{ \
		GranularNetworkMemoryScope.BeginWork(); \
		Work; \
		GranularNetworkMemoryScope.EndWork(Id); \
	}
#define GRANULAR_NETWORK_MEMORY_TRACKING_CUSTOM_WORK(Id, Value) GranularNetworkMemoryScope.LogCustomWork(Id, Value);

#else

#define GRANULAR_NETWORK_MEMORY_TRACKING_INIT(Archive, ScopeName) 
#define GRANULAR_NETWORK_MEMORY_TRACKING_TRACK(Id, Work) { Work; }
#define GRANULAR_NETWORK_MEMORY_TRACKING_CUSTOM_WORK(Id, Work) 

#endif
