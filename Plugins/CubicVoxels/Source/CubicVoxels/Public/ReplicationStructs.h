#pragma once
#include "ThreadedWorldGeneration/FVoxelWorldGenerationRunnable.h"
#include "Containers/Queue.h" 
#include "ReplicationStructs.generated.h"

class AVoxelDataStreamer;

struct FVoxelWorldManagedPlayerData
{
	FVoxelWorldGenerationRunnable* PlayerWorldGenerationThread;
	TQueue<FChunkThreadedWorkOrderBase, EQueueMode::Mpsc>* ChunkGenerationOrdersQueuePtr;
	
	FVoxelWorldGenerationRunnable* PlayerChunkSidesGenerationThread;
	TQueue<FChunkThreadedWorkOrderBase, EQueueMode::Mpsc>* ChunkSidesMeshingOrdersQueuePtr;

	TObjectPtr<AVoxelDataStreamer> PlayerDataStreamer;
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

struct FVoxelStreamData
{
	FName StreamType;
	int32 StreamOwner; //To refer to an actor unambiguously in multiplayer, we use its global ID managed by the engine
	TArray<uint8> DataToStream;
	int32 CurrentIndex;
	int32 ChunkSize;

	FVoxelStreamData()
	{
		CurrentIndex = 0;
		ChunkSize = 32000;
		StreamType = FName();
	}

	FVoxelStreamData(int32 StreamSender, FName TypeOfStream ,TArray<uint8> Data, int32 StreamChunkSize)
	{
		StreamOwner = StreamSender;
		StreamType = TypeOfStream;
		CurrentIndex = 0;
		DataToStream = Data;
		ChunkSize = StreamChunkSize;
		
		ChunkSize = 32000;
	}
};

