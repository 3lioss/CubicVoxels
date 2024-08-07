﻿// .cpp
#include "ThreadedWorldGeneration/FVoxelWorldGenerationRunnable.h"

FVoxelWorldGenerationRunnable::FVoxelWorldGenerationRunnable()
{
	Thread = FRunnableThread::Create(this, TEXT("World generation Thread"), 0, TPri_Normal);

}

bool FVoxelWorldGenerationRunnable::Init() {
	//PlayerRelativeLocation = FIntVector(0,0,0);
	return true;
}

uint32 FVoxelWorldGenerationRunnable::Run() {
	while (!bShutdown) {
		
		//Empty the queue used to communicate with the game thread into an array
		//Sorted by distance to the players

		FChunkThreadedWorkOrderBase CurrentOrder;
		while (ChunkThreadedWorkOrdersQueue.Dequeue(CurrentOrder) && !bShutdown )
		{
			SortedByDistanceToPlayerChunkBuffer.Add(CurrentOrder);
		}

		const TMap<int32, FIntVector> ManagedPlayersPositionsMapCopy = ManagedPlayersPositionsMapCopy; //Copy of the managed players' positions that is guaranteed not to change during sorting of generation orders by distance

		SortedByDistanceToPlayerChunkBuffer.Sort([this, ManagedPlayersPositionsMapCopy](const FChunkThreadedWorkOrderBase& A, const FChunkThreadedWorkOrderBase& B)
		{
			return IsCloserToNearestPlayer(A,B, ManagedPlayersPositionsMapCopy);
		});
		
		
		for (int32 i = 0; i < SortedByDistanceToPlayerChunkBuffer.Num() && !bShutdown ; i++)
		{
			SortedByDistanceToPlayerChunkBuffer[i].SendOrder();
		}
		SortedByDistanceToPlayerChunkBuffer.Empty();
		
	}
	UE_LOG(LogTemp, Warning, TEXT("Run function is exiting"))
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



bool FVoxelWorldGenerationRunnable::IsCloserToNearestPlayer(FChunkThreadedWorkOrderBase A,
                                                            FChunkThreadedWorkOrderBase B, const TMap<int32, FIntVector>& ManagedPlayersPositionsMapConstCopy)
{
	int32 MinSquareDistanceAToPlayer = -1;
	int32 MinSquareDistanceBToPlayer = -1;

	const auto PlayerPositionsCopy = ManagedPlayersPositionsMap;
	for (const auto& PlayerDataPair : PlayerPositionsCopy) //TODO: Add a set that stores all the managed players to avoid weird data race issue
	{
		
		const auto DA = (PlayerDataPair.Value - A.ChunkLocation);
		const int32 CurrentDistanceToA = DA.X*DA.X + DA.Y*DA.Y + DA.Z*DA.Z;
		if (MinSquareDistanceAToPlayer == -1 || CurrentDistanceToA < MinSquareDistanceAToPlayer)
		{
			MinSquareDistanceAToPlayer = CurrentDistanceToA;
		}

		const auto DB = (PlayerDataPair.Value - B.ChunkLocation);
		const int32 CurrentDistanceToB = DB.X*DB.X + DB.Y*DB.Y + DB.Z*DB.Z;
		if (MinSquareDistanceBToPlayer == -1 || CurrentDistanceToB < MinSquareDistanceBToPlayer)
		{
			MinSquareDistanceBToPlayer = CurrentDistanceToB;
		}
		
	}

	return MinSquareDistanceAToPlayer < MinSquareDistanceBToPlayer;
}
