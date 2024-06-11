// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelStructs.h"
#include "GameFramework/SaveGame.h"
#include "VoxelWorldNetworkSerializationSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class CUBICVOXELS_API UVoxelWorldNetworkSerializationSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	UPROPERTY()
	TMap<FIntVector, FRegionDataSerializationWrapper> WorldVoxelData;

	UPROPERTY()
	FString WorldName;
};
