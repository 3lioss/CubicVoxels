﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VoxelStreamInterpretationInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UVoxelStreamInterpretationInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class CUBICVOXELS_API IVoxelStreamInterpretationInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	virtual void InterpretVoxelStream(int32 StreamOwner, FName StreamType) = 0;
};
