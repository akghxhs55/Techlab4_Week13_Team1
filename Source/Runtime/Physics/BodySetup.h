#pragma once

#include "AggregateGeom.h"
#include "BodySetup.generated.h"

UCLASS()
class UBodySetup : public UObject
{
	GENERATED_BODY()
public:
	~UBodySetup() override;

	UPROPERTY(EditAnywhere)
	FAggregateGeom AggGeom; // 하나의 강체(Rigid Body)에 사용할 여러 충돌 도형(Shape)의 정보를 모아놓은 자료구조, 복잡한 도형을 여러개의 충돌체를 통해 표현

private:
};
