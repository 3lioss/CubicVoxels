#pragma once
#include "FVoxelWorldGenerationRunnable.h"
#include "Containers/Queue.h" 


struct FVoxelWorldManagedPlayerData
{
	FVoxelWorldGenerationRunnable* PlayerWorldGenerationThread;
	TQueue<FChunkThreadedWorkOrderBase, EQueueMode::Mpsc>* ChunkGenerationOrdersQueuePtr;
	
	FVoxelWorldGenerationRunnable* PlayerChunkSidesGenerationThread;
	TQueue<FChunkThreadedWorkOrderBase, EQueueMode::Mpsc>* ChunkSidesMeshingOrdersQueuePtr;
};
