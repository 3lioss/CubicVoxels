// .cpp
#include "FVoxelThread.h"

bool FVoxelThread::Init() {
	PlayerRelativeOrigin = FIntVector(0,0,0);
	return true;
}

uint32 FVoxelThread::Run() {
	while (!bShutdown) {

		//Look at all the updates given on the player's position, and keep the most recent one
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

		
		FChunkThreadedWorkOrderBase CurrentOrder; 
		if (ChunkThreadedWorkOrdersQueue.Dequeue(CurrentOrder))
		{
			//Empty the queue used to communicate with the game thread into an array that can be safely iterated over
			OrderedChunkThreadedWorkOrders.Add(CurrentOrder);
		}			
		else
		{
			
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
				
				OrderedChunkThreadedWorkOrders.RemoveAt(NextOrderIndex);
				NextOrder.SendOrder();
			} 
			
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
	bShutdown = true;
}

bool FVoxelThread::IsFartherToPlayer(FChunkThreadedWorkOrderBase A, FChunkThreadedWorkOrderBase B)
{
	/*Finds which to-be-generated chunk is closer to the player between A and B*/
	const auto  CA = A.ChunkLocation - PlayerRelativeOrigin;
	const auto  CB = B.ChunkLocation - PlayerRelativeOrigin; 

	return (CA.X*CA.X + CA.Y*CA.Y + CA.Z*CA.Z > CB.X*CB.X + CB.Y*CB.Y + CB.Z*CB.Z);
}
