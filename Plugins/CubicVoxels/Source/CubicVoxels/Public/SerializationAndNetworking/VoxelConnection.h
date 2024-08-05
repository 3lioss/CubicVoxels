// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetConnection.h"
#include "VoxelConnection.generated.h"

/**
 * NetConnection that is used to transmit voxel data from the server to the client
 */
UCLASS()
class CUBICVOXELS_API UVoxelConnection : public UNetConnection
{
	GENERATED_BODY()
};
