// .h
#pragma once
#include "CoreMinimal.h"
#include "ChunkThreadingStructs.h"

class FVoxelWorldGenerationRunnable : public FRunnable {
public:
	FVoxelWorldGenerationRunnable();

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Exit() override;
	virtual void Stop() override;

	bool bShutdown= false;

	//Functions to retrieve pointers to the queues used to communicate with the game thread, they are used on the game thread
	TQueue< FChunkThreadedWorkOrderBase, EQueueMode::Mpsc>* GetGenerationOrdersQueue();
	
	//Queue used to communicate with the game thread
	TQueue< FChunkThreadedWorkOrderBase, EQueueMode::Mpsc> ChunkThreadedWorkOrdersQueue;

	void StartShutdown();
	
	//FIntVector PlayerRelativeLocation;
	TMap<TObjectPtr<APlayerController>, FIntVector> ManagedPlayersPositionsMap;

private:
	
	TArray< FChunkThreadedWorkOrderBase> OrderedChunkThreadedWorkOrders;

	//bool IsFartherToPlayer(FChunkThreadedWorkOrderBase A, FChunkThreadedWorkOrderBase B);
	bool IsCloserToNearestPlayer(FChunkThreadedWorkOrderBase A, FChunkThreadedWorkOrderBase B);

protected:
	FRunnableThread* Thread = nullptr;

};
