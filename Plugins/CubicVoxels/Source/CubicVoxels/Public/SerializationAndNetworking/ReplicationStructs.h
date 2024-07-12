#pragma once
#include "ThreadedWorldGeneration/FVoxelWorldGenerationRunnable.h"
#include "Containers/Queue.h"
#include "VoxelStructs.h"
#include "ReplicationStructs.generated.h"

class AVoxelDataStreamer;

struct FVoxelWorldManagedPlayerData
{
	FVoxelWorldGenerationRunnable* PlayerWorldGenerationThread;
	TQueue<FChunkThreadedWorkOrderBase, EQueueMode::Mpsc>* ChunkGenerationOrdersQueuePtr;
	
	FVoxelWorldGenerationRunnable* PlayerChunkSidesGenerationThread;
	TQueue<FChunkThreadedWorkOrderBase, EQueueMode::Mpsc>* ChunkSidesMeshingOrdersQueuePtr;

	TObjectPtr<AVoxelDataStreamer> PlayerDataStreamer;

	TSet<FIntVector> LocationsOfChunksThatExistOnClient;

};

USTRUCT()
struct FVoxelStreamChunk
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 StartIndex;

	UPROPERTY()
	TArray<uint8> DataSlice;

	UPROPERTY()
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
	mutable int32  StreamID; //Unique identifier that is assigned by the voxel data streamer
	
	FVoxelStreamData( FName TypeOfStream ,TArray<uint8> Data)
	{
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

USTRUCT()
struct FChunkReplicatedData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(SaveGame)
	FChunkGeometry ChunkGeometry;
	
};