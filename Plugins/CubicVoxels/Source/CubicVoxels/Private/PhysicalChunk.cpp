// Fill out your copyright notice in the Description page of Project Settings.

#include "PhysicalChunk.h"
#include "ProceduralMeshComponent.h"
#include "VoxelWorldDataSaveGame.h"
#include "VoxelStructs.h"
#include "Engine/DataTable.h"
#include "VoxelChunkThreadingUtilities.h"
#include "VoxelWorld.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
APhysicalChunk::APhysicalChunk()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>("Physical mesh");
	SetRootComponent(Mesh);
	Mesh->bUseAsyncCooking = true;
	
}

void APhysicalChunk::LoadBlocks(TSharedPtr<FChunkData> InputVoxelData)
{
	BlocksData = InputVoxelData;
}

void APhysicalChunk::AddQuads(TMap<FIntVector4, FVoxel> VoxelQuadsToAdd)
{
	VoxelQuads.Append(VoxelQuadsToAdd);
}

bool APhysicalChunk::HasQuadAt(FIntVector4 QuadLocation)
{
		return VoxelQuads.Contains(QuadLocation);
}

void APhysicalChunk::RemoveQuad(FIntVector4 Quad)
{
	VoxelQuads.Remove(Quad);
}

// Called when the game starts or when spawned
void APhysicalChunk::BeginPlay()
{
	Super::BeginPlay();
}

bool APhysicalChunk::IsInsideChunk(FIntVector BlockLocation)
{
	return (BlockLocation.X < ChunkSize && BlockLocation.Y < ChunkSize && BlockLocation.X < ChunkSize && BlockLocation.X >= 0 && BlockLocation.Y >= 0 && BlockLocation.Z >= 0);
}


void APhysicalChunk::RemoveBlockFromBlockData(FIntVector3 BlockLocation) //TO DO: Add locking mechanism to avoid race condition with chunk side generation
{
	
	auto BlockIndex = BlockLocation.X*ChunkSize*ChunkSize + BlockLocation.Y*ChunkSize + BlockLocation.Z;

	int32 i = 0;
	
	while ((BlocksData->ChunkData)[i].StackSize - 1 < BlockIndex)
	{
		BlockIndex -= (BlocksData->ChunkData)[i].StackSize;
		i += 1;
	}
	
	
	if (!CompareVoxels((BlocksData->ChunkData)[i].Voxel, DefaultVoxel) )
	{
		if (BlockIndex == 0)
		{
			(BlocksData->ChunkData)[i].StackSize -= 1;
			BlocksData->ChunkData.Insert(MakeStack(DefaultVoxel, 1), i);
		}
		else
		{
			if (BlockIndex == (BlocksData->ChunkData)[i].StackSize - 1)
			{
				if (i+1 < BlocksData->ChunkData.Num())
				{
					if (CompareVoxels((BlocksData->ChunkData)[i+1].Voxel, DefaultVoxel) )
					{
						(BlocksData->ChunkData)[i].StackSize -= 1;
						(BlocksData->ChunkData)[i+1].StackSize += 1;
					}
					else
					{
						(BlocksData->ChunkData)[i].StackSize -= 1;
						BlocksData->ChunkData.Insert(MakeStack(DefaultVoxel, 1), i+1);
					}
				}
				else
				{
					(BlocksData->ChunkData)[i].StackSize -= 1;
					BlocksData->ChunkData.Add(MakeStack(DefaultVoxel, 1));
				}
			}
			else
			{
				const auto Temp = (BlocksData->ChunkData)[i].StackSize;
				(BlocksData->ChunkData)[i].StackSize = BlockIndex;
				if (i+1 < BlocksData->ChunkData.Num())
				{
					BlocksData->ChunkData.Insert(MakeStack((BlocksData->ChunkData)[i].Voxel, Temp - BlockIndex - 1), i+1);
					BlocksData->ChunkData.Insert(MakeStack(DefaultVoxel, 1), i+1);
				}
				else
				{
					BlocksData->ChunkData.Add(MakeStack(DefaultVoxel, 1));
					BlocksData->ChunkData.Add(MakeStack((BlocksData->ChunkData)[i].Voxel, Temp - BlockIndex - 1));
				}
			}
			
		}
	}
		
}

void APhysicalChunk::SetBlockInBlockData(FIntVector3 BlockLocation, FVoxel BlockVoxel)
{
	auto BlockIndex = BlockLocation.X*ChunkSize*ChunkSize + BlockLocation.Y*ChunkSize + BlockLocation.Z;

	int32 i = 0;
	
	while ((BlocksData->ChunkData)[i].StackSize - 1 < BlockIndex)
	{
		BlockIndex -= (BlocksData->ChunkData)[i].StackSize;
		i += 1;
	}
	
	
	if (!CompareVoxels((BlocksData->ChunkData)[i].Voxel, BlockVoxel) )
	{
		if (BlockIndex == 0)
		{
			(BlocksData->ChunkData)[i].StackSize -= 1;
			BlocksData->ChunkData.Insert(MakeStack(BlockVoxel, 1), i);
		}
		else
		{
			if (BlockIndex == (BlocksData->ChunkData)[i].StackSize - 1)
			{
				if (i+1 < BlocksData->ChunkData.Num())
				{
					if (CompareVoxels((BlocksData->ChunkData)[i+1].Voxel, BlockVoxel) )
					{
						(BlocksData->ChunkData)[i].StackSize -= 1;
						(BlocksData->ChunkData)[i+1].StackSize += 1;
					}
					else
					{
						(BlocksData->ChunkData)[i].StackSize -= 1;
						BlocksData->ChunkData.Insert(MakeStack(BlockVoxel, 1), i+1);
					}
				}
				else
				{
					(BlocksData->ChunkData)[i].StackSize -= 1;
					BlocksData->ChunkData.Add(MakeStack(BlockVoxel, 1));
				}
			}
			else
			{
				const auto Temp = (BlocksData->ChunkData)[i].StackSize;
				(BlocksData->ChunkData)[i].StackSize = BlockIndex;
				if (i+1 < BlocksData->ChunkData.Num())
				{
					BlocksData->ChunkData.Insert(MakeStack((BlocksData->ChunkData)[i].Voxel, Temp - BlockIndex - 1), i+1);
					BlocksData->ChunkData.Insert(MakeStack(BlockVoxel, 1), i+1);
				}
				else
				{
					BlocksData->ChunkData.Add(MakeStack(BlockVoxel, 1));
					BlocksData->ChunkData.Add(MakeStack((BlocksData->ChunkData)[i].Voxel, Temp - BlockIndex - 1));
				}
			}
			
		}
	}
}


// Called every frame
void APhysicalChunk::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APhysicalChunk::SaveChunkDataToDisk()
{
	auto SaveObject = Cast<UVoxelWorldDataSaveGame>(UGameplayStatics::CreateSaveGameObject(UVoxelWorldDataSaveGame::StaticClass()));

	SaveObject->ChunkData.SetNum(BlocksData->ChunkData.Num());
	for (int i = 0; i < BlocksData->ChunkData.Num(); i++)
	{
		
		SaveObject->ChunkData[i] = BlocksData->ChunkData[i];
	}
	
	FString ChunkCoordinates = "";
	ChunkCoordinates.AppendInt(Location.X);
	ChunkCoordinates.Append(",");
	ChunkCoordinates.AppendInt(Location.Y);
	ChunkCoordinates.Append(",");
	ChunkCoordinates.AppendInt(Location.Z);
	const FString& SaveSlotName = ChunkCoordinates;
	
	UGameplayStatics::SaveGameToSlot(SaveObject, SaveSlotName, 0 );
	
}


void APhysicalChunk::RenderChunk(float VoxelSize)
{
	const  FVector localBlockVertexData[8] = {
		FVector(1,1,1),
		FVector(1,0,1),
		FVector(1,0,0),
		FVector(1,1,0),
		FVector(0,0,1),
		FVector(0,1,1),
		FVector(0,1,0),
		FVector(0,0,0)
	};

	const int LocalBlockTriangleData[24] = {
		0,1,2,3, // Forward
		5,0,3,6, // Right
		4,5,6,7, // Back
		1,4,7,2, // Left
		5,4,1,0, // Up
		3,2,7,6  // Down  
	};

	TMap<FName, TArray<FVector>>  VertexData;
	TMap<FName, TArray<FVector>>  NormalsData;
	TMap<FName, TArray<int32>> TriangleData;
	TMap<FName, TArray<FVector2d>> UVData;
	TMap<FName, TArray<FColor>> VertexColors;
	TMap<FName, TArray<FProcMeshTangent>> Tangents;

	TMap<FName, int32> SectionIndices;
	TMap<FName, bool> VoxelPhysicsParameters;
	int32 NumberOfSections = 0;

	TMap<FName, int32> VertexCounts;

	for (auto& VoxelQuad : VoxelQuads)
	{
		const auto CurrentVoxelType = VoxelQuad.Get<1>().VoxelType; //TODO: replace the voxel type with an FName that relates to a material. The FName is related to its material through a data table, which serves as a "texture pack"
		
		if (!SectionIndices.Contains(CurrentVoxelType))
		{
			SectionIndices.Add( CurrentVoxelType, NumberOfSections);
			VoxelPhysicsParameters.Add(CurrentVoxelType, VoxelQuad.Get<1>().IsSolid);
			VertexCounts.Add(CurrentVoxelType, 0);
			NumberOfSections += 1;
			
			VertexData.Add(CurrentVoxelType,  TArray<FVector>());
			NormalsData.Add(CurrentVoxelType, TArray<FVector>() );
			TriangleData.Add(CurrentVoxelType, TArray<int32>() );
			UVData.Add(CurrentVoxelType, TArray<FVector2d>() );
			VertexColors.Add(CurrentVoxelType, TArray<FColor>() );
			Tangents.Add(CurrentVoxelType, TArray<FProcMeshTangent>() );

			VertexCounts.Add(CurrentVoxelType, 0);
			
		}
		
		for (int j = 0; j < 4; ++j)
		{
			VertexData.Find(CurrentVoxelType)->Add(VoxelSize*localBlockVertexData[LocalBlockTriangleData[j + VoxelQuad.Get<0>().W * 4]] + VoxelSize*FVector(VoxelQuad.Get<0>().X,VoxelQuad.Get<0>().Y, VoxelQuad.Get<0>().Z ) );
		}
									
		UVData.Find(CurrentVoxelType)->Append({FVector2d(1,1), FVector2d(1,0), FVector2d(0,0), FVector2d(0,1)});
		const auto CurrentVertexCount = *VertexCounts.Find(CurrentVoxelType);
		TriangleData.Find(CurrentVoxelType)->Append({CurrentVertexCount + 3, CurrentVertexCount + 2, CurrentVertexCount, CurrentVertexCount + 2, CurrentVertexCount + 1, CurrentVertexCount});
		*VertexCounts.Find(CurrentVoxelType) += 4;
	}

	Mesh->ClearAllMeshSections();

	for (auto& VoxelType : SectionIndices)
	{
		const auto CurrentSectionVoxelType = VoxelType.Get<0>();
		auto CurrentSectionPhysicsPtr = VoxelPhysicsParameters.Find(VoxelType.Get<0>());
		bool IsSectionSolid = true;

		if (CurrentSectionPhysicsPtr)
		{
			IsSectionSolid = *CurrentSectionPhysicsPtr;
		}
		
		Mesh->CreateMeshSection(VoxelType.Get<1>(), *VertexData.Find(CurrentSectionVoxelType), *TriangleData.Find(CurrentSectionVoxelType), *NormalsData.Find(CurrentSectionVoxelType), *UVData.Find(CurrentSectionVoxelType), *VertexColors.Find(CurrentSectionVoxelType), *Tangents.Find(CurrentSectionVoxelType), IsSectionSolid );

		const auto VoxelCharacteristics = VoxelCharacteristicsData->FindRow<FVoxelCharacteristics>(VoxelType.Get<0>(), TEXT("Retrieving recently created chunk section's material") );
		if (VoxelCharacteristics)
		{
			Mesh->SetMaterial(VoxelType.Get<1>(), VoxelCharacteristics->VoxelMaterial);
			
		}
		
	}	
}

void APhysicalChunk::DestroyBlockAt(FVector BlockWorldLocation)
{
	auto BlockLocation = FIntVector( (BlockWorldLocation - GetActorLocation())/(DefaultVoxelSize) );

	const FIntVector Neighbors[6] = {
		FIntVector(BlockLocation.X+1,BlockLocation.Y,BlockLocation.Z),
		FIntVector(BlockLocation.X,BlockLocation.Y+1,BlockLocation.Z),
		FIntVector(BlockLocation.X-1,BlockLocation.Y,BlockLocation.Z),
		FIntVector(BlockLocation.X,BlockLocation.Y-1,BlockLocation.Z),
		FIntVector(BlockLocation.X,BlockLocation.Y,BlockLocation.Z+1),
		FIntVector(BlockLocation.X,BlockLocation.Y,BlockLocation.Z-1)							
	};

	const FIntVector NeighboringChunks[6] = {
		FIntVector(Location.X+1,Location.Y,Location.Z),
		FIntVector(Location.X,Location.Y+1,Location.Z),
		FIntVector(Location.X-1,Location.Y,Location.Z),
		FIntVector(Location.X,Location.Y-1,Location.Z),
		FIntVector(Location.X,Location.Y,Location.Z+1),
		FIntVector(Location.X,Location.Y,Location.Z-1)							
	};

	const int32 OppositeDirections[6] = {
		2,
		3,
		0,
		1,
		5,
		4							
	};
		
	//Remove the quads associated to the block and add new ones
	for (int32 i = 0; i < 6; ++i)
	{
		VoxelQuads.Remove(FIntVector4(BlockLocation.X, BlockLocation.Y, BlockLocation.Z, i));
		if (IsInsideChunk(Neighbors[i]))
		{
			const auto NeighboringVoxel = GetBlockAtInChunk(Neighbors[i], *BlocksData, ChunkSize);
			if (!CompareVoxels(NeighboringVoxel,DefaultVoxel) && !VoxelQuads.Contains(FIntVector4(Neighbors[i].X, Neighbors[i].Y, Neighbors[i].Z, OppositeDirections[i]))) // TO DO: modify this check to be able to delete transparent blocks
			{
				VoxelQuads.Add(FIntVector4(Neighbors[i].X, Neighbors[i].Y, Neighbors[i].Z, OppositeDirections[i]), NeighboringVoxel);
			}
		}
		else
		{
			if (OwningWorld->IsChunkLoaded(NeighboringChunks[i]))
			{
				const auto NeighborPtr = OwningWorld->GetChunkAt(NeighboringChunks[i]);
				const auto NeighboringVoxel = GetBlockAtInChunk(NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize), *(NeighborPtr->BlocksData), ChunkSize);
				if (!CompareVoxels(NeighboringVoxel, DefaultVoxel) && !NeighborPtr->HasQuadAt(FIntVector4(Neighbors[i].X, Neighbors[i].Y, Neighbors[i].Z, OppositeDirections[i])))
				{
					auto Temp = TMap<FIntVector4, FVoxel>();
					Temp.Add(FIntVector4(NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize).X, NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize).Y, NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize).Z, OppositeDirections[i]), NeighboringVoxel);
					NeighborPtr->AddQuads( Temp);
					NeighborPtr->RenderChunk(DefaultVoxelSize);	
				}
			}
		}
	}
		
	RemoveBlockFromBlockData(BlockLocation); 
	
	RenderChunk(DefaultVoxelSize);	

	OwningWorld->SetChunkSavedData(Location, *BlocksData);
}

void APhysicalChunk::SetBlockAt(FVector BlockWorldLocation, FVoxel BlockType)
{
	auto BlockLocation = FIntVector( (BlockWorldLocation - GetActorLocation())/(DefaultVoxelSize) );

	const FIntVector Neighbors[6] = {
		FIntVector(BlockLocation.X+1,BlockLocation.Y,BlockLocation.Z),
		FIntVector(BlockLocation.X,BlockLocation.Y+1,BlockLocation.Z),
		FIntVector(BlockLocation.X-1,BlockLocation.Y,BlockLocation.Z),
		FIntVector(BlockLocation.X,BlockLocation.Y-1,BlockLocation.Z),
		FIntVector(BlockLocation.X,BlockLocation.Y,BlockLocation.Z+1),
		FIntVector(BlockLocation.X,BlockLocation.Y,BlockLocation.Z-1)							
	};

	const FIntVector NeighboringChunks[6] = {
		FIntVector(Location.X+1,Location.Y,Location.Z),
		FIntVector(Location.X,Location.Y+1,Location.Z),
		FIntVector(Location.X-1,Location.Y,Location.Z),
		FIntVector(Location.X,Location.Y-1,Location.Z),
		FIntVector(Location.X,Location.Y,Location.Z+1),
		FIntVector(Location.X,Location.Y,Location.Z-1)							
	};

	const int32 OppositeDirections[6] = {
		2,
		3,
		0,
		1,
		5,
		4							
	};
		
	//Add the quads associated to the new block and remove old ones
	for (int32 i = 0; i < 6; ++i)
	{
		VoxelQuads.Add(FIntVector4(BlockLocation.X, BlockLocation.Y, BlockLocation.Z, i), BlockType);
		
		if (IsInsideChunk(Neighbors[i]))
		{
			const auto NeighboringVoxel = GetBlockAtInChunk(Neighbors[i], *BlocksData, ChunkSize);
			if (CompareVoxels(NeighboringVoxel, DefaultVoxel) && !VoxelQuads.Contains(FIntVector4(Neighbors[i].X, Neighbors[i].Y, Neighbors[i].Z, i)) && (!BlockType.IsTransparent || CompareVoxels(NeighboringVoxel, BlockType)))
			{
				VoxelQuads.Remove(FIntVector4(Neighbors[i].X, Neighbors[i].Y, Neighbors[i].Z, OppositeDirections[i]));
			}
		}
		else
		{
			if (OwningWorld->IsChunkLoaded(NeighboringChunks[i]))
			{
				const auto NeighborPtr = OwningWorld->GetChunkAt(NeighboringChunks[i]);
				const auto NeighboringVoxel = GetBlockAtInChunk(NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize), *(NeighborPtr->BlocksData), ChunkSize);
				
				if (CompareVoxels(NeighboringVoxel,DefaultVoxel) && !NeighborPtr->HasQuadAt(FIntVector4(Neighbors[i].X, Neighbors[i].Y, Neighbors[i].Z, i)) && (!BlockType.IsTransparent || CompareVoxels(NeighboringVoxel, BlockType)))
				{
					NeighborPtr->RemoveQuad(FIntVector4(NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize).X, NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize).Y, NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize).Z, OppositeDirections[i]));
					NeighborPtr->RenderChunk(DefaultVoxelSize);
				}
			}
		}
	}
		
	SetBlockInBlockData(BlockLocation, BlockType); 
	
	RenderChunk(DefaultVoxelSize);
	
	OwningWorld->SetChunkSavedData(Location, *BlocksData);	
}


