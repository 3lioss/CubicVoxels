﻿// .cpp
#include "FVoxelThread.h"
#include "VoxelChunkThreadingUtilities.h"
#include "VoxelStructs.h"

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

		FChunkThreadedWorkOrderBase CurrentOrder;
		if (ChunkThreadedWorkOrdersQueue.Dequeue(CurrentOrder))
		{
			OrderedChunkThreadedWorkOrders.Add(CurrentOrder);
		}			
		else
		{
			OrderedChunkThreadedWorkOrders.Sort([this](FChunkThreadedWorkOrderBase A, FChunkThreadedWorkOrderBase B)
			{
				return this->IsFartherToPlayer(A,B);
 			}); // sort the pre-cooked chunks by distance to player
			
			if (!OrderedChunkThreadedWorkOrders.IsEmpty())
			{
				OrderedChunkThreadedWorkOrders.Pop().SendOrder();
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

TQueue<FChunkThreadedWorkOrderBase, EQueueMode::Spsc>* FVoxelThread::GetGenerationOrdersQueue()
{
	return &ChunkThreadedWorkOrdersQueue;
}

TQueue<TTuple<FIntVector, float>, EQueueMode::Spsc>* FVoxelThread::GetPlayerUpdatesQueue()
{
	return &PlayerPositionUpdates;
}

void FVoxelThread::StartShutdown()
{
	bShutdown = true;
	ChunkThreadedWorkOrdersQueue.Empty();
	OrderedChunkThreadedWorkOrders.Empty();
}

bool FVoxelThread::IsFartherToPlayer(FChunkThreadedWorkOrderBase A, FChunkThreadedWorkOrderBase B)
{
	const auto  CA = A.ChunkLocation - PlayerRelativeOrigin;
	const auto  CB = B.ChunkLocation - PlayerRelativeOrigin; 

	return (CA.X*CA.X + CA.Y*CA.Y + CA.Z*CA.Z > CB.X*CB.X + CB.Y*CB.Y + CB.Z*CB.Z);
}