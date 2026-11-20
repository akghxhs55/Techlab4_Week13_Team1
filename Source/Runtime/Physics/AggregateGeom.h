#pragma once

#include "AggregateGeom.generated.h"

USTRUCT()
struct FKShapeElem
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Center;
	UPROPERTY()
	FRotator Rotation;
};

USTRUCT()
struct FKSphereElem : public FKShapeElem
{
	GENERATED_BODY()

	UPROPERTY()
	float Radius;
};

USTRUCT()
struct FKBoxElem : public FKShapeElem
{
	GENERATED_BODY()

	UPROPERTY()
	float X;
	
	UPROPERTY()
	float Y;
	
	UPROPERTY()
	float Z;
};

USTRUCT()
struct FKSphylElem : public FKShapeElem
{
	GENERATED_BODY()

	UPROPERTY()
	float Radius;
	UPROPERTY()
	float Length;
};

USTRUCT()
struct FKAggregateGeom
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FKSphereElem> SphereElems;
	UPROPERTY()
	TArray<FKBoxElem> BoxElems;
	UPROPERTY()
	TArray<FKSphylElem> SphylElems;
	int32 GetElementCount() const { return SphereElems.Num() + BoxElems.Num() + SphylElems.Num(); }
	void EmptyElements() {
		SphereElems.Empty();
		BoxElems.Empty();
		SphylElems.Empty();
	}
};
