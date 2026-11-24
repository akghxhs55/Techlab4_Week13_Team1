#pragma once

#include "GameFramework/Pawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VehicleMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "WheeledVehiclePawn.generated.h"

/**
 * 4륜 차량 기본 폰 (Unreal AWheeledVehiclePawn 대응)
 * 차체 충돌체/스태틱 메시, 4륜 무브먼트 컴포넌트, 3인칭 스프링암 및 카메라를 기본 제공합니다.
 */
UCLASS()
class AWheeledVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	AWheeledVehiclePawn();
	virtual ~AWheeledVehiclePawn() override = default;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	virtual UPawnMovementComponent* GetMovementComponent() const override;

	UBoxComponent* GetChassisCollision() const { return ChassisCollision; }
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }
	UVehicleMovementComponent* GetVehicleMovement() const { return VehicleMovement; }
	USpringArmComponent* GetSpringArmComponent() const { return SpringArmComponent; }
	UCameraComponent* GetCameraComponent() const { return CameraComponent; }

	UStaticMeshComponent* GetWheelMesh(int32 Index) const;

	// 편의 조작 바인딩
	void SetThrottleInput(float Val);
	void SetSteeringInput(float Val);
	void SetBrakeInput(float Val);
	void SetHandbrakeInput(bool bVal);

protected:
	UPROPERTY(VisibleAnywhere)
	UBoxComponent* ChassisCollision = nullptr;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* MeshComponent = nullptr;

	UPROPERTY(VisibleAnywhere)
	UVehicleMovementComponent* VehicleMovement = nullptr;

	UPROPERTY(VisibleAnywhere)
	USpringArmComponent* SpringArmComponent = nullptr;

	UPROPERTY(VisibleAnywhere)
	UCameraComponent* CameraComponent = nullptr;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* WheelMesh_FL = nullptr;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* WheelMesh_FR = nullptr;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* WheelMesh_RL = nullptr;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* WheelMesh_RR = nullptr;
};
