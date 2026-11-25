#include "EnginePCH.h"
#include "BodySetup.h"

#include "PhysicsAsset.h"
#include "UObject/Casts.h"

UBodySetup::~UBodySetup()
{
	if (UPhysicsAsset* Asset = Cast<UPhysicsAsset>(GetOuter()))
	{
		Asset->BodySetups.Remove(this);
	}
}
