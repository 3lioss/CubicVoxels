// .cpp
#include "FVoxelWorldGenerationRunnable.h"

FVoxelWorldGenerationRunnable::FVoxelWorldGenerationRunnable()
{
	Thread = FRunnableThread::Create(this, TEXT("World generation Thread"), 0, TPri_Normal);
}

bool FVoxelWorldGenerationRunnable::Init() {
	PlayerRelativeLocation = FIntVector(0,0,0);
	return true;
}

uint32 FVoxelWorldGenerationRunnable::Run() {
	while (!bShutdown) {
		
		//Empty the queue used to communicate with the game thread into an array that can be safely sorted
		FChunkThreadedWorkOrderBase CurrentOrder;
		while (ChunkThreadedWorkOrdersQueue.Dequeue(CurrentOrder) )
		{
			OrderedChunkThreadedWorkOrders.Add(CurrentOrder);
		}

		//Sorting the generation orders by distance to the player and executing them
		OrderedChunkThreadedWorkOrders.Sort([this](const FChunkThreadedWorkOrderBase& A, const FChunkThreadedWorkOrderBase& B)
		{
			return IsFartherToPlayer(B,A);
		});
		
		for (int32 i = 0; i < OrderedChunkThreadedWorkOrders.Num() ; i++)
		{
			OrderedChunkThreadedWorkOrders[i].SendOrder();
		}
		OrderedChunkThreadedWorkOrders.Empty();
		
		
		
	}
	return 0;
}

void FVoxelWorldGenerationRunnable::Exit() {
	/* Post-Run code, threaded */
}

void FVoxelWorldGenerationRunnable::Stop() {
	bShutdown = true;
}

TQueue<FChunkThreadedWorkOrderBase, EQueueMode::Mpsc>* FVoxelWorldGenerationRunnable::GetGenerationOrdersQueue()
{
	return &ChunkThreadedWorkOrdersQueue;
}


void FVoxelWorldGenerationRunnable::StartShutdown()
{
	ChunkThreadedWorkOrdersQueue.Empty();
	bShutdown = true;
}

bool FVoxelWorldGenerationRunnable::IsFartherToPlayer(FChunkThreadedWorkOrderBase A, FChunkThreadedWorkOrderBase B)
{
	/*Finds which to-be-generated chunk is closer to the player between A and B*/
	const auto  CA = A.ChunkLocation - PlayerRelativeLocation;
	const auto  CB = B.ChunkLocation - PlayerRelativeLocation; 

	return (CA.X*CA.X + CA.Y*CA.Y + CA.Z*CA.Z > CB.X*CB.X + CB.Y*CB.Y + CB.Z*CB.Z);
}
