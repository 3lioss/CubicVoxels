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
	
	int32 StartIndex;
	TArray<uint8> DataSlice;
	int32 EndOfStreamIndex;
};

struct FVoxelStreamManager
{
	TArray<uint8> DataToStream;
	int32 CurrentIndex;
};

struct FVoxelStreamData
{

	FName StreamType;
	int32  StreamOwner; //To refer to an actor unambiguously in multiplayer, we use its global ID managed by the engine
	
	FVoxelStreamData(int32 StreamSender, FName TypeOfStream ,TArray<uint8> Data)
	{
		StreamOwner = StreamSender;
		StreamType = TypeOfStream;
		DataToStream = Data;
		
	}

	const TArray<uint8>& GetStreamData() const 
	{
		return DataToStream;
	}

private:
	TArray<uint8> DataToStream;
};

