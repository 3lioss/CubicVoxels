#pragma once
#include "ThreadedWorldGeneration/FVoxelWorldGenerationRunnable.h"
#include "Containers/Queue.h" 
#include "ReplicationStructs.generated.h"

struct FVoxelWorldManagedPlayerData
{
	FVoxelWorldGenerationRunnable* PlayerWorldGenerationThread;
	TQueue<FChunkThreadedWorkOrderBase, EQueueMode::Mpsc>* ChunkGenerationOrdersQueuePtr;
	
	FVoxelWorldGenerationRunnable* PlayerChunkSidesGenerationThread;
	TQueue<FChunkThreadedWorkOrderBase, EQueueMode::Mpsc>* ChunkSidesMeshingOrdersQueuePtr;
};

USTRUCT()
struct FVoxelStreamChunk
{
	GENERATED_USTRUCT_BODY()
	
	uint32 StartIndex;
	TArray<uint8> DataSlice;
	uint32 EndOfStreamIndex;
};

struct FVoxelStreamManager
{
	TArray<uint8> DataToStream;
	int32 CurrentIndex;
};

