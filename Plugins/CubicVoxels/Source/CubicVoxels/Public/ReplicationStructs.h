#pragma once
#include "FVoxelThread.h"
#include "Containers/Queue.h" 


struct FVoxelWorldManagedPlayerData
{
	FVoxelThread* ManagingThread;
	TQueue<FChunkThreadedWorkOrderBase, EQueueMode::Mpsc>* ChunkThreadedWorkOrdersQueuePtr;
};
