// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "VoxelWorldGlobalDataSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class CUBICVOXELS_API UVoxelWorldGlobalDataSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TSet<FIntVector> SavedRegions;
};
