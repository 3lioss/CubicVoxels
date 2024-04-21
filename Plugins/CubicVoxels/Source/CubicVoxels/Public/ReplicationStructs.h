#pragma once
#include "FVoxelWorldGenerationRunnable.h"
#include "Containers/Queue.h" 


struct FVoxelWorldManagedPlayerData
{
	FVoxelWorldGenerationRunnable* ManagingThread;
	TQueue<FChunkThreadedWorkOrderBase, EQueueMode::Mpsc>* ChunkThreadedWorkOrdersQueuePtr;
};
