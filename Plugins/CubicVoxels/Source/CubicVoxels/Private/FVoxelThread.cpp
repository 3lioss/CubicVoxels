// .cpp
#include "FVoxelThread.h"

bool FVoxelThread::Init() {
	/* Should the thread start? */
	PlayerRelativeOrigin = FIntVector(0,0,0);
	return true;
}

uint32 FVoxelThread::Run() {
	while (!bShutdown) {
		
		float LastSavedTime = 0;
		TTuple<FIntVector, float> CurrentPlayerPositionUpdate;

		while(PlayerPositionUpdates.Dequeue(CurrentPlayerPositionUpdate))
		{
			if (CurrentPlayerPositionUpdate.Get<1>() > LastSavedTime)
			{
				LastSavedTime = CurrentPlayerPositionUpdate.Get<1>();
				PlayerRelativeOrigin = CurrentPlayerPositionUpdate.Get<0>();
			}
		}

		FChunkThreadedWorkOrderBase CurrentOrder; //TODO: Replace this code by a max-finding function operating on TQueues
		if (ChunkThreadedWorkOrdersQueue.Dequeue(CurrentOrder))
		{
			OrderedChunkThreadedWorkOrders.Add(CurrentOrder);
			//UE_LOG(LogTemp, Display, TEXT("Acquired order for chunk at %d, %d, %d"), CurrentOrder.ChunkLocation.X, CurrentOrder.ChunkLocation.Y, CurrentOrder.ChunkLocation.Z )
		}			
		else
		{
			auto StartTime = FDateTime::UtcNow();
			
			if (!OrderedChunkThreadedWorkOrders.IsEmpty())
			{
				//Retrieving the chunk closest to the player
				auto NextOrder =  OrderedChunkThreadedWorkOrders[0];
				int32 NextOrderIndex = 0;
				for (int32 i = 0 ;	i < OrderedChunkThreadedWorkOrders.Num(); i++)
				{
					if (IsFartherToPlayer(NextOrder, OrderedChunkThreadedWorkOrders[i]))
					{
						NextOrder = OrderedChunkThreadedWorkOrders[i];
						NextOrderIndex = i;
					}
				}
				
				NextOrder.SendOrder();
				OrderedChunkThreadedWorkOrders.RemoveAt(NextOrderIndex);
			} 
			
			//UE_LOG(LogTemp, Verbose, TEXT("Time taken to order chunks: %f"), TimeElapsedInMs);

		}
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

TQueue<TTuple<FIntVector, float>, EQueueMode::Mpsc>* FVoxelThread::GetPlayerUpdatesQueue()
{
	return &PlayerPositionUpdates;
}

void FVoxelThread::StartShutdown()
{
	ChunkThreadedWorkOrdersQueue.Empty();
	OrderedChunkThreadedWorkOrders.Empty();
	bShutdown = true;
}

bool FVoxelThread::IsFartherToPlayer(FChunkThreadedWorkOrderBase A, FChunkThreadedWorkOrderBase B)
{
	const auto  CA = A.ChunkLocation - PlayerRelativeOrigin;
	const auto  CB = B.ChunkLocation - PlayerRelativeOrigin; 

	return (CA.X*CA.X + CA.Y*CA.Y + CA.Z*CA.Z > CB.X*CB.X + CB.Y*CB.Y + CB.Z*CB.Z);
}
