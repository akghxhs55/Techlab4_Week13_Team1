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
	ChassisCollision->BoxExtent = FVector(2.76f, 0.98f, 0.55f); // 페라리 F1 실측: 5.53m x 1.96m x 1.1m의 로컬 반 크기
	ChassisCollision->BodyInstance.bSimulatePhysics = true;
	ChassisCollision->BodyInstance.bEnableGravity = true;
	ChassisCollision->BodyInstance.CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	ChassisCollision->BodyInstance.MassInKgOverride = 850.0f; // F1 레이싱 카 총중량
	ChassisCollision->BodyInstance.LinearDamping = 0.15f;
	ChassisCollision->BodyInstance.AngularDamping = 0.5f;

	// 2. 차체 렌더링용 비주얼 메시 (ChassisCollision의 자식으로 붙이며, 자식을 갖지 않음)
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
	MeshComponent->SetupAttachment(ChassisCollision);
	MeshComponent->BodyInstance.bSimulatePhysics = false;
	MeshComponent->BodyInstance.CollisionEnabled = ECollisionEnabled::NoCollision;

	// 차체 메시 검색: Gemera 우선 -> Ferrari F1 -> Ferrari -> Cube 대체
	UStaticMesh* ChassisMesh = UAssetManager::GetAssetByPath<UStaticMesh>("Meshes/Gemera/Gemera_Chassis.obj");
	if (!ChassisMesh)
	{
		ChassisMesh = UAssetManager::GetAssetByPath<UStaticMesh>("Meshes/FerrariF1/FerrariF1_Chassis.obj");
	}
	if (!ChassisMesh)
	{
		ChassisMesh = UAssetManager::GetAssetByPath<UStaticMesh>("Meshes/Ferrari/Ferrari_Chassis.obj");
	}
	const bool bIsCustomChassis = (ChassisMesh != nullptr);
	if (!ChassisMesh)
	{
		ChassisMesh = UAssetManager::GetAssetByPath<UStaticMesh>("Cube");
	}
	if (ChassisMesh)
	{
		MeshComponent->SetStaticMesh(ChassisMesh);
	}

	if (bIsCustomChassis)
	{
		// 실측 OBJ: 바닥이 Z=0이므로 박스 충돌체 중심(Z=0)에 맞춰 Z=-0.564m 오프셋 적용
		MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -0.564f));
		MeshComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	}
	else
	{
		MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
		MeshComponent->SetRelativeScale3D(FVector(4.0f, 2.0f, 1.0f));
	}

	// 3. 차량 무브먼트 컴포넌트 (루트 물리 컴포넌트 바인딩)
	VehicleMovement = CreateDefaultSubobject<UVehicleMovementComponent>("VehicleMovement");
	VehicleMovement->SetUpdatedComponent(ChassisCollision);

	// 4. 스프링암 및 카메라 (차체에 맞춘 3인칭 레이싱 시점)
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>("SpringArm");
	SpringArmComponent->SetupAttachment(ChassisCollision);
	SpringArmComponent->TargetArmLength = 7.5f;
	SpringArmComponent->TargetOffset = FVector(0.0f, 0.0f, 1.6f);
	SpringArmComponent->SetRelativeRotation(FRotator(15.0f, 0.0f, 0.0f));
	SpringArmComponent->bInheritPitch = false;
	SpringArmComponent->bInheritRoll = false;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>("Camera");
	CameraComponent->SetupAttachment(SpringArmComponent);

	// 5. 바퀴 비주얼 컴포넌트 생성 (ChassisCollision 직속 자식 -> 부모 스케일 1.0으로 왜곡 없음)
	UStaticMesh* WheelMesh_FL_Asset = UAssetManager::GetAssetByPath<UStaticMesh>("Meshes/FerrariF1/FerrariF1_Wheel_FL.obj");
	UStaticMesh* WheelMesh_FR_Asset = UAssetManager::GetAssetByPath<UStaticMesh>("Meshes/FerrariF1/FerrariF1_Wheel_FR.obj");
	UStaticMesh* WheelMesh_RL_Asset = UAssetManager::GetAssetByPath<UStaticMesh>("Meshes/FerrariF1/FerrariF1_Wheel_RL.obj");
	UStaticMesh* WheelMesh_RR_Asset = UAssetManager::GetAssetByPath<UStaticMesh>("Meshes/FerrariF1/FerrariF1_Wheel_RR.obj");

	if (!WheelMesh_FL_Asset || !WheelMesh_FR_Asset || !WheelMesh_RL_Asset || !WheelMesh_RR_Asset)
	{
		UStaticMesh* F1Front = UAssetManager::GetAssetByPath<UStaticMesh>("Meshes/FerrariF1/FerrariF1_Wheel_Front.obj");
		UStaticMesh* F1Rear = UAssetManager::GetAssetByPath<UStaticMesh>("Meshes/FerrariF1/FerrariF1_Wheel_Rear.obj");
		if (F1Front && F1Rear)
		{
			if (!WheelMesh_FL_Asset) WheelMesh_FL_Asset = F1Front;
			if (!WheelMesh_FR_Asset) WheelMesh_FR_Asset = F1Front;
			if (!WheelMesh_RL_Asset) WheelMesh_RL_Asset = F1Rear;
			if (!WheelMesh_RR_Asset) WheelMesh_RR_Asset = F1Rear;
		}
	}

	if (!WheelMesh_FL_Asset || !WheelMesh_FR_Asset || !WheelMesh_RL_Asset || !WheelMesh_RR_Asset)
	{
		UStaticMesh* GemeraFL = UAssetManager::GetAssetByPath<UStaticMesh>("Meshes/Gemera/Gemera_Wheel_FL.obj");
		UStaticMesh* GemeraFR = UAssetManager::GetAssetByPath<UStaticMesh>("Meshes/Gemera/Gemera_Wheel_FR.obj");
		UStaticMesh* GemeraRL = UAssetManager::GetAssetByPath<UStaticMesh>("Meshes/Gemera/Gemera_Wheel_RL.obj");
		UStaticMesh* GemeraRR = UAssetManager::GetAssetByPath<UStaticMesh>("Meshes/Gemera/Gemera_Wheel_RR.obj");
		if (!WheelMesh_FL_Asset) WheelMesh_FL_Asset = GemeraFL;
		if (!WheelMesh_FR_Asset) WheelMesh_FR_Asset = GemeraFR;
		if (!WheelMesh_RL_Asset) WheelMesh_RL_Asset = GemeraRL;
		if (!WheelMesh_RR_Asset) WheelMesh_RR_Asset = GemeraRR;
	}

	if (!WheelMesh_FL_Asset || !WheelMesh_FR_Asset || !WheelMesh_RL_Asset || !WheelMesh_RR_Asset)
	{
		UStaticMesh* WheelMeshFront = UAssetManager::GetAssetByPath<UStaticMesh>("Meshes/Gemera/Gemera_Wheel_Front.obj");
		UStaticMesh* WheelMeshRear = UAssetManager::GetAssetByPath<UStaticMesh>("Meshes/Gemera/Gemera_Wheel_Rear.obj");
		if (!WheelMeshFront || !WheelMeshRear)
		{
			WheelMeshFront = UAssetManager::GetAssetByPath<UStaticMesh>("Meshes/Ferrari/Ferrari_Wheel_R.obj");
			WheelMeshRear = UAssetManager::GetAssetByPath<UStaticMesh>("Meshes/Ferrari/Ferrari_Wheel_L.obj");
		}
		if (WheelMeshFront && WheelMeshRear)
		{
			if (!WheelMesh_FL_Asset) WheelMesh_FL_Asset = WheelMeshFront;
			if (!WheelMesh_FR_Asset) WheelMesh_FR_Asset = WheelMeshFront;
			if (!WheelMesh_RL_Asset) WheelMesh_RL_Asset = WheelMeshRear;
			if (!WheelMesh_RR_Asset) WheelMesh_RR_Asset = WheelMeshRear;
		}
	}

	if (!WheelMesh_FL_Asset)
	{
		UStaticMesh* FallbackWheel = UAssetManager::GetAssetByPath<UStaticMesh>("Cylinder");
		if (!FallbackWheel)
		{
			FallbackWheel = UAssetManager::GetAssetByPath<UStaticMesh>("Cube");
		}
		WheelMesh_FL_Asset = FallbackWheel;
		WheelMesh_FR_Asset = FallbackWheel;
		WheelMesh_RL_Asset = FallbackWheel;
		WheelMesh_RR_Asset = FallbackWheel;
	}

	auto SetupWheelComp = [&](UStaticMeshComponent*& Comp, const char* Name, UStaticMesh* MeshAsset, const FVector& FallbackOffset)
	{
		Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetupAttachment(ChassisCollision);
		Comp->SetStaticMesh(MeshAsset);
		Comp->SetRelativeLocation(FallbackOffset);
		const bool bIsCyl = (!MeshAsset || MeshAsset->GetPath() == "Cylinder" || MeshAsset->GetPath() == "Cube");
		if (!bIsCyl)
		{
			Comp->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
			Comp->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
		}
		else
		{
			// Cylinder 기본(반경 1m, 높이 1m) -> 바퀴 반경 0.35m(지름 0.7m), 폭 0.2m 및 Roll 90도 정렬
			Comp->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.20f));
			Comp->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
		}
		Comp->BodyInstance.bSimulatePhysics = false;
		Comp->BodyInstance.CollisionEnabled = ECollisionEnabled::NoCollision;
	};

	SetupWheelComp(WheelMesh_FL, "Wheel_FL", WheelMesh_FL_Asset, FVector(1.483f, -0.842f, -0.2f));
	SetupWheelComp(WheelMesh_FR, "Wheel_FR", WheelMesh_FR_Asset, FVector(1.483f, 0.842f, -0.2f));
	SetupWheelComp(WheelMesh_RL, "Wheel_RL", WheelMesh_RL_Asset, FVector(-1.515f, -0.834f, -0.2f));
	SetupWheelComp(WheelMesh_RR, "Wheel_RR", WheelMesh_RR_Asset, FVector(-1.515f, 0.834f, -0.2f));
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

				const bool bIsCylinderFallback = (WheelComp->GetStaticMesh() && 
					(WheelComp->GetStaticMesh()->GetPath() == "Cylinder" || WheelComp->GetStaticMesh()->GetPath() == "Cube"));

				if (!bIsCylinderFallback)
				{
					// 커스텀 휠 OBJ(Gemera, Ferrari 등)는 이미 Y축이 회전축이며 전진/조향 축에 정렬되어 있으므로 포즈 회전을 그대로 적용
					const FQuat TotalWheelRot = LocalWheelPose.Rotation * WheelMeshRotationOffset.Quaternion();
					WheelComp->SetRelativeRotation(TotalWheelRot.ToFRotator());
				}
				else
				{
					const FQuat TotalWheelRot = LocalWheelPose.Rotation * CylinderOrientation;
					WheelComp->SetRelativeRotation(TotalWheelRot.ToFRotator());
				}
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
