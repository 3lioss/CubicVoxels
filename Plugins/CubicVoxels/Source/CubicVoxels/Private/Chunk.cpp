// Fill out your copyright notice in the Description page of Project Settings.

#include "..\Public\Chunk.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "VoxelStructs.h"
#include "Engine/DataTable.h"
#include "ThreadedWorldGeneration/VoxelChunkThreadingUtilities.h"
#include "VoxelWorld.h"

// Sets default values
AChunk::AChunk()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>("Physical mesh");
	SetRootComponent(Mesh);
	Mesh->bUseAsyncCooking = true;
	bReplicates = true;

	IsInsideGeometryLoaded = false;

	VoxelCharacteristicsData = ConstructorHelpers::FObjectFinder<UDataTable>(TEXT("/Script/Engine.DataTable'/CubicVoxels/DefaultVoxelCharacteistics.DefaultVoxelCharacteistics'")).Object;

}

void AChunk::LoadBlocks(TSharedPtr<FChunkData> InputVoxelData)
{
	/*Sets the voxel data of the chunk */ 
	BlocksDataPtr = InputVoxelData;
}

void AChunk::AddQuads(TMap<FIntVector4, FVoxel> VoxelQuadsToAdd)
{
	/*Adds faces to the chunk */
	VoxelQuads.Append(VoxelQuadsToAdd);
}

bool AChunk::HasQuadAt(FIntVector4 QuadLocation)
{
	/*Tests if there is a face at the given location*/
	return VoxelQuads.Contains(QuadLocation);
}

void AChunk::RemoveQuad(FIntVector4 Quad)
{
	/*Removes a face at the given location*/
	VoxelQuads.Remove(Quad);
}

void AChunk::ShowFaceGenerationStatus()
{
	if (IsInsideGeometryLoaded)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Inside geometry loaded"));	
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Inside geometry not loaded"));	
	}

	if (IsSideGeometryLoaded[0])
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Side 0 geometry loaded"));	
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Side 0 geometry not loaded"));	
	}

	if (IsSideGeometryLoaded[1])
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Side 1 geometry loaded"));	
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Side 1 geometry not loaded"));	
	}

	if (IsSideGeometryLoaded[2])
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Side 2 geometry loaded"));	
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Side 2 geometry not loaded"));	
	}

	if (IsSideGeometryLoaded[3])
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Side 3 geometry loaded"));	
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Side 3 geometry not loaded"));	
	}

	if (IsSideGeometryLoaded[4])
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Side 4 geometry loaded"));	
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Side 4 geometry not loaded"));	
	}
	
	if (IsSideGeometryLoaded[5])
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Side 5 geometry loaded"));	
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Side 5 geometry not loaded"));	
	}
}

void AChunk::BeginPlay()
{
	Super::BeginPlay();
}

bool AChunk::IsInsideChunk(FIntVector BlockLocation)
{
	return (BlockLocation.X < ChunkSize && BlockLocation.Y < ChunkSize && BlockLocation.Z < ChunkSize && BlockLocation.X >= 0 && BlockLocation.Y >= 0 && BlockLocation.Z >= 0);
}



void AChunk::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AChunk::RenderChunk(float VoxelSize)
{
	/*Creates the procedural mesh of the chunk based on its quads data*/
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

	TMap<FName, ProcMeshSectionDataWrapper>  VoxelTypeToMeshSectionMap;
	
	int32 NumberOfAlreadyCreatedSections = 0;
	
	for (auto& VoxelQuad : VoxelQuads)
	{
		const auto CurrentVoxelType = VoxelQuad.Value.VoxelType;
		
		if (!VoxelTypeToMeshSectionMap.Contains(CurrentVoxelType))
		{
			auto MeshSection = ProcMeshSectionDataWrapper();

			MeshSection.SectionIndex = NumberOfAlreadyCreatedSections;
			NumberOfAlreadyCreatedSections +=1;

			MeshSection.IsSolid = VoxelQuad.Value.IsSolid;

			VoxelTypeToMeshSectionMap.Add(VoxelQuad.Value.VoxelType, MeshSection);
			
		}

		if (auto MeshSectionDataPtr = VoxelTypeToMeshSectionMap.Find(CurrentVoxelType))
		{
			for (int j = 0; j < 4; ++j)
			{
				MeshSectionDataPtr->Vertices.Add(VoxelSize*localBlockVertexData[LocalBlockTriangleData[j + VoxelQuad.Key.W * 4]] + VoxelSize*FVector(VoxelQuad.Key.X,VoxelQuad.Key.Y, VoxelQuad.Key.Z ) );
			}
									
			MeshSectionDataPtr->UVs.Append({FVector2d(1,1), FVector2d(1,0), FVector2d(0,0), FVector2d(0,1)});
			const auto CurrentVertexCount = MeshSectionDataPtr->VertexCount;
			MeshSectionDataPtr->Triangles.Append({CurrentVertexCount + 3, CurrentVertexCount + 2, CurrentVertexCount, CurrentVertexCount + 2, CurrentVertexCount + 1, CurrentVertexCount});
			MeshSectionDataPtr->VertexCount += 4;
		}
		
		
	}

	Mesh->ClearAllMeshSections();

	for (const auto& VoxelTypeMeshSectionPair : VoxelTypeToMeshSectionMap)
	{
		Mesh->CreateMeshSection(VoxelTypeMeshSectionPair.Value.SectionIndex, VoxelTypeMeshSectionPair.Value.Vertices, VoxelTypeMeshSectionPair.Value.Triangles, VoxelTypeMeshSectionPair.Value.Normals, VoxelTypeMeshSectionPair.Value.UVs, VoxelTypeMeshSectionPair.Value.VertexColors, VoxelTypeMeshSectionPair.Value.Tangents, VoxelTypeMeshSectionPair.Value.IsSolid);
		if (const auto VoxelCharacteristics = VoxelCharacteristicsData->FindRow<FVoxelCharacteristics>(VoxelTypeMeshSectionPair.Key, TEXT("Retrieving recently created chunk section's material") ))
		{
			Mesh->SetMaterial(VoxelTypeMeshSectionPair.Value.SectionIndex, VoxelCharacteristics->VoxelMaterial);
		}

	}
	FChunkData::Compress(*BlocksDataPtr);

}

void AChunk::DestroyBlockAt(FVector BlockWorldLocation)
{
	/*Destroys the block at the given world location*/
	const auto BlockLocation = FloorVector(BlockWorldLocation - GetActorLocation())/(DefaultVoxelSize);
	
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
			const auto NeighboringVoxel = BlocksDataPtr->GetVoxelAt(Neighbors[i]);
			if ((NeighboringVoxel != DefaultVoxel) && !VoxelQuads.Contains(FIntVector4(Neighbors[i].X, Neighbors[i].Y, Neighbors[i].Z, OppositeDirections[i]))) // TO DO: modify this check to be able to delete transparent blocks
			{
				VoxelQuads.Add(FIntVector4(Neighbors[i].X, Neighbors[i].Y, Neighbors[i].Z, OppositeDirections[i]), NeighboringVoxel);
			}
		}
		else
		{
			if (const auto NeighborPtr = OwningWorld->GetActorOfLoadedChunk(NeighboringChunks[i]))
			{
				const auto NeighboringVoxel = NeighborPtr->BlocksDataPtr->GetVoxelAt(NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize));
				if ((NeighboringVoxel != DefaultVoxel) && !NeighborPtr->HasQuadAt(FIntVector4(Neighbors[i].X, Neighbors[i].Y, Neighbors[i].Z, OppositeDirections[i])))
				{

					auto Temp = TMap<FIntVector4, FVoxel>();
					Temp.Add(FIntVector4(NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize).X, NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize).Y, NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize).Z, OppositeDirections[i]), NeighboringVoxel);
					NeighborPtr->AddQuads( Temp);
					NeighborPtr->RenderChunk(DefaultVoxelSize);	
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Neighborhing chunk doesn't exist"))
			}
		}
	}
	BlocksDataPtr->RemoveVoxel(BlockLocation);
	RenderChunk(DefaultVoxelSize);
	
}

void AChunk::SetBlockAt(FVector BlockWorldLocation, FVoxel BlockType)
{
	/*Sets the block at the given world location*/
	auto RelativeLocation = (BlockWorldLocation - GetActorLocation())/(DefaultVoxelSize);
	const auto BlockLocation = FIntVector(FMath::Floor(RelativeLocation.X), FMath::Floor(RelativeLocation.Y), FMath::Floor(RelativeLocation.Z));
	
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
			const auto NeighboringVoxel = BlocksDataPtr->GetVoxelAt(Neighbors[i]);
			if ((NeighboringVoxel.IsTransparent) && !VoxelQuads.Contains(FIntVector4(Neighbors[i].X, Neighbors[i].Y, Neighbors[i].Z, i)) && (!BlockType.IsTransparent || (NeighboringVoxel == BlockType)))
			{
				VoxelQuads.Remove(FIntVector4(Neighbors[i].X, Neighbors[i].Y, Neighbors[i].Z, OppositeDirections[i]));
			}
		}
		else
		{
			if (const auto NeighborPtr = OwningWorld->GetActorOfLoadedChunk(NeighboringChunks[i]))
			{
				const auto NeighboringVoxel = NeighborPtr->BlocksDataPtr->GetVoxelAt(NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize));
				
				if ((NeighboringVoxel.IsTransparent) && !NeighborPtr->HasQuadAt(FIntVector4(Neighbors[i].X, Neighbors[i].Y, Neighbors[i].Z, i)) && (!BlockType.IsTransparent || (NeighboringVoxel == BlockType)))
				{
					NeighborPtr-> RemoveQuad(FIntVector4(NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize).X, NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize).Y, NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize).Z, OppositeDirections[i]));
					NeighborPtr->RenderChunk(DefaultVoxelSize);
				}
			}
		}
	}

	BlocksDataPtr->SetVoxel(BlockLocation, BlockType);
	
	RenderChunk(DefaultVoxelSize);
	
}

FVoxel AChunk::GetBlockAt(FVector BlockWorldLocation)
{
	
	/*Sets the block at the given world location*/
	auto RelativeLocation = (BlockWorldLocation - GetActorLocation())/(DefaultVoxelSize);
	const auto BlockLocation = FIntVector(FMath::Floor(RelativeLocation.X), FMath::Floor(RelativeLocation.Y), FMath::Floor(RelativeLocation.Z));

	return BlocksDataPtr->GetVoxelAt(BlockLocation);
	
}
