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

void AChunk::AddQuads(TMap<FIntVector, FVoxelGeometryElement> VoxelQuadsToAdd)
{
	/*Adds faces to the chunk */
	VoxelGeometry.Append(VoxelQuadsToAdd);
}

bool AChunk::HasQuadAt(FIntVector VoxelLocation, int32 DirectionIndex)
{
	/*Tests if there is a face at the given location*/
	const auto VoxelAtLocationPtr = VoxelGeometry.Find(Location);
	if (VoxelAtLocationPtr)
	{
		return (VoxelAtLocationPtr->GeometryShape & BaseGeometryShapes[DirectionIndex]) == BaseGeometryShapes[DirectionIndex];
	}
	return false;
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

void AChunk::InterpretVoxelStream(FName StreamType, const TArray<uint8>& VoxelStream)
{
	// if (StreamType == "Chunk replicated data")
	// {
	// 	FChunkReplicatedData DeserializedChunkReplicatedData;
	// 	FMemoryReader MemoryReader(VoxelStream, true);
	// 	FChunkReplicatedData::StaticStruct()->SerializeBin(MemoryReader, &DeserializedChunkReplicatedData);
	//
	// 	VoxelQuads = DeserializedChunkReplicatedData.ChunkGeometry.Geometry;
	// 	RenderChunk(DefaultVoxelSize);
	// }
}

void AChunk::BeginPlay()
{
	Super::BeginPlay();
}

void AChunk::AddGeometryAt(FIntVector VoxelLocation, FVoxelGeometryElement GeometryElement )
{
	if (const auto ExistingVoxelGeometry = VoxelGeometry.Find(VoxelLocation))
	{
		ExistingVoxelGeometry->GeometryShape = ExistingVoxelGeometry->GeometryShape | GeometryElement.GeometryShape;
	}
	else
	{
		VoxelGeometry.Add(VoxelLocation, GeometryElement);
	}
}

void AChunk::RemoveGeometryAt(FIntVector VoxelLocation, uint8 GeometryShape)
{
	if (const auto ExistingVoxelGeometry = VoxelGeometry.Find(VoxelLocation))
	{
		ExistingVoxelGeometry->GeometryShape = ExistingVoxelGeometry->GeometryShape & (255 - GeometryShape);

	}
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

	TMap<FName, ProcMeshSectionDataWrapper> VoxelTypeToMeshSectionMap;
	
	int32 NumberOfAlreadyCreatedSections = 0;
	
	for (auto& LocalizedVoxelGeometryElement : VoxelGeometry)
	{
		for (int32 DirectionIndex = 0; DirectionIndex < 6; DirectionIndex++)
		{
			if ((LocalizedVoxelGeometryElement.Value.GeometryShape & BaseGeometryShapes[DirectionIndex]) != BaseGeometryShapes[DirectionIndex])
			{
				continue;
			}
			
			const auto CurrentVoxelType = LocalizedVoxelGeometryElement.Value.VoxelType;
		
			if (!VoxelTypeToMeshSectionMap.Contains(CurrentVoxelType))
			{
				auto MeshSection = ProcMeshSectionDataWrapper();

				MeshSection.SectionIndex = NumberOfAlreadyCreatedSections;
				NumberOfAlreadyCreatedSections +=1;

				MeshSection.IsSolid = true; //VoxelQuad.Value. .Value.IsSolid;  //TODO: Replace this !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

				VoxelTypeToMeshSectionMap.Add(LocalizedVoxelGeometryElement.Value.VoxelType, MeshSection);
			}

			if (auto MeshSectionDataPtr = VoxelTypeToMeshSectionMap.Find(CurrentVoxelType))
			{
				for (int j = 0; j < 4; ++j)
				{
					MeshSectionDataPtr->Vertices.Add(VoxelSize*localBlockVertexData[LocalBlockTriangleData[j + DirectionIndex * 4]] + VoxelSize*FVector(LocalizedVoxelGeometryElement.Key.X,LocalizedVoxelGeometryElement.Key.Y, LocalizedVoxelGeometryElement.Key.Z ) );
				}
									
				MeshSectionDataPtr->UVs.Append({FVector2d(1,1), FVector2d(1,0), FVector2d(0,0), FVector2d(0,1)});
				const auto CurrentVertexCount = MeshSectionDataPtr->VertexCount;
				MeshSectionDataPtr->Triangles.Append({CurrentVertexCount + 3, CurrentVertexCount + 2, CurrentVertexCount, CurrentVertexCount + 2, CurrentVertexCount + 1, CurrentVertexCount});
				MeshSectionDataPtr->VertexCount += 4;
			}
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
}

void AChunk::CompressChunk()
{
	FChunkData::Compress(*BlocksDataPtr);
}


FChunkGeometry AChunk::GetGeometryData()
{
	FChunkGeometry ChunkGeometryData;
	ChunkGeometryData.Geometry = VoxelGeometry;
	ChunkGeometryData.ChunkLocation = Location;
	ChunkGeometryData.DirectionIndex = 6;

	return ChunkGeometryData;
}

void AChunk::DestroyBlockAt(FVector BlockWorldLocation)
{
	/*Destroys the block at the given world location*/
	// const auto BlockLocation = FloorVector(BlockWorldLocation - GetActorLocation())/(DefaultVoxelSize);
	//
	// const FIntVector Neighbors[6] = {
	// 	FIntVector(BlockLocation.X+1,BlockLocation.Y,BlockLocation.Z),
	// 	FIntVector(BlockLocation.X,BlockLocation.Y+1,BlockLocation.Z),
	// 	FIntVector(BlockLocation.X-1,BlockLocation.Y,BlockLocation.Z),
	// 	FIntVector(BlockLocation.X,BlockLocation.Y-1,BlockLocation.Z),
	// 	FIntVector(BlockLocation.X,BlockLocation.Y,BlockLocation.Z+1),
	// 	FIntVector(BlockLocation.X,BlockLocation.Y,BlockLocation.Z-1)							
	// };
	//
	// const FIntVector NeighboringChunks[6] = {
	// 	FIntVector(Location.X+1,Location.Y,Location.Z),
	// 	FIntVector(Location.X,Location.Y+1,Location.Z),
	// 	FIntVector(Location.X-1,Location.Y,Location.Z),
	// 	FIntVector(Location.X,Location.Y-1,Location.Z),
	// 	FIntVector(Location.X,Location.Y,Location.Z+1),
	// 	FIntVector(Location.X,Location.Y,Location.Z-1)							
	// };
	//
	// const int32 OppositeDirections[6] = {
	// 	2,
	// 	3,
	// 	0,
	// 	1,
	// 	5,
	// 	4							
	// };
	// 	
	// //Remove the quads associated to the block and add new ones
	// VoxelGeometry.Remove(BlockLocation);
	// for (int32 i = 0; i < 6; ++i)
	// {
	// 	if (IsInsideChunk(Neighbors[i]))
	// 	{
	// 		const auto NeighboringVoxel = BlocksDataPtr->GetVoxelAt(Neighbors[i]);
	// 		if ((NeighboringVoxel != DefaultVoxel) && !HasQuadAt(Neighbors[i], OppositeDirections[i]) ) 
	// 		{
	// 			auto NewGeometryElement = FVoxelGeometryElement(NeighboringVoxel.VoxelType);
	// 			NewGeometryElement.GeometryShape = BaseGeometryShapes[OppositeDirections[i]];
	// 			AddGeometryAt(Neighbors[i], NewGeometryElement);
	// 		}
	// 	}
	// 	else
	// 	{
	// 		if (const auto NeighborPtr = OwningWorld->GetActorOfLoadedChunk(NeighboringChunks[i]))
	// 		{
	// 			const auto NeighboringVoxel = NeighborPtr->BlocksDataPtr->GetVoxelAt(NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize));
	// 			if ((NeighboringVoxel != DefaultVoxel) && !NeighborPtr->HasQuadAt(Neighbors[i], OppositeDirections[i]))
	// 			{
	// 				auto NewGeometryElement = FVoxelGeometryElement(NeighboringVoxel.VoxelType);
	// 				NewGeometryElement.GeometryShape = BaseGeometryShapes[OppositeDirections[i]];
	// 				NeighborPtr->AddGeometryAt(NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize),  NewGeometryElement);
	// 				NeighborPtr->RenderChunk(DefaultVoxelSize);	
	// 			}
	// 		}
	// 		else
	// 		{
	// 			UE_LOG(LogTemp, Error, TEXT("Neighborhing chunk doesn't exist"))
	// 		}
	// 	}
	// }
	// BlocksDataPtr->RemoveVoxel(BlockLocation);
	// RenderChunk(DefaultVoxelSize);
	SetBlockAt(BlockWorldLocation, DefaultVoxel);
	
}

void AChunk::SetBlockAt(FVector BlockWorldLocation, FVoxel BlockType)
{
	/*Sets the block at the given world location*/
	auto RelativeLocation = (BlockWorldLocation - GetActorLocation())/(DefaultVoxelSize);
	const auto BlockLocation = FloorVector(RelativeLocation); 
	
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

	//Remove the old faces for the block that is being edited
	VoxelGeometry.Remove(BlockLocation);

	//Adds the new faces for the targeted block
	//Removes the faces that have become occluded
	for (int32 i = 0; i < 6; ++i)
	{
		auto AddedFaceGeometry = FVoxelGeometryElement(BlockType.VoxelType);
		AddedFaceGeometry.GeometryShape = BaseGeometryShapes[i];

		auto NewlyOccludedFaceGeometry =  FVoxelGeometryElement(BlockType.VoxelType);
		NewlyOccludedFaceGeometry.GeometryShape = BaseGeometryShapes[OppositeDirections[i]];
		
		if (IsInsideChunk(Neighbors[i]))
		{
			const auto NeighboringVoxel = BlocksDataPtr->GetVoxelAt(Neighbors[i]);

			//Add a new face
			UE_LOG(LogTemp, Display, TEXT("The neighbor is %hs"), NeighboringVoxel.IsTransparent ? "transparent" : "opaque")
			if (NeighboringVoxel.IsTransparent && NeighboringVoxel != BlockType)
			{
				AddGeometryAt(BlockLocation, AddedFaceGeometry);
			}

			//Remove an occluded face
			if (!BlockType.IsTransparent || NeighboringVoxel == BlockType)
			{
				RemoveGeometryAt(Neighbors[i], BaseGeometryShapes[OppositeDirections[i]]);
			}
		}
		else
		{
			if (const auto NeighborPtr = OwningWorld->GetActorOfLoadedChunk(NeighboringChunks[i]))
			{
				const auto NeighboringVoxel = NeighborPtr->BlocksDataPtr->GetVoxelAt(NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize));

				//Add a new face
				if (NeighboringVoxel.IsTransparent && NeighboringVoxel != BlockType)
				{
					AddGeometryAt(BlockLocation, AddedFaceGeometry);
				}

				//Remove an occluded face
				if (!BlockType.IsTransparent || NeighboringVoxel == BlockType)
				{
					NeighborPtr-> RemoveGeometryAt(NormaliseCyclicalCoordinates(Neighbors[i], ChunkSize), BaseGeometryShapes[OppositeDirections[i]]);
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
