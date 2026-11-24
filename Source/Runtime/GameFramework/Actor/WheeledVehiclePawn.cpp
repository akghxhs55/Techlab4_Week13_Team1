#include "EnginePCH.h"
#include "GameFramework/Actor/WheeledVehiclePawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VehicleMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Asset/AssetManager.h"

AWheeledVehiclePawn::AWheeledVehiclePawn()
{
	// 1. 차체 루트 충돌체 (스케일 1.0 유지 -> 자식 컴포넌트의 Shear 왜곡 방지)
	ChassisCollision = CreateDefaultSubobject<UBoxComponent>("ChassisCollision");
	SetRootComponent(ChassisCollision);
	ChassisCollision->BoxExtent = FVector(2.0f, 1.0f, 0.5f); // 4m x 2m x 1m의 로컬 반 크기
	ChassisCollision->BodyInstance.bSimulatePhysics = true;
	ChassisCollision->BodyInstance.bEnableGravity = true;
	ChassisCollision->BodyInstance.CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	ChassisCollision->BodyInstance.MassInKgOverride = 1500.0f;
	ChassisCollision->BodyInstance.LinearDamping = 0.15f;
	ChassisCollision->BodyInstance.AngularDamping = 0.5f;

	// 2. 차체 렌더링용 비주얼 메시 (ChassisCollision의 자식으로 붙이며, 자식을 갖지 않음)
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
	MeshComponent->SetupAttachment(ChassisCollision);
	MeshComponent->BodyInstance.bSimulatePhysics = false;
	MeshComponent->BodyInstance.CollisionEnabled = ECollisionEnabled::NoCollision;

	// 기본 큐브 메시 지정
	UStaticMesh* CubeMesh = UAssetManager::GetAssetByPath<UStaticMesh>("Cube");
	if (CubeMesh)
	{
		MeshComponent->SetStaticMesh(CubeMesh);
	}
	MeshComponent->SetRelativeScale3D(FVector(4.0f, 2.0f, 1.0f));

	// 3. 차량 무브먼트 컴포넌트 (루트 물리 컴포넌트 바인딩)
	VehicleMovement = CreateDefaultSubobject<UVehicleMovementComponent>("VehicleMovement");
	VehicleMovement->SetUpdatedComponent(ChassisCollision);

	// 4. 스프링암 및 카메라
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>("SpringArm");
	SpringArmComponent->SetupAttachment(ChassisCollision);
	SpringArmComponent->TargetArmLength = 6.0f;
	SpringArmComponent->TargetOffset = FVector(0.0f, 0.0f, 1.5f);
	SpringArmComponent->SetRelativeRotation(FRotator(15.0f, 0.0f, 0.0f));
	SpringArmComponent->bInheritPitch = false;
	SpringArmComponent->bInheritRoll = false;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>("Camera");
	CameraComponent->SetupAttachment(SpringArmComponent);

	// 5. 바퀴 비주얼 컴포넌트 생성 (ChassisCollision 직속 자식 -> 부모 스케일 1.0으로 왜곡 없음)
	UStaticMesh* WheelAsset = UAssetManager::GetAssetByPath<UStaticMesh>("Cylinder");
	if (!WheelAsset)
	{
		WheelAsset = CubeMesh;
	}

	auto SetupWheelComp = [&](UStaticMeshComponent*& Comp, const char* Name)
	{
		Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetupAttachment(ChassisCollision);
		Comp->SetStaticMesh(WheelAsset);
		// Cylinder 기본(반경 1m, 높이 1m) -> 바퀴 반경 0.35m(지름 0.7m), 폭 0.2m에 맞춤
		Comp->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.20f));
		Comp->BodyInstance.bSimulatePhysics = false;
		Comp->BodyInstance.CollisionEnabled = ECollisionEnabled::NoCollision;
	};

	SetupWheelComp(WheelMesh_FL, "Wheel_FL");
	SetupWheelComp(WheelMesh_FR, "Wheel_FR");
	SetupWheelComp(WheelMesh_RL, "Wheel_RL");
	SetupWheelComp(WheelMesh_RR, "Wheel_RR");
}

void AWheeledVehiclePawn::BeginPlay()
{
	Super::BeginPlay();
}

void AWheeledVehiclePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 바퀴 비주얼 컴포넌트 실시간 동기화 (서스펜션 상하 스트로크 + 조향각 + 롤링 회전)
	if (VehicleMovement)
	{
		// 실린더 메시는 중심축이 Z축(위쪽)에 있으므로, Roll 90도 회전을 합성하여 차량 좌우(Y축) 회전축과 일치시킴
		const FQuat CylinderOrientation = FRotator(0.0f, 0.0f, 90.0f).Quaternion();

		for (int32 i = 0; i < 4; ++i)
		{
			if (UStaticMeshComponent* WheelComp = GetWheelMesh(i))
			{
				const FTransform LocalWheelPose = VehicleMovement->GetWheelLocalTransform(i);
				WheelComp->SetRelativeLocation(LocalWheelPose.Location);

				const FQuat TotalWheelRot = LocalWheelPose.Rotation * CylinderOrientation;
				WheelComp->SetRelativeRotation(TotalWheelRot.ToFRotator());
			}
		}
	}
}

UStaticMeshComponent* AWheeledVehiclePawn::GetWheelMesh(int32 Index) const
{
	switch (Index)
	{
	case 0: return WheelMesh_FL;
	case 1: return WheelMesh_FR;
	case 2: return WheelMesh_RL;
	case 3: return WheelMesh_RR;
	default: return nullptr;
	}
}

UPawnMovementComponent* AWheeledVehiclePawn::GetMovementComponent() const
{
	return VehicleMovement;
}

void AWheeledVehiclePawn::SetThrottleInput(float Val)
{
	if (VehicleMovement)
	{
		VehicleMovement->SetThrottleInput(Val);
	}
}

void AWheeledVehiclePawn::SetSteeringInput(float Val)
{
	if (VehicleMovement)
	{
		VehicleMovement->SetSteeringInput(Val);
	}
}

void AWheeledVehiclePawn::SetBrakeInput(float Val)
{
	if (VehicleMovement)
	{
		VehicleMovement->SetBrakeInput(Val);
	}
}

void AWheeledVehiclePawn::SetHandbrakeInput(bool bVal)
{
	if (VehicleMovement)
	{
		VehicleMovement->SetHandbrakeInput(bVal);
	}
}
