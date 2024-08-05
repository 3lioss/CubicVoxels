// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IpNetDriver.h"
#include "IpNetDriver.h"
#include "Engine/NetDriver.h"
#include "VoxelNetDriver.generated.h"

/**
 * 
 */
UCLASS()
class CUBICVOXELS_API UVoxelNetDriver : public UIpNetDriver
{
	GENERATED_BODY()
	
	bool InitConnectionClass();
};
