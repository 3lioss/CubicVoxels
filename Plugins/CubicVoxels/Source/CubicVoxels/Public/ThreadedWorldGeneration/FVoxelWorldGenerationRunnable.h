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
	TMap<int32, FIntVector> ManagedPlayersPositionsMap;

	

private:
	
	TArray< FChunkThreadedWorkOrderBase> OrderedChunkThreadedWorkOrders;

	bool IsCloserToNearestPlayer(FChunkThreadedWorkOrderBase A, FChunkThreadedWorkOrderBase B, const TMap<int32, FIntVector>& ManagedPlayersPositionsMapConstCopy);

protected:
	FRunnableThread* Thread = nullptr;

};
