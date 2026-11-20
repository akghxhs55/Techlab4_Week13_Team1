#pragma once

#include "AggregateGeom.h"
#include "BodySetup.generated.h"

UCLASS()
class UBodySetup : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere)
	FKAggregateGeom AggGeom;

private:
};
