// .cpp
#include "FVoxelThread.h"

bool FVoxelThread::Init() {
	PlayerRelativeLocation = FIntVector(0,0,0);
	return true;
}

uint32 FVoxelThread::Run() {
	while (!bShutdown) {

		//Empty the queue used to communicate with the game thread into an array that can be safely iterated over
		FChunkThreadedWorkOrderBase CurrentOrder; 
		while (ChunkThreadedWorkOrdersQueue.Dequeue(CurrentOrder))
		{
			OrderedChunkThreadedWorkOrders.Add(CurrentOrder);
		}			
		
		OrderedChunkThreadedWorkOrders.Sort([this](const FChunkThreadedWorkOrderBase& A, const FChunkThreadedWorkOrderBase& B)
		{
			return IsFartherToPlayer(B,A);
		});
		for (int32 i = 0; i < OrderedChunkThreadedWorkOrders.Num(); i++)
		{
			OrderedChunkThreadedWorkOrders[i].SendOrder();
		}
		OrderedChunkThreadedWorkOrders.Empty();
		
			
		
	}
	return 0;
}

void FVoxelThread::Exit() {
	/* Post-Run code, threaded */
}

void FVoxelThread::Stop() {
	bShutdown = true;
}

TQueue<FChunkThreadedWorkOrderBase, EQueueMode::Mpsc>* FVoxelThread::GetGenerationOrdersQueue()
{
	return &ChunkThreadedWorkOrdersQueue;
}





void FVoxelThread::StartShutdown()
{
	ChunkThreadedWorkOrdersQueue.Empty();
	bShutdown = true;
}

bool FVoxelThread::IsFartherToPlayer(FChunkThreadedWorkOrderBase A, FChunkThreadedWorkOrderBase B)
{
	/*Finds which to-be-generated chunk is closer to the player between A and B*/
	const auto  CA = A.ChunkLocation - PlayerRelativeLocation;
	const auto  CB = B.ChunkLocation - PlayerRelativeLocation; 

	return (CA.X*CA.X + CA.Y*CA.Y + CA.Z*CA.Z > CB.X*CB.X + CB.Y*CB.Y + CB.Z*CB.Z);
}
