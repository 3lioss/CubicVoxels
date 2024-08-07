﻿#pragma once
#include "VoxelStructs.h"
#include "VoxelChunkThreadingUtilities.h"
#include "Enums.h"

struct FChunkThreadedWorkOrderBase
{
	/*Structure that holds the data needed to generate a given chunk, depending on how the chunk needs to be generated*/

	//Data held
	FIntVector ChunkLocation;
	EChunkThreadedWorkOrderType OrderType;
	TQueue< TTuple<FIntVector, TSharedPtr<FChunkData>>, EQueueMode::Mpsc>* OutputChunkDataQueuePtr;
	TQueue< TSharedPtr<FChunkGeometry>, EQueueMode::Mpsc>* GeneratedChunkGeometryToLoadQueuePtr;

	FVoxel (*GenerationFunction) (FVector);
	TSharedPtr<FChunkData> TargetChunkDataPtr;

	//Data specific to chunk sides generation orders
	TSharedPtr<FChunkData> NeighboringChunkDataPtr;
	int32 DirectionIndex;

	//Method that generates the underlying chunk
	void SendOrder()
	{
		if (OrderType == EChunkThreadedWorkOrderType::GenerationAndMeshing)
		{
			GenerateChunkDataAndComputeInsideFaces(ChunkLocation, OutputChunkDataQueuePtr, GeneratedChunkGeometryToLoadQueuePtr, GenerationFunction);
		}

		if (OrderType == EChunkThreadedWorkOrderType::GeneratingAndMeshingWithAdditiveData)
		{
			GenerateUnloadedDataAndComputeInsideFaces(ChunkLocation, OutputChunkDataQueuePtr, GeneratedChunkGeometryToLoadQueuePtr, GenerationFunction, TargetChunkDataPtr);
		}

		if (OrderType == EChunkThreadedWorkOrderType::MeshingFromData)
		{
			ComputeInsideFacesOfLoadedChunk(ChunkLocation, OutputChunkDataQueuePtr, GeneratedChunkGeometryToLoadQueuePtr, TargetChunkDataPtr);
		}

		if(OrderType == EChunkThreadedWorkOrderType::GeneratingExistingChunksSides)
		{
			//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Launching order for chunk sides generation"));
			ComputeChunkSideFacesFromData(TargetChunkDataPtr, NeighboringChunkDataPtr, DirectionIndex, GeneratedChunkGeometryToLoadQueuePtr, ChunkLocation);
		}
		
	};

};

