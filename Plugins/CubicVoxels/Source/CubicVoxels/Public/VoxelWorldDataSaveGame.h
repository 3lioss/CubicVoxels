// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "VoxelStructs.h"
#include "VoxelWorldDataSaveGame.generated.h"



UCLASS()
class CUBICVOXELS_API UVoxelWorldDataSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	UPROPERTY(VisibleAnywhere)
	TMap<FIntVector, FChunkData> RegionData;
	
};
