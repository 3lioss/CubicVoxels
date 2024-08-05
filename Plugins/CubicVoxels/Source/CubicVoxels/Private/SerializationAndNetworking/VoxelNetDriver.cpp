// Fill out your copyright notice in the Description page of Project Settings.


#include "SerializationAndNetworking/VoxelNetDriver.h"
#include "SerializationAndNetworking/VoxelConnection.h"

bool UVoxelNetDriver::InitConnectionClass()
{
	// Always use the UVoxelConnection class.
	NetConnectionClass = UVoxelConnection::StaticClass();
		
	return true;
}