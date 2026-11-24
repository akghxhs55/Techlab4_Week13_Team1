#include "EnginePCH.h"
#include "GameFramework/Actor/WheeledVehiclePawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VehicleMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Asset/AssetManager.h"

AWheeledVehiclePawn::AWheeledVehiclePawn()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
	SetRootComponent(MeshComponent);
	MeshComponent->BodyInstance.bSimulatePhysics = true;
	MeshComponent->BodyInstance.bEnableGravity = true;
	MeshComponent->BodyInstance.CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	MeshComponent->BodyInstance.MassInKgOverride = 1500.0f;

	// 기본 큐브 메시 지정
	UStaticMesh* CubeMesh = UAssetManager::GetAssetByPath<UStaticMesh>("Cube");
	if (CubeMesh)
	{
		MeshComponent->SetStaticMesh(CubeMesh);
	}
	MeshComponent->SetRelativeScale3D(FVector(4.0f, 2.0f, 1.0f));

	VehicleMovement = CreateDefaultSubobject<UVehicleMovementComponent>("VehicleMovement");
	VehicleMovement->SetUpdatedComponent(MeshComponent);

	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>("SpringArm");
	SpringArmComponent->SetupAttachment(MeshComponent);
	SpringArmComponent->TargetArmLength = 6.0f;
	SpringArmComponent->TargetOffset = FVector(0.0f, 0.0f, 1.5f);
	SpringArmComponent->SetRelativeRotation(FRotator(15.0f, 0.0f, 0.0f));
	SpringArmComponent->bInheritPitch = false;
	SpringArmComponent->bInheritRoll = false;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>("Camera");
	CameraComponent->SetupAttachment(SpringArmComponent);

	// 바퀴 비주얼 컴포넌트 생성 (실린더 또는 큐브 메시)
	UStaticMesh* WheelAsset = UAssetManager::GetAssetByPath<UStaticMesh>("Cylinder");
	if (!WheelAsset)
	{
		WheelAsset = CubeMesh;
	}

	auto SetupWheelComp = [&](UStaticMeshComponent*& Comp, const char* Name)
	{
		Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetupAttachment(MeshComponent);
		Comp->SetStaticMesh(WheelAsset);
		Comp->SetRelativeScale3D(FVector(0.7f, 0.2f, 0.7f)); // 직경 70cm, 폭 20cm
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
		for (int32 i = 0; i < 4; ++i)
		{
			if (UStaticMeshComponent* WheelComp = GetWheelMesh(i))
			{
				const FTransform LocalWheelPose = VehicleMovement->GetWheelLocalTransform(i);
				WheelComp->SetRelativeLocation(LocalWheelPose.Location);
				WheelComp->SetRelativeRotation(LocalWheelPose.Rotation.ToFRotator());
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
