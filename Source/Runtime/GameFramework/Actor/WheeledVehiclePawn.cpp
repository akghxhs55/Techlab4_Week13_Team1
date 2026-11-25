#include "EnginePCH.h"
#include "GameFramework/Actor/WheeledVehiclePawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VehicleMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Asset/AssetManager.h"
#include "Engine/World.h"
#include "UI/UserWidget.h"
#include "UI/PanelWidget.h"
#include "UI/TextBlock.h"
#include "UI/ProgressBar.h"
#include "UI/Image.h"
#include "Math/EngineMath.h"

#include <cmath>
#include <cstdio>

AWheeledVehiclePawn::AWheeledVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;

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
		// 물리 평형 시 바퀴 중심(Chassis 기준 -0.324m)과 F1 차체 허브(+0.334m)를 일치시켜 바퀴와 서스펜션 암 정렬
		MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -0.658f));
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

AWheeledVehiclePawn::~AWheeledVehiclePawn()
{
	DestroyTelemetryHUD();
}

void AWheeledVehiclePawn::BeginPlay()
{
	Super::BeginPlay();

	if (CameraComponent)
	{
		BaseFieldOfView = CameraComponent->GetFieldOfView();
	}
	if (SpringArmComponent)
	{
		BaseTargetArmLength = SpringArmComponent->TargetArmLength;
		BaseSocketOffset = SpringArmComponent->SocketOffset;
		SpringArmComponent->bEnableCameraRotationLag = true;
		SpringArmComponent->CameraRotationLagSpeed = 12.0f;
		SpringArmComponent->bEnableCameraLag = true;
		SpringArmComponent->CameraLagSpeed = 20.0f;
		SpringArmComponent->CameraLagMaxDistance = 0.55f;
	}

	ElapsedSessionTime = 0.0f;
	BuffetingTime = 0.0f;

	if (bShowTelemetryHUD && GetWorld() && GetWorld()->IsGameWorld())
	{
		CreateTelemetryHUD();
	}
}

void AWheeledVehiclePawn::EndPlay()
{
	DestroyTelemetryHUD();
	if (CameraComponent)
	{
		CameraComponent->SetFieldOfView(BaseFieldOfView);
	}
	if (SpringArmComponent)
	{
		SpringArmComponent->TargetArmLength = BaseTargetArmLength;
		SpringArmComponent->SocketOffset = BaseSocketOffset;
	}
	Super::EndPlay();
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

		// 속도 비례 동적 FOV 및 G-Force/풍압 카메라 모션
		const float SpeedKmH = std::abs(VehicleMovement->GetForwardSpeedKmH());
		const float SpeedAlpha = FMath::Clamp(SpeedKmH / 300.0f, 0.0f, 1.0f);

		if (CameraComponent && bEnableDynamicFOV)
		{
			const float TargetFOV = BaseFieldOfView + HighSpeedFOVBoost * std::pow(SpeedAlpha, 1.15f);
			const float NextFOV = FMath::FInterpTo(CameraComponent->GetFieldOfView(), TargetFOV, DeltaTime, 6.0f);
			CameraComponent->SetFieldOfView(NextFOV);
		}

		if (SpringArmComponent && bEnableGForceCamera)
		{
			const float LongG = VehicleMovement->GetLongitudinalG();
			const float LatG = VehicleMovement->GetLateralG();

			const float TargetArmLen = BaseTargetArmLength + FMath::Clamp(LongG * 0.25f, -0.45f, 0.55f);
			SpringArmComponent->TargetArmLength = FMath::FInterpTo(SpringArmComponent->TargetArmLength, TargetArmLen, DeltaTime, 8.0f);

			FVector TargetSocket = BaseSocketOffset;
			TargetSocket.Y += FMath::Clamp(-LatG * 0.14f, -0.45f, 0.45f);

			const float BuffetAlpha = FMath::Clamp((SpeedKmH - 130.0f) / 170.0f, 0.0f, 1.0f);
			if (BuffetAlpha > 0.01f)
			{
				BuffetingTime += DeltaTime * (18.0f + 16.0f * BuffetAlpha);
				TargetSocket.Y += std::sin(BuffetingTime * 1.7f) * 0.010f * BuffetAlpha;
				TargetSocket.Z += std::cos(BuffetingTime * 2.3f) * 0.008f * BuffetAlpha;
			}

			SpringArmComponent->SocketOffset = FMath::VInterpTo(SpringArmComponent->SocketOffset, TargetSocket, DeltaTime, 10.0f);
		}

		UpdateTelemetryHUD(DeltaTime);
	}
}

void AWheeledVehiclePawn::CreateTelemetryHUD()
{
	if (TelemetryWidget || !GetWorld())
	{
		return;
	}

	TelemetryWidget = NewObject<UUserWidget>(nullptr);
	if (!TelemetryWidget)
	{
		return;
	}
	TelemetryWidget->SetOwningWorld(GetWorld());
	TelemetryWidget->Visibility = static_cast<int32>(EUIVisibility::HitTestInvisible);

	UCanvasPanel* RootCanvas = NewObject<UCanvasPanel>(nullptr);
	TelemetryWidget->SetRootWidget(RootCanvas);

	// 우하단 F1 텔레메트리 카드 (360 x 176 px @ 1080p 기준)
	UImage* CardBg = NewObject<UImage>(nullptr);
	CardBg->Anchors = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	CardBg->AlignmentX = 1.0f;
	CardBg->AlignmentY = 1.0f;
	CardBg->Offsets = FVector4(-32.0f, -32.0f, 360.0f, 176.0f);
	CardBg->ColorAndOpacity = FLinearColor(0.05f, 0.06f, 0.08f, 0.82f);
	RootCanvas->AddChild(CardBg);

	// 상단 페라리 레드 액센트 바
	UImage* AccentLine = NewObject<UImage>(nullptr);
	AccentLine->Anchors = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	AccentLine->AlignmentX = 1.0f;
	AccentLine->AlignmentY = 1.0f;
	AccentLine->Offsets = FVector4(-32.0f, -204.0f, 360.0f, 4.0f);
	AccentLine->ColorAndOpacity = FLinearColor(0.86f, 0.05f, 0.08f, 0.95f);
	RootCanvas->AddChild(AccentLine);

	// 기어 배경 박스
	UImage* GearBg = NewObject<UImage>(nullptr);
	GearBg->Anchors = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	GearBg->AlignmentX = 1.0f;
	GearBg->AlignmentY = 1.0f;
	GearBg->Offsets = FVector4(-48.0f, -124.0f, 68.0f, 68.0f);
	GearBg->ColorAndOpacity = FLinearColor(0.11f, 0.13f, 0.17f, 0.95f);
	RootCanvas->AddChild(GearBg);

	// 속도 수치 (KM/H)
	SpeedText = NewObject<UTextBlock>(nullptr);
	SpeedText->bAutoSize = false;
	SpeedText->Anchors = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	SpeedText->AlignmentX = 0.0f;
	SpeedText->AlignmentY = 0.0f;
	SpeedText->Offsets = FVector4(-372.0f, -192.0f, 175.0f, 54.0f);
	SpeedText->FontSize = 48.0f;
	SpeedText->ColorAndOpacity = FLinearColor(0.98f, 0.98f, 1.0f, 1.0f);
	SpeedText->SetText("0");
	RootCanvas->AddChild(SpeedText);

	UTextBlock* UnitText = NewObject<UTextBlock>(nullptr);
	UnitText->bAutoSize = false;
	UnitText->Anchors = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	UnitText->AlignmentX = 0.0f;
	UnitText->AlignmentY = 0.0f;
	UnitText->Offsets = FVector4(-372.0f, -142.0f, 80.0f, 22.0f);
	UnitText->FontSize = 14.0f;
	UnitText->ColorAndOpacity = FLinearColor(0.65f, 0.70f, 0.78f, 1.0f);
	UnitText->SetText("KM/H");
	RootCanvas->AddChild(UnitText);

	// 기어 텍스트
	GearText = NewObject<UTextBlock>(nullptr);
	GearText->bAutoSize = false;
	GearText->Anchors = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	GearText->AlignmentX = 0.0f;
	GearText->AlignmentY = 0.0f;
	GearText->Offsets = FVector4(-116.0f, -186.0f, 68.0f, 60.0f);
	GearText->FontSize = 46.0f;
	GearText->Justification = 1;
	GearText->ColorAndOpacity = FLinearColor(1.0f, 0.82f, 0.18f, 1.0f);
	GearText->SetText("1");
	RootCanvas->AddChild(GearText);

	// RPM 프로그레스 바
	RPMBar = NewObject<UProgressBar>(nullptr);
	RPMBar->Anchors = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	RPMBar->AlignmentX = 0.0f;
	RPMBar->AlignmentY = 0.0f;
	RPMBar->Offsets = FVector4(-372.0f, -114.0f, 324.0f, 14.0f);
	RPMBar->BackgroundColor = FLinearColor(0.12f, 0.14f, 0.18f, 0.95f);
	RPMBar->BorderPadding = FVector(1.0f, 1.0f, 0.0f);
	RPMBar->SetFillColorAndOpacity(FLinearColor(0.18f, 0.88f, 0.40f, 1.0f));
	RPMBar->SetPercent(0.25f);
	RootCanvas->AddChild(RPMBar);

	// RPM & G-Force 정보
	RPMText = NewObject<UTextBlock>(nullptr);
	RPMText->bAutoSize = false;
	RPMText->Anchors = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	RPMText->AlignmentX = 0.0f;
	RPMText->AlignmentY = 0.0f;
	RPMText->Offsets = FVector4(-372.0f, -94.0f, 150.0f, 22.0f);
	RPMText->FontSize = 15.0f;
	RPMText->ColorAndOpacity = FLinearColor(0.82f, 0.86f, 0.92f, 1.0f);
	RPMText->SetText("4200 RPM");
	RootCanvas->AddChild(RPMText);

	GForceText = NewObject<UTextBlock>(nullptr);
	GForceText->bAutoSize = false;
	GForceText->Anchors = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	GForceText->AlignmentX = 0.0f;
	GForceText->AlignmentY = 0.0f;
	GForceText->Offsets = FVector4(-222.0f, -94.0f, 174.0f, 22.0f);
	GForceText->FontSize = 15.0f;
	GForceText->Justification = 2;
	GForceText->ColorAndOpacity = FLinearColor(0.68f, 0.84f, 1.0f, 1.0f);
	GForceText->SetText("0.0G LAT");
	RootCanvas->AddChild(GForceText);

	// 주행 시간 & TCS/DRS 상태
	TimerText = NewObject<UTextBlock>(nullptr);
	TimerText->bAutoSize = false;
	TimerText->Anchors = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	TimerText->AlignmentX = 0.0f;
	TimerText->AlignmentY = 0.0f;
	TimerText->Offsets = FVector4(-372.0f, -68.0f, 160.0f, 20.0f);
	TimerText->FontSize = 14.0f;
	TimerText->ColorAndOpacity = FLinearColor(0.75f, 0.78f, 0.84f, 1.0f);
	TimerText->SetText("00:00.0");
	RootCanvas->AddChild(TimerText);

	StatusText = NewObject<UTextBlock>(nullptr);
	StatusText->bAutoSize = false;
	StatusText->Anchors = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	StatusText->AlignmentX = 0.0f;
	StatusText->AlignmentY = 0.0f;
	StatusText->Offsets = FVector4(-210.0f, -68.0f, 162.0f, 20.0f);
	StatusText->FontSize = 14.0f;
	StatusText->Justification = 2;
	StatusText->ColorAndOpacity = FLinearColor(0.35f, 0.90f, 0.45f, 1.0f);
	StatusText->SetText("TCS ON");
	RootCanvas->AddChild(StatusText);

	// 스로틀 / 브레이크 입력 바
	ThrottleBar = NewObject<UProgressBar>(nullptr);
	ThrottleBar->Anchors = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ThrottleBar->AlignmentX = 0.0f;
	ThrottleBar->AlignmentY = 0.0f;
	ThrottleBar->Offsets = FVector4(-372.0f, -44.0f, 158.0f, 8.0f);
	ThrottleBar->BackgroundColor = FLinearColor(0.12f, 0.14f, 0.16f, 0.95f);
	ThrottleBar->BorderPadding = FVector(0.0f, 0.0f, 0.0f);
	ThrottleBar->SetFillColorAndOpacity(FLinearColor(0.16f, 0.92f, 0.44f, 1.0f));
	ThrottleBar->SetPercent(0.0f);
	RootCanvas->AddChild(ThrottleBar);

	BrakeBar = NewObject<UProgressBar>(nullptr);
	BrakeBar->Anchors = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	BrakeBar->AlignmentX = 0.0f;
	BrakeBar->AlignmentY = 0.0f;
	BrakeBar->Offsets = FVector4(-206.0f, -44.0f, 158.0f, 8.0f);
	BrakeBar->BackgroundColor = FLinearColor(0.12f, 0.14f, 0.16f, 0.95f);
	BrakeBar->BorderPadding = FVector(0.0f, 0.0f, 0.0f);
	BrakeBar->SetFillColorAndOpacity(FLinearColor(0.95f, 0.22f, 0.22f, 1.0f));
	BrakeBar->SetPercent(0.0f);
	RootCanvas->AddChild(BrakeBar);

	TelemetryWidget->OnDestroyed = [this]()
	{
		TelemetryWidget = nullptr;
		SpeedText = nullptr;
		GearText = nullptr;
		RPMText = nullptr;
		GForceText = nullptr;
		TimerText = nullptr;
		StatusText = nullptr;
		RPMBar = nullptr;
		ThrottleBar = nullptr;
		BrakeBar = nullptr;
	};

	TelemetryWidget->AddToViewport(50);
	if (!TelemetryWidget->IsInViewport())
	{
		TelemetryWidget->Destroy();
	}
}

void AWheeledVehiclePawn::UpdateTelemetryHUD(float DeltaTime)
{
	if (!TelemetryWidget || !VehicleMovement)
	{
		return;
	}

	ElapsedSessionTime += DeltaTime;
	const int32 SpeedKmH = static_cast<int32>(std::round(std::abs(VehicleMovement->GetForwardSpeedKmH())));
	const int32 Gear = VehicleMovement->GetCurrentGear();
	const float RPM = VehicleMovement->GetEngineRPM();
	const float MaxRPM = std::max(VehicleMovement->GetMaxRPM(), 1000.0f);
	const float RPMRatio = FMath::Clamp(RPM / MaxRPM, 0.0f, 1.0f);

	char Buf[64];
	if (SpeedText)
	{
		std::snprintf(Buf, sizeof(Buf), "%d", SpeedKmH);
		SpeedText->SetText(Buf);
	}

	if (GearText)
	{
		if (Gear < 0)
		{
			GearText->SetText("R");
			GearText->ColorAndOpacity = FLinearColor(0.95f, 0.30f, 0.30f, 1.0f);
		}
		else if (Gear == 0)
		{
			GearText->SetText("N");
			GearText->ColorAndOpacity = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);
		}
		else
		{
			std::snprintf(Buf, sizeof(Buf), "%d", Gear);
			GearText->SetText(Buf);
			GearText->ColorAndOpacity = (RPMRatio > 0.88f)
				? FLinearColor(0.98f, 0.25f, 0.32f, 1.0f)
				: FLinearColor(1.0f, 0.82f, 0.18f, 1.0f);
		}
	}

	if (RPMBar)
	{
		RPMBar->SetPercent(RPMRatio);
		if (RPMRatio > 0.88f)
		{
			RPMBar->SetFillColorAndOpacity(FLinearColor(0.95f, 0.16f, 0.28f, 1.0f));
		}
		else if (RPMRatio > 0.68f)
		{
			RPMBar->SetFillColorAndOpacity(FLinearColor(0.98f, 0.72f, 0.14f, 1.0f));
		}
		else
		{
			RPMBar->SetFillColorAndOpacity(FLinearColor(0.18f, 0.88f, 0.40f, 1.0f));
		}
	}

	if (RPMText)
	{
		std::snprintf(Buf, sizeof(Buf), "%d RPM", static_cast<int32>(std::round(RPM)));
		RPMText->SetText(Buf);
	}

	if (GForceText)
	{
		std::snprintf(Buf, sizeof(Buf), "%.1fG LAT  %.1fG", std::abs(VehicleMovement->GetLateralG()), VehicleMovement->GetLongitudinalG());
		GForceText->SetText(Buf);
	}

	if (TimerText)
	{
		const int32 TotalTenths = static_cast<int32>(ElapsedSessionTime * 10.0f);
		const int32 Mins = TotalTenths / 600;
		const int32 Secs = (TotalTenths / 10) % 60;
		const int32 Tenths = TotalTenths % 10;
		std::snprintf(Buf, sizeof(Buf), "%02d:%02d.%d", Mins, Secs, Tenths);
		TimerText->SetText(Buf);
	}

	if (StatusText)
	{
		if (!VehicleMovement->bEnableTractionControl)
		{
			StatusText->SetText("TCS OFF");
			StatusText->ColorAndOpacity = FLinearColor(0.60f, 0.64f, 0.70f, 1.0f);
		}
		else if (VehicleMovement->IsTCActive())
		{
			StatusText->SetText("TCS ACTIVE");
			StatusText->ColorAndOpacity = FLinearColor(1.0f, 0.75f, 0.16f, 1.0f);
		}
		else
		{
			StatusText->SetText("TCS READY");
			StatusText->ColorAndOpacity = FLinearColor(0.35f, 0.90f, 0.45f, 1.0f);
		}
	}

	if (ThrottleBar)
	{
		ThrottleBar->SetPercent(FMath::Clamp(VehicleMovement->GetThrottleInput(), 0.0f, 1.0f));
	}
	if (BrakeBar)
	{
		BrakeBar->SetPercent(FMath::Clamp(VehicleMovement->GetBrakeInput(), 0.0f, 1.0f));
	}
}

void AWheeledVehiclePawn::DestroyTelemetryHUD()
{
	if (TelemetryWidget)
	{
		UUserWidget* WidgetToDestroy = TelemetryWidget;
		TelemetryWidget = nullptr;
		WidgetToDestroy->Destroy();
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
