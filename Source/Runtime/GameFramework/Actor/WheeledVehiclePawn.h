#pragma once

#include "GameFramework/Pawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VehicleMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/ParticleSystemComponent.h"
#include "WheeledVehiclePawn.generated.h"

class UUserWidget;
class UTextBlock;
class UProgressBar;

/**
 * 4륜 차량 기본 폰 (Unreal AWheeledVehiclePawn 대응)
 * 차체 충돌체/스태틱 메시, 4륜 무브먼트 컴포넌트, 3인칭 스프링암 및 카메라, F1 텔레메트리 HUD를 기본 제공합니다.
 */
UCLASS()
class AWheeledVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	AWheeledVehiclePawn();
	virtual ~AWheeledVehiclePawn() override;

	virtual void BeginPlay() override;
	virtual void EndPlay() override;
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
	void ToggleDRS();
	void ToggleTractionControl();

	UPROPERTY(EditAnywhere)
	FRotator WheelMeshRotationOffset = FRotator(0.0f, 0.0f, 0.0f);

	// ---- 속도감 카메라 & F1 HUD 설정 ----
	UPROPERTY(EditAnywhere)
	bool bEnableDynamicFOV = true;

	UPROPERTY(EditAnywhere)
	float HighSpeedFOVBoost = 18.0f; // 300 km/h 기준 추가 시야각 (기본 60도 -> 78도)

	UPROPERTY(EditAnywhere)
	bool bEnableGForceCamera = true;

	UPROPERTY(EditAnywhere)
	bool bShowTelemetryHUD = true;

	UPROPERTY(EditAnywhere)
	bool bEnableVehicleAudio = true;

	UPROPERTY(EditAnywhere)
	bool bEnableVehicleParticles = true;

protected:
	void CreateTelemetryHUD();
	void UpdateTelemetryHUD(float DeltaTime);
	void DestroyTelemetryHUD();
	void InitVehicleAudio();
	void UpdateVehicleAudio(float DeltaTime);
	void StopVehicleAudio();
	void InitVehicleParticles();
	void UpdateVehicleParticles();

	float BaseFieldOfView = 60.0f;
	float BaseTargetArmLength = 7.0f;
	FVector BaseSocketOffset = FVector::ZeroVector;
	float BuffetingTime = 0.0f;
	float ElapsedSessionTime = 0.0f;

	UUserWidget* TelemetryWidget = nullptr;
	UTextBlock* SpeedText = nullptr;
	UTextBlock* GearText = nullptr;
	UTextBlock* RPMText = nullptr;
	UTextBlock* GForceText = nullptr;
	UTextBlock* TimerText = nullptr;
	UTextBlock* StatusText = nullptr;
	UProgressBar* RPMBar = nullptr;
	UProgressBar* ThrottleBar = nullptr;
	UProgressBar* BrakeBar = nullptr;

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

	UPROPERTY(VisibleAnywhere)
	UAudioComponent* EngineAudioComponent = nullptr;

	UPROPERTY(VisibleAnywhere)
	UAudioComponent* WindAudioComponent = nullptr;

	UPROPERTY(VisibleAnywhere)
	UAudioComponent* TireAudioComponent = nullptr;

	UPROPERTY(VisibleAnywhere)
	UParticleSystemComponent* TireSmokeRL = nullptr;

	UPROPERTY(VisibleAnywhere)
	UParticleSystemComponent* TireSmokeRR = nullptr;

	UPROPERTY(VisibleAnywhere)
	UParticleSystemComponent* FloorSparks = nullptr;

	bool bTireSmokeActiveRL = false;
	bool bTireSmokeActiveRR = false;
	bool bFloorSparksActive = false;
};
