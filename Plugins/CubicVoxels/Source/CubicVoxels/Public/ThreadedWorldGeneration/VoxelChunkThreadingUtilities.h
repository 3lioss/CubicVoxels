#pragma once
#include "VoxelStructs.h"
#include "GlobalPluginParameters.h"

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
	
static void GenerateChunkDataAndComputeInsideFaces(FIntVector Coordinates, TQueue< TTuple<FIntVector, TSharedPtr<FChunkData>>, EQueueMode::Mpsc>* PreCookedChunksToLoadBlockData, TQueue< TSharedPtr<FChunkGeometry>, EQueueMode::Mpsc>* ChunkGeometryLoadingQueuePtr,  FVoxel (*GenerationFunction) (FVector))
{
	/*Function to generate procedurally a chunk and its mesh data*/
	
	// auto StartTime = FDateTime::UtcNow(); 
	
	//Create the variable that will store voxel data
	
	const TSharedPtr<FChunkData> ChunkDataPtr(new FChunkData);

	//Fill the arrays with the chunk's voxels
	for (int32 x = 0; x < ChunkSize; x++)
	{
		for (int32 y = 0; y < ChunkSize; y++)
		{
			for (int32 z = 0; z < ChunkSize; z++)
			{
				auto const Position = DefaultVoxelSize*FVector(x + ChunkSize*Coordinates.X, y + ChunkSize*Coordinates.Y , z + ChunkSize*Coordinates.Z);
					
				ChunkDataPtr->SetVoxel(x,y,z, (*GenerationFunction)(Position));
					
			}
		}
	}
	

	// float TimeElapsedInMs = (FDateTime::UtcNow() - StartTime).GetTotalMilliseconds(); 
	// UE_LOG(LogTemp, Warning, TEXT("Time taken to generate: %f"), TimeElapsedInMs);
	// StartTime = FDateTime::UtcNow(); 
		
	//Generate the chunk's quads data
	TMap<FIntVector4, FVoxel> QuadsData;
	
	for (int x = 0; x < ChunkSize; ++x)
	{
		for (int y = 0; y < ChunkSize; ++y)
		{
			for (int z = 0; z < ChunkSize; ++z)
			{
				
				if (ChunkDataPtr->GetVoxelAt(x,y,z).VoxelType != "Air" ) //TODO: replace with a more robust test
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
							if (ChunkDataPtr->GetVoxelAt(Directions[i]).IsTransparent == true && ChunkDataPtr->GetVoxelAt(Directions[i]) != ChunkDataPtr->GetVoxelAt(x,y,z) ) 
							{
								QuadsData.Add(FIntVector4(x,y,z,i), ChunkDataPtr->GetVoxelAt(x,y,z));
							}
						}
					}
				}
			}
		}
	}
	// TimeElapsedInMs = (FDateTime::UtcNow() - StartTime).GetTotalMilliseconds(); //to remove later
	// UE_LOG(LogTemp, Warning, TEXT("Time taken to mesh: %f"), TimeElapsedInMs);

	auto GeneratedGeometry = MakeShared<FChunkGeometry>();
	GeneratedGeometry->ChunkLocation = Coordinates;
	GeneratedGeometry->Geometry = QuadsData;
	GeneratedGeometry->DirectionIndex = -1;
	ChunkGeometryLoadingQueuePtr->Enqueue(GeneratedGeometry);
	PreCookedChunksToLoadBlockData->Enqueue(MakeTuple(Coordinates, ChunkDataPtr));
}

static void GenerateUnloadedDataAndComputeInsideFaces(FIntVector Coordinates, TQueue< TTuple<FIntVector, TSharedPtr<FChunkData>>, EQueueMode::Mpsc>* PreCookedChunksToLoadBlockData, TQueue< TSharedPtr<FChunkGeometry>, EQueueMode::Mpsc>* ChunkGeometryLoadingQueuePtr,  FVoxel (*GenerationFunction) (FVector),  TSharedPtr<FChunkData> ChunkDataPtr)
{
	/*Generate a chunk defined additively based on the procedural generator, then generate its mesh data*/
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Blue, TEXT("Starting to generate from additive data"));	

	//Fill the arrays with the chunk's voxels
	for (int32 x = 0; x < ChunkSize; x++)
	{
		for (int32 y = 0; y < ChunkSize; y++)
		{
			for (int32 z = 0; z < ChunkSize; z++)
			{
				if (!ChunkDataPtr->GetVoxelAt(x,y,z))
				{
					auto const Position = DefaultVoxelSize*FVector(x + ChunkSize*Coordinates.X, y + ChunkSize*Coordinates.Y , z + ChunkSize*Coordinates.Z);
					
					ChunkDataPtr->SetVoxel(x,y,z, (*GenerationFunction)(Position));
				}
				
				// FString MyFooString = (ChunkDataPtr->GetVoxelAt(x,y,z).VoxelType).ToString();
				// const TCHAR* tchar_is_wchar_t_under_the_hood = *MyFooString;
				// UE_LOG(LogTemp, Display, TEXT("Generated a voxel of type %s"), tchar_is_wchar_t_under_the_hood )
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
				
				if (ChunkDataPtr->GetVoxelAt(x,y,z).VoxelType != "Air" ) //TODO: replace with a more robust test
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
							if (ChunkDataPtr->GetVoxelAt(Directions[i]).IsTransparent == true && ChunkDataPtr->GetVoxelAt(Directions[i]) != ChunkDataPtr->GetVoxelAt(x,y,z) ) 
							{
								QuadsData.Add(FIntVector4(x,y,z,i), ChunkDataPtr->GetVoxelAt(x,y,z));
							}
						}
					}
				}
			}
		}
	}

	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Blue, TEXT("Finished generating from additive data"));	
	auto GeneratedGeometry = MakeShared<FChunkGeometry>();
	GeneratedGeometry->ChunkLocation = Coordinates;
	GeneratedGeometry->Geometry = QuadsData;
	GeneratedGeometry->DirectionIndex = -1;
	ChunkGeometryLoadingQueuePtr->Enqueue(GeneratedGeometry);
	PreCookedChunksToLoadBlockData->Enqueue(MakeTuple(Coordinates, ChunkDataPtr));
}

static void ComputeInsideFacesOfLoadedChunk(FIntVector Coordinates, TQueue< TTuple<FIntVector, TSharedPtr<FChunkData>>, EQueueMode::Mpsc>* PreCookedChunksToLoadBlockData, TQueue< TSharedPtr<FChunkGeometry>, EQueueMode::Mpsc>* ChunkGeometryLoadingQueuePtr,  TSharedPtr<FChunkData> CompressedChunkBlocksPtr)
{
	/*Generate the mesh data of a chunk whose voxel data is already accessible*/
		
	//Generate the chunk's quads data
	TMap<FIntVector4, FVoxel> QuadsData;
	
	for (int x = 0; x < ChunkSize; ++x)
	{
		for (int y = 0; y < ChunkSize; ++y)
		{
			for (int z = 0; z < ChunkSize; ++z)
			{
				const auto CurrentBlock =  CompressedChunkBlocksPtr->GetVoxelAt(FIntVector3(x,y,z));

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
							const auto CurrentNeighbour = CompressedChunkBlocksPtr->GetVoxelAt(Directions[i]);
							if (CurrentNeighbour.IsTransparent == true && (CurrentNeighbour != CurrentBlock) ) 
							{
								QuadsData.Add(FIntVector4(x,y,z,i), CurrentBlock);
							}
						}
					}
				}
			}
		}
	}

	auto GeneratedGeometry = MakeShared<FChunkGeometry>();
	GeneratedGeometry->ChunkLocation = Coordinates;
	GeneratedGeometry->Geometry = QuadsData;
	GeneratedGeometry->DirectionIndex = -1;
	ChunkGeometryLoadingQueuePtr->Enqueue(GeneratedGeometry);
	PreCookedChunksToLoadBlockData->Enqueue(MakeTuple(Coordinates, CompressedChunkBlocksPtr));
}

static int32 Modulo(int32 Number, int32 N) 
{
	/*Function to compute a modulo in accordance with french mathematical standards*/
	
	if (Number >= 0)
	{
		return Number % N;
	}
	
	return N + Number % N;
}

static FIntVector NormaliseCyclicalCoordinates(FIntVector Coordinates, int32 Norm)
{
	/*Function to compute an integer vector modulo a given integer "norm"*/
	
	return FIntVector( Modulo(Coordinates.X,  Norm), Modulo(Coordinates.Y , Norm), Modulo(Coordinates.Z , Norm));
}

static void ComputeChunkSideFacesFromData(TSharedPtr<FChunkData> DataOfChunkToAddFacesTo, TSharedPtr<FChunkData> NeighbourChunkBlocks, int32 DirectionIndex, TQueue< TSharedPtr<FChunkGeometry>, EQueueMode::Mpsc>* ChunkGeometryLoadingQueuePtr, FIntVector ChunkToAddFacesToCoordinates)
{
	/*Function to compute the faces of a chunks' sides*/

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

	TMap<FIntVector4, FVoxel> SideGeometryData;
		
	for (int x = 0; x < ChunkSize; ++x)
	{
		for (int y = 0; y < ChunkSize; ++y)
		{
			const auto CurrentBlockLocation = LoopXDirection*x + LoopYDirection*y + LoopOrigin[DirectionIndex];
			const auto CurrentNeighbourLocation = NormaliseCyclicalCoordinates(CurrentBlockLocation + Directions[DirectionIndex], ChunkSize) ; //There is probably a problem there as the compression-decompression and the geometry generation logic seem to be working
			if (DataOfChunkToAddFacesTo->GetVoxelAt(CurrentBlockLocation).VoxelType != "Air" && NeighbourChunkBlocks->GetVoxelAt(CurrentNeighbourLocation).IsTransparent == true && (NeighbourChunkBlocks->GetVoxelAt(CurrentNeighbourLocation) != DataOfChunkToAddFacesTo->GetVoxelAt(CurrentBlockLocation) )) //TODO: modify so that no reference is made to air 
			{
				SideGeometryData.Add(FIntVector4(CurrentBlockLocation.X,CurrentBlockLocation.Y, CurrentBlockLocation.Z , DirectionIndex), DataOfChunkToAddFacesTo->GetVoxelAt(CurrentBlockLocation) );
			}
		}
	}
	
	
	auto GeneratedGeometry = MakeShared<FChunkGeometry>();
	GeneratedGeometry->ChunkLocation = ChunkToAddFacesToCoordinates;
	GeneratedGeometry->Geometry = SideGeometryData;
	GeneratedGeometry->DirectionIndex = DirectionIndex;
	ChunkGeometryLoadingQueuePtr->Enqueue(GeneratedGeometry);
}

