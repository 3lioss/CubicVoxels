// .h
#pragma once
#include "CoreMinimal.h"
#include "ChunkThreadingStructs.h"

class FVoxelThread : public FRunnable {
public:
	FVoxelThread(  ) {
		Thread = FRunnableThread::Create(this, TEXT("VoxelThread"));
	};

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Exit() override;
	virtual void Stop() override;

	TQueue< FChunkThreadedWorkOrderBase, EQueueMode::Spsc>* GetGenerationOrdersQueue( );
	TQueue<TTuple<FIntVector, float>, EQueueMode::Spsc>* GetPlayerUpdatesQueue();
	
	FRunnableThread* Thread;
	bool bShutdown= false;
	
	TQueue< FChunkThreadedWorkOrderBase, EQueueMode::Spsc> ChunkThreadedWorkOrdersQueue;
	TQueue<TTuple<FIntVector, float>, EQueueMode::Spsc> PlayerPositionUpdates;

private:
	FIntVector PlayerRelativeOrigin;
	
	TArray< FChunkThreadedWorkOrderBase> OrderedChunkThreadedWorkOrders;

	bool IsFartherToPlayer(FChunkThreadedWorkOrderBase A, FChunkThreadedWorkOrderBase B);
};
