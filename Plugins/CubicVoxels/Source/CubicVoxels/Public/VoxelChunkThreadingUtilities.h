﻿#pragma once
#include "VoxelStructs.h"

static FVoxel GetBlockAtInChunk(FIntVector IntLocation,const FChunkData ChunkData, int32 ChunkSize) //Function to manipulate compressed chunk data
{
	const int32 BlockIndex = IntLocation.X*ChunkSize*ChunkSize + IntLocation.Y*ChunkSize + IntLocation.Z ;
	
	int32 Counter = -1;
	int32 i = 0;
	while (Counter + ChunkData.ChunkData[i].StackSize < BlockIndex)
	{
		Counter += ChunkData.ChunkData[i].StackSize;
		i += 1;
		if (i >= ChunkData.ChunkData.Num())
		{
			FVoxel Temp;
			Temp.VoxelType = "Error";
			return Temp;
		}
	}
	return ChunkData.ChunkData[i].Voxel;
}

//constant arrays helpful to build the triangles of a chunk's mesh
const  FVector BlockVertexData[8] = {
	FVector(1,1,1),
	FVector(1,0,1),
	FVector(1,0,0),
	FVector(1,1,0),
	FVector(0,0,1),
	FVector(0,1,1),
	FVector(0,1,0),
	FVector(0,0,0)
};

const int BlockTriangleData[24] = {
	0,1,2,3, // Forward
	5,0,3,6, // Right
	4,5,6,7, // Back
	1,4,7,2, // Left
	5,4,1,0, // Up
	3,2,7,6  // Down
};

static bool CompareVoxels(FVoxel v1, FVoxel v2) //function to test if two voxels are equal
{
	return (v1.VoxelType == v2.VoxelType);
}
	
static void GenerateChunkDataAndComputeInsideFaces(FIntVector Coordinates, TQueue< TTuple<FIntVector, TSharedPtr<FChunkData>>, EQueueMode::Mpsc>* PreCookedChunksToLoadBlockData, TQueue< TTuple<FIntVector, TMap<FIntVector4, FVoxel>>, EQueueMode::Mpsc>* ChunkQuadsToLoad,const int32 ChunkSize, float VoxelSize, FVoxel (*GenerationFunction) (FVector))
{
	//Create the blocks three-dimensional array
	const TUniquePtr<TArray< TArray<TArray<FVoxel>>>> ChunkBlocksPtr(new TArray< TArray<TArray<FVoxel>>>);
	const TSharedPtr<FChunkData> CompressedChunkBlocksPtr(new FChunkData);
	ChunkBlocksPtr->SetNum(ChunkSize);

	//Fill the arrays with the chunk's voxels
	int32 BlockCounter = 0;
	for (int32 x = 0; x < ChunkSize; x++)
	{
		(*ChunkBlocksPtr)[x].SetNum(ChunkSize);
		for (int32 y = 0; y < ChunkSize; y++)
		{
			(*ChunkBlocksPtr)[x][y].SetNum(ChunkSize);
			for (int32 z = 0; z < ChunkSize; z++)
			{
				auto const Position = VoxelSize*FVector(x + ChunkSize*Coordinates.X, y + ChunkSize*Coordinates.Y , z + ChunkSize*Coordinates.Z);
					
				(*ChunkBlocksPtr)[x][y][z] = (*GenerationFunction)(Position);

				if (x ==0 && y==0 && z==0)
				{
					(CompressedChunkBlocksPtr->ChunkData).Add(MakeStack( (*ChunkBlocksPtr)[0][0][0], 1));
				}
				else
				{
					if ( CompareVoxels((CompressedChunkBlocksPtr->ChunkData)[BlockCounter].Voxel , (*ChunkBlocksPtr)[x][y][z]) )
					{
						(CompressedChunkBlocksPtr->ChunkData)[BlockCounter].StackSize += 1;
					}
					else
					{
						(CompressedChunkBlocksPtr->ChunkData).Add( MakeStack( (*ChunkBlocksPtr)[x][y][z], 1)  );
						BlockCounter +=1;
					}
				}

				if ( ! CompareVoxels( (*ChunkBlocksPtr)[x][y][z] , GetBlockAtInChunk(FIntVector(x,y,z), (*CompressedChunkBlocksPtr), ChunkSize ) ))
				{
					UE_LOG(LogTemp, Error, TEXT("There is a compression or decompression problem"));
				}
					
			}
		}
	}
		
	//Generate the chunk's quads data
	TMap<FIntVector4, FVoxel> QuadsData;
	
	for (int x = 0; x < ChunkSize; ++x)
	{
		for (int y = 0; y < ChunkSize; ++y)
		{
			for (int z = 0; z < ChunkSize; ++z)
			{
				
				if ((*ChunkBlocksPtr)[x][y][z].VoxelType != "Air" )
				{
					const FIntVector Directions[6] = {
						FIntVector(x+1,y,z),
						FIntVector(x,y+1,z),
						FIntVector(x-1,y,z),
						FIntVector(x,y-1,z),
						FIntVector(x,y,z+1),
						FIntVector(x,y,z-1)							
					};
						
					for (int i = 0; i < 6; ++i)
					{
						if (!(Directions[i].X < 0 || Directions[i].X >= ChunkSize || Directions[i].Y < 0 || Directions[i].Y >= ChunkSize || Directions[i].Z < 0 || Directions[i].Z >= ChunkSize)) 
						{
							if ((*ChunkBlocksPtr)[Directions[i].X][Directions[i].Y][Directions[i].Z].IsTransparent == true && !CompareVoxels((*ChunkBlocksPtr)[Directions[i].X][Directions[i].Y][Directions[i].Z], (*ChunkBlocksPtr)[x][y][z]) ) 
							{
								QuadsData.Add(FIntVector4(x,y,z,i), (*ChunkBlocksPtr)[x][y][z]);
							}
						}
					}
				}
			}
		}
	}

	ChunkQuadsToLoad->Enqueue(MakeTuple(Coordinates, QuadsData));
	PreCookedChunksToLoadBlockData->Enqueue(MakeTuple(Coordinates, CompressedChunkBlocksPtr));
}

static void ComputeInsideFacesOfLoadedChunk(FIntVector Coordinates, TQueue< TTuple<FIntVector, TSharedPtr<FChunkData>>, EQueueMode::Mpsc>* PreCookedChunksToLoadBlockData, TQueue< TTuple<FIntVector, TMap<FIntVector4, FVoxel>>, EQueueMode::Mpsc>* ChunkQuadsToLoad,const int32 ChunkSize, float VoxelSize, TSharedPtr<FChunkData> CompressedChunkBlocksPtr)
{
		
	//Generate the chunk's quads data
	TMap<FIntVector4, FVoxel> QuadsData;
	
	for (int x = 0; x < ChunkSize; ++x)
	{
		for (int y = 0; y < ChunkSize; ++y)
		{
			for (int z = 0; z < ChunkSize; ++z)
			{
				const auto CurrentBlock = GetBlockAtInChunk(FIntVector3(x,y,z),  (*CompressedChunkBlocksPtr), ChunkSize);
				if (CurrentBlock.VoxelType != "Air" )
				{
					const FIntVector Directions[6] = {
						FIntVector(x+1,y,z),
						FIntVector(x,y+1,z),
						FIntVector(x-1,y,z),
						FIntVector(x,y-1,z),
						FIntVector(x,y,z+1),
						FIntVector(x,y,z-1)							
					};
						
					for (int i = 0; i < 6; ++i)
					{
						if (Directions[i].X >= 0 && Directions[i].X < ChunkSize && Directions[i].Y >= 0 && Directions[i].Y < ChunkSize && Directions[i].Z >= 0 && Directions[i].Z < ChunkSize) 
						{
							const auto CurrentNeighbour = GetBlockAtInChunk(FIntVector3(Directions[i].X,Directions[i].Y,Directions[i].Z),  (*CompressedChunkBlocksPtr), ChunkSize);
							if (CurrentNeighbour.IsTransparent == true && !CompareVoxels(CurrentNeighbour, CurrentBlock) ) 
							{
								QuadsData.Add(FIntVector4(x,y,z,i), CurrentBlock);
							}
						}
					}
				}
			}
		}
	}

	ChunkQuadsToLoad->Enqueue(MakeTuple(Coordinates, QuadsData));
	PreCookedChunksToLoadBlockData->Enqueue(MakeTuple(Coordinates, CompressedChunkBlocksPtr));
}

static int32 Modulo(int32 Number, int32 N) //Custom function to compute class of a nulbr in Z/NZ
{
	if (Number >= 0)
	{
		return Number % N;
	}
	
	return N + Number % N;
}

static FIntVector NormaliseCyclicalCoordinates(FIntVector Coordinates, int32 Norm)
{
	return FIntVector( Modulo(Coordinates.X,  Norm), Modulo(Coordinates.Y , Norm), Modulo(Coordinates.Z , Norm));
}

static void ComputeChunkSideFacesFromData(FChunkData* DataOfChunkToAddFacesTo, FChunkData* NeighbourChunkBlocks, int32 DirectionIndex, TQueue< TTuple<FIntVector, TMap<FIntVector4, FVoxel>>, EQueueMode::Mpsc>* OuputChunkQuadsQueue, const int32 ChunkSize, FIntVector ChunkToAddFacesToCoordinates)
{

	const FIntVector Directions[6] = {
		FIntVector(1,0,0),
		FIntVector(0,1,0),
		FIntVector(-1,0,0),
		FIntVector(0,-1,0),
		FIntVector(0,0,1),
		FIntVector(0,0,-1)							
	};

	const FIntVector LoopOrigin[6] = {
		FIntVector(ChunkSize - 1,0,0),
		FIntVector(0,ChunkSize - 1,0),
		FIntVector(0,0,0),
		FIntVector(0,0,0),
		FIntVector(0,0,ChunkSize - 1),
		FIntVector(0,0,0)
	};
	
	const FIntVector LoopFirstVectors[6] = {
		FIntVector(0,1,0),
		FIntVector(1,0,0),
		FIntVector(0,1,0),
		FIntVector(1,0,0),
		FIntVector(1,0,0),
		FIntVector(1,0,0)	
	};

	const FIntVector LoopSecondVectors[6] = {
		FIntVector(0,0,1),
		FIntVector(0,0,1),
		FIntVector(0,0,1),
		FIntVector(0,0,1),
		FIntVector(0,1,0),
		FIntVector(0,1,0)	
	};

	const auto LoopXDirection = LoopFirstVectors[DirectionIndex];
	const auto LoopYDirection = LoopSecondVectors[DirectionIndex];

	//Generate the chunk's side's quads data

	TMap<FIntVector4, FVoxel> SideQuadsData;
		
	for (int x = 0; x < ChunkSize; ++x)
	{
		for (int y = 0; y < ChunkSize; ++y)
		{
			const auto CurrentBlockLocation = LoopXDirection*x + LoopYDirection*y + LoopOrigin[DirectionIndex];
			const auto CurrentNeighbourLocation = NormaliseCyclicalCoordinates(CurrentBlockLocation + Directions[DirectionIndex], ChunkSize) ; //There is probably a problem there as the compression-decompression and the geometry generation logic seem to be working
			if (GetBlockAtInChunk(CurrentBlockLocation, *DataOfChunkToAddFacesTo, ChunkSize ).VoxelType != "Air" && GetBlockAtInChunk(CurrentNeighbourLocation, *NeighbourChunkBlocks, ChunkSize ).IsTransparent == true && !CompareVoxels(GetBlockAtInChunk(CurrentNeighbourLocation, *NeighbourChunkBlocks, ChunkSize ), GetBlockAtInChunk(CurrentBlockLocation, *DataOfChunkToAddFacesTo, ChunkSize )))
			{
				SideQuadsData.Add(FIntVector4(CurrentBlockLocation.X,CurrentBlockLocation.Y, CurrentBlockLocation.Z , DirectionIndex), GetBlockAtInChunk(CurrentBlockLocation, *DataOfChunkToAddFacesTo, ChunkSize ));
			}
		}
	}
	
	OuputChunkQuadsQueue->Enqueue(MakeTuple(ChunkToAddFacesToCoordinates,SideQuadsData));
}