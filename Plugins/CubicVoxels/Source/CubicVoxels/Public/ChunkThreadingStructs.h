﻿#pragma once
#include "VoxelStructs.h"
#include "VoxelChunkThreadingUtilities.h"
#include "Enums.h"

struct FChunkThreadedWorkOrderBase
{
	FIntVector ChunkLocation;
	EChunkThreadedWorkOrderType OrderType;
	TQueue< TTuple<FIntVector, TSharedPtr<FChunkData>>, EQueueMode::Mpsc>* OutputChunkDataQueuePtr;
	TQueue< TTuple<FIntVector, TMap<FIntVector4, FVoxel>>, EQueueMode::Mpsc>* OutputChunkFacesQueuePtr;
	//int32 ChunkSize;
	//float BlockSize;

	FVoxel (*GenerationFunction) (FVector) ;
	TSharedPtr<FChunkData> CompressedChunkBlocksPtr;
	
	void SendOrder()
	{
		if (OrderType == EChunkThreadedWorkOrderType::GenerationAndMeshing)
		{
			GenerateChunkDataAndComputeInsideFaces(ChunkLocation, OutputChunkDataQueuePtr, OutputChunkFacesQueuePtr, GenerationFunction);
		}

		if (OrderType == EChunkThreadedWorkOrderType::MeshingFromData)
		{
			ComputeInsideFacesOfLoadedChunk(ChunkLocation, OutputChunkDataQueuePtr, OutputChunkFacesQueuePtr, CompressedChunkBlocksPtr);
		}
	};

};

