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

	FRunnableThread* Thread;
	bool bShutdown= false;

	//Functions to retrieve pointers to the queues used to communicate with the game thread, they are used on the game thread
	TQueue< FChunkThreadedWorkOrderBase, EQueueMode::Mpsc>* GetGenerationOrdersQueue();
	
	//Queue used to communicate with the game thread
	TQueue< FChunkThreadedWorkOrderBase, EQueueMode::Mpsc> ChunkThreadedWorkOrdersQueue;

	//Method used to update the thread's reference for the player position
	void UpdatePlayerRelativeLocation(FIntVector NewLocation);

	void StartShutdown();

private:
	FCriticalSection PlayerPositionUpdateSection;
	FIntVector PlayerRelativeLocation;
	
	TArray< FChunkThreadedWorkOrderBase> OrderedChunkThreadedWorkOrders;

	bool IsFartherToPlayer(FChunkThreadedWorkOrderBase A, FChunkThreadedWorkOrderBase B);
};
