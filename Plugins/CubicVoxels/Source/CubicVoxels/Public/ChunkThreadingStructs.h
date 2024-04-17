#pragma once
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
	TQueue< TTuple<FIntVector, TMap<FIntVector4, FVoxel>>, EQueueMode::Mpsc>* OutputChunkFacesQueuePtr;

	FVoxel (*GenerationFunction) (FVector);
	TSharedPtr<FChunkData> CompressedChunkBlocksPtr;

	//Method that generates the underlying chunk
	void SendOrder()
	{
		if (OrderType == EChunkThreadedWorkOrderType::GenerationAndMeshing)
		{
			GenerateChunkDataAndComputeInsideFaces(ChunkLocation, OutputChunkDataQueuePtr, OutputChunkFacesQueuePtr, GenerationFunction);
		}

		if (OrderType == EChunkThreadedWorkOrderType::GeneratingAndMeshingWithAdditiveData)
		{
			GenerateUnloadedDataAndComputeInsideFaces(ChunkLocation, OutputChunkDataQueuePtr, OutputChunkFacesQueuePtr, GenerationFunction, CompressedChunkBlocksPtr);
		}

		if (OrderType == EChunkThreadedWorkOrderType::MeshingFromData)
		{
			ComputeInsideFacesOfLoadedChunk(ChunkLocation, OutputChunkDataQueuePtr, OutputChunkFacesQueuePtr, CompressedChunkBlocksPtr);
		}
		
	};

};

