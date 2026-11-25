#include "EnginePCH.h"
#include "VehicleSimulation.h"

#include <cmath>

#include "PhysXConversion.h"
#include "PhysicsCore.h"
#include "BodyInstance.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Math/EngineMath.h"

#include "PxPhysicsAPI.h"
#include "vehicle/PxVehicleAPI.h"
#include "vehicle/physxRoadGeometry/PxVehiclePhysXRoadGeometryState.h"
#include "vehicle/physxRoadGeometry/PxVehiclePhysXRoadGeometryHelpers.h"
#include "extensions/PxRigidBodyExt.h"

using namespace physx;

// PhysX Vehicle SDK 내부 상태 및 파라미터 보관 구조체 (PIMPL)
struct FPhysXVehicleData
{
	PxVehicleAxleDescription AxleDescription;
	PxVehicleWheelParams WheelParams[4];
	PxVehicleSuspensionParams SuspParams[4];
	PxVehicleSuspensionForceParams SuspForceParams[4];
	PxVehicleSuspensionStateCalculationParams SuspCalcParams;
	PxVehiclePhysXMaterialFrictionParams MaterialFrictionParams;

	PxVehicleRigidBodyState RigidBodyState;
	PxVehicleRoadGeometryState RoadGeomStates[4];
	PxVehicleSuspensionState SuspStates[4];
	PxVehicleSuspensionForce SuspForces[4];

	PxConvexMesh* UnitCylinderSweepMesh = nullptr;
	PxMaterial* ChassisSlideMaterial = nullptr;
};

FVehicleSimulationPipeline::FVehicleSimulationPipeline()
{
}

FVehicleSimulationPipeline::~FVehicleSimulationPipeline()
{
	CleanupPhysXData();
}

void FVehicleSimulationPipeline::Initialize(const FVehicleParams& InParams, PxRigidDynamic* InChassisActor)
{
	Params = InParams;
	ChassisActor = InChassisActor;

	SetupPhysXData();
}

void FVehicleSimulationPipeline::SetParams(const FVehicleParams& InParams)
{
	Params = InParams;
	SetupPhysXData();
}

void FVehicleSimulationPipeline::CleanupPhysXData()
{
	if (PhysXData)
	{
		if (PhysXData->UnitCylinderSweepMesh)
		{
			PxVehicleUnitCylinderSweepMeshDestroy(PhysXData->UnitCylinderSweepMesh);
			PhysXData->UnitCylinderSweepMesh = nullptr;
		}
		if (PhysXData->ChassisSlideMaterial)
		{
			PhysXData->ChassisSlideMaterial->release();
			PhysXData->ChassisSlideMaterial = nullptr;
		}
		delete PhysXData;
		PhysXData = nullptr;
	}
}

void FVehicleSimulationPipeline::SetupPhysXData()
{
	CleanupPhysXData();
	PhysXData = new FPhysXVehicleData();

	if (PxPhysics* Physics = FPhysicsCore::GetPhysics())
	{
		const PxCookingParams CookParams(Physics->getTolerancesScale());
		PhysXData->UnitCylinderSweepMesh = PxVehicleUnitCylinderSweepMeshCreate(GetKurisuVehicleFrame(), *Physics, CookParams);

		// 연석/바닥과 닿아도 걸리거나 튀지 않는 초저마찰·무반발 스키드 머티리얼을 차체 충돌 셰이프에 적용
		PhysXData->ChassisSlideMaterial = Physics->createMaterial(0.02f, 0.02f, 0.0f);
		if (PhysXData->ChassisSlideMaterial)
		{
			PhysXData->ChassisSlideMaterial->setFrictionCombineMode(PxCombineMode::eMIN);
			PhysXData->ChassisSlideMaterial->setRestitutionCombineMode(PxCombineMode::eMIN);
			if (ChassisActor)
			{
				const PxU32 NbShapes = ChassisActor->getNbShapes();
				for (PxU32 s = 0; s < NbShapes; ++s)
				{
					PxShape* Shape = nullptr;
					ChassisActor->getShapes(&Shape, 1, s);
					if (Shape)
					{
						Shape->setMaterials(&PhysXData->ChassisSlideMaterial, 1);
					}
				}
			}
		}
	}

	// 2개 액슬(전륜 0,1 / 후륜 2,3) 구성
	PhysXData->AxleDescription.setToDefault();
	const PxU32 frontWheels[2] = { 0, 1 };
	PhysXData->AxleDescription.addAxle(2, frontWheels);
	const PxU32 rearWheels[2] = { 2, 3 };
	PhysXData->AxleDescription.addAxle(2, rearWheels);

	// 서스펜션 계산 설정 (3D 원통 스윕 지원, 코너링 중 안쪽 바퀴 접지 유지를 위해 펴짐 속도 제한 비활성)
	PhysXData->SuspCalcParams.suspensionJounceCalculationType = PhysXData->UnitCylinderSweepMesh
		? PxVehicleSuspensionJounceCalculationType::eSWEEP
		: PxVehicleSuspensionJounceCalculationType::eRAYCAST;
	PhysXData->SuspCalcParams.limitSuspensionExpansionVelocity = false;

	// 타이어 마찰 기본값 (단일 기본 마찰 1.0)
	PhysXData->MaterialFrictionParams.defaultFriction = 1.0f;
	PhysXData->MaterialFrictionParams.nbMaterialFrictions = 0;
	PhysXData->MaterialFrictionParams.materialFrictions = nullptr;

	for (int32 i = 0; i < 4; ++i)
	{
		const FWheelConfig& Wheel = Params.GetWheel(i);

		// WheelParams
		PhysXData->WheelParams[i].radius = Wheel.Radius;
		PhysXData->WheelParams[i].halfWidth = Wheel.HalfWidth;
		PhysXData->WheelParams[i].mass = Wheel.Mass;
		PhysXData->WheelParams[i].moi = 0.5f * Wheel.Mass * Wheel.Radius * Wheel.Radius;
		PhysXData->WheelParams[i].dampingRate = Wheel.DampingRate;

		// SuspParams
		PhysXData->SuspParams[i].suspensionAttachment = PxTransform(K2PVector(Wheel.Offset));
		PhysXData->SuspParams[i].suspensionTravelDir = PxVec3(0.0f, 0.0f, -1.0f);
		PhysXData->SuspParams[i].suspensionTravelDist = Wheel.SuspensionTravelDist;
		PhysXData->SuspParams[i].wheelAttachment = PxTransform(PxIdentity);

		// SuspForceParams
		PhysXData->SuspForceParams[i].stiffness = Wheel.SpringStiffness;
		PhysXData->SuspForceParams[i].damping = Wheel.SpringDamping;
		PhysXData->SuspForceParams[i].sprungMass = Wheel.SprungMass;

		// RoadGeomStates 초기화
		PhysXData->RoadGeomStates[i].hitState = false;
		PhysXData->RoadGeomStates[i].friction = 1.0f;
		PhysXData->RoadGeomStates[i].plane = PxPlane(0.0f, 0.0f, 1.0f, 0.0f);
		PhysXData->RoadGeomStates[i].velocity = PxVec3(0.0f);

		// SuspStates 초기화
		PhysXData->SuspStates[i].jounce = 0.0f;
		PhysXData->SuspStates[i].jounceSpeed = 0.0f;

		// WheelStates 초기화
		WheelStates[i].bInContact = false;
		WheelStates[i].SuspensionJounce = 0.0f;
		WheelStates[i].SteerAngle = 0.0f;
		WheelStates[i].RotationSpeed = 0.0f;
		WheelStates[i].RotationAngle = 0.0f;
		WheelStates[i].SlipRatio = 0.0f;
		WheelStates[i].SlipAngleRad = 0.0f;
		WheelStates[i].bOnGravel = false;
		WheelStates[i].LocalPose = FTransform(FQuat::Identity(), Wheel.Offset);
	}

	CurrentGear = 1;
	EngineRPM = Params.IdleRPM;
	ShiftCooldownTimer = 0.0f;
	LateralG = 0.0f;
	LongitudinalG = 0.0f;
	bPrevVelocityValid = false;
	bTCActive = false;
	bDRSRequested = false;
	bDRSActive = false;
	GravelTime = 0.0f;
}

float FVehicleSimulationPipeline::EvaluateEngineTorqueScale(float InRPM) const
{
	const float Denom = std::max(Params.MaxRPM - Params.IdleRPM, 1000.0f);
	const float NormRPM = FMath::Clamp((InRPM - Params.IdleRPM) / Denom, 0.0f, 1.0f);

	// F1 하이브리드(ERS + 터보 ICE): 저회전 ERS 토크 필(0.78) -> 중·고회전 피크(1.06) -> 레브리미터 직전(0.88)
	if (NormRPM < 0.65f)
	{
		return FMath::Lerp(0.78f, 1.06f, NormRPM / 0.65f);
	}
	return FMath::Lerp(1.06f, 0.88f, (NormRPM - 0.65f) / 0.35f);
}

void FVehicleSimulationPipeline::UpdateDrivetrainAndShift(float DeltaTime, float CurrentForwardSpeed, float Throttle)
{
	if (ShiftCooldownTimer > 0.0f)
	{
		ShiftCooldownTimer = std::max(0.0f, ShiftCooldownTimer - DeltaTime);
	}

	if (CurrentForwardSpeed < -0.5f || (Throttle < -0.05f && CurrentForwardSpeed < 1.5f))
	{
		CurrentGear = -1;
	}
	else if (CurrentGear < 1)
	{
		CurrentGear = 1;
	}

	const float RearRadius = std::max(Params.RearLeftWheel.Radius, 0.1f);
	const float WheelAvgOmega = 0.5f * (std::abs(WheelStates[2].RotationSpeed) + std::abs(WheelStates[3].RotationSpeed));
	const float EffectiveSpeed = FMath::Lerp(std::abs(CurrentForwardSpeed), WheelAvgOmega * RearRadius, 0.25f);
	const float WheelRPM = (EffectiveSpeed / RearRadius) * (60.0f / (2.0f * 3.14159265f));

	const float ActiveGearRatio = (CurrentGear == -1)
		? Params.ReverseGearRatio
		: Params.GearRatios[FMath::Clamp(CurrentGear - 1, 0, FVehicleParams::MaxForwardGears - 1)];
	const float TargetRPM = FMath::Clamp(Params.IdleRPM + WheelRPM * ActiveGearRatio * Params.FinalDriveRatio, Params.IdleRPM, Params.MaxRPM);
	EngineRPM = FMath::FInterpTo(EngineRPM, TargetRPM, DeltaTime, 14.0f);

	if (CurrentGear >= 1 && ShiftCooldownTimer <= 0.0f)
	{
		if (EngineRPM >= Params.UpshiftRPM && CurrentGear < FVehicleParams::MaxForwardGears && Throttle > 0.1f)
		{
			++CurrentGear;
			ShiftCooldownTimer = 0.16f;
		}
		else if (EngineRPM <= Params.DownshiftRPM && CurrentGear > 1)
		{
			--CurrentGear;
			ShiftCooldownTimer = 0.16f;
		}
	}
}

float FVehicleSimulationPipeline::GetAverageRearSlipRatio() const
{
	return 0.5f * (std::abs(WheelStates[2].SlipRatio) + std::abs(WheelStates[3].SlipRatio));
}

int32 FVehicleSimulationPipeline::GetGravelWheelCount() const
{
	int32 Count = 0;
	for (int32 i = 0; i < 4; ++i)
	{
		if (WheelStates[i].bInContact && WheelStates[i].bOnGravel)
		{
			++Count;
		}
	}
	return Count;
}

void FVehicleSimulationPipeline::Update(float DeltaTime, PxScene* Scene, float Throttle, float Steer, float Brake, bool bHandbrake)
{
	if (!ChassisActor || !Scene || !PhysXData || DeltaTime <= 0.0f)
	{
		return;
	}

	// 1. 차체 물리 상태 동기화
	PxVehicleReadRigidBodyStateFromPhysXActor(*ChassisActor, PhysXData->RigidBodyState);

	const PxTransform ChassisPose = ChassisActor->getGlobalPose();
	const PxVec3 ForwardDir = ChassisPose.q.rotate(PxVec3(1.0f, 0.0f, 0.0f));
	const PxVec3 RightDir = ChassisPose.q.rotate(PxVec3(0.0f, 1.0f, 0.0f));
	const PxVec3 UpDir = ChassisPose.q.rotate(PxVec3(0.0f, 0.0f, 1.0f));

	// 2. 조향각 계산 (전륜 조향 + 속도 반비례 감쇠로 중·고속 스핀 방지 및 정밀 코너링)
	const PxVec3 ChassisVel = ChassisActor->getLinearVelocity();
	const float CurrentForwardSpeed = ChassisVel.dot(ForwardDir);
	const FVector CurWorldVel = P2KVector(ChassisVel);
	if (bPrevVelocityValid)
	{
		const FVector Accel = (CurWorldVel - PrevWorldVelocity) / DeltaTime;
		const float RawLongG = Accel.Dot(P2KVector(ForwardDir)) / 9.81f;
		const float RawLatG = Accel.Dot(P2KVector(RightDir)) / 9.81f;
		LongitudinalG = FMath::FInterpTo(LongitudinalG, FMath::Clamp(RawLongG, -6.0f, 6.0f), DeltaTime, 12.0f);
		LateralG = FMath::FInterpTo(LateralG, FMath::Clamp(RawLatG, -6.0f, 6.0f), DeltaTime, 12.0f);
	}
	PrevWorldVelocity = CurWorldVel;
	bPrevVelocityValid = true;

	UpdateDrivetrainAndShift(DeltaTime, CurrentForwardSpeed, Throttle);

	const float SpeedRatio = std::abs(CurrentForwardSpeed) / std::max(Params.HighSpeedSteerRefSpeed, 1.0f);
	const float SteerSpeedScale = std::max(Params.MinHighSpeedSteerScale, 1.0f / (1.0f + std::pow(SpeedRatio, 1.85f)));

	// 고속 직진/코너링 요 진동 억제 (중속 턴인은 유지하고 180~320 km/h 요 흔들림만 감쇠)
	const float HighSpeedYawRatio = FMath::Clamp(std::abs(CurrentForwardSpeed) / 75.0f, 0.0f, 1.0f);
	const float YawRate = ChassisActor->getAngularVelocity().dot(UpDir);
	if (HighSpeedYawRatio > 0.15f && std::abs(YawRate) > 1e-3f)
	{
		const float YawDampTorque = -YawRate * (900.0f * HighSpeedYawRatio * HighSpeedYawRatio);
		ChassisActor->addTorque(UpDir * YawDampTorque, PxForceMode::eFORCE);
	}

	float SteerAngleRad[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	WheelStates[0].SteerAngle = Steer * Params.FrontLeftWheel.MaxSteerAngle * SteerSpeedScale;
	SteerAngleRad[0] = FMath::DegreesToRadians(WheelStates[0].SteerAngle);

	WheelStates[1].SteerAngle = Steer * Params.FrontRightWheel.MaxSteerAngle * SteerSpeedScale;
	SteerAngleRad[1] = FMath::DegreesToRadians(WheelStates[1].SteerAngle);

	WheelStates[2].SteerAngle = 0.0f;
	SteerAngleRad[2] = 0.0f;

	WheelStates[3].SteerAngle = 0.0f;
	SteerAngleRad[3] = 0.0f;

	// 2.5 공력 다운포스(Downforce + 차고 그라운드 이펙트) 및 공기저항(Drag) + DRS 인가
	if (bDRSRequested && Brake < 0.05f && CurrentForwardSpeed > 20.0f && Throttle > 0.05f)
	{
		bDRSActive = true;
	}
	else
	{
		bDRSActive = false;
		if (Brake >= 0.05f || CurrentForwardSpeed < 15.0f)
		{
			bDRSRequested = false;
		}
	}

	const float ActiveDownforceScale = bDRSActive ? Params.DRSDownforceScale : 1.0f;
	const float ActiveDragScale = bDRSActive ? Params.DRSDragScale : 1.0f;

	if (CurrentForwardSpeed > 1.0f && Params.DownforceCoeff > 0.0f)
	{
		float AvgCompression = 0.0f;
		int32 ContactCount = 0;
		for (int32 i = 0; i < 4; ++i)
		{
			if (WheelStates[i].bInContact)
			{
				const float Travel = std::max(Params.GetWheel(i).SuspensionTravelDist, 0.05f);
				AvgCompression += FMath::Clamp(WheelStates[i].SuspensionJounce / Travel, 0.0f, 1.0f);
				++ContactCount;
			}
		}
		if (ContactCount > 0)
		{
			AvgCompression /= static_cast<float>(ContactCount);
		}
		const float GroundEffectFactor = 1.0f + Params.GroundEffectStrength * AvgCompression;

		// 전진 속도^2에 비례하는 하향력 (약간의 후방 공력 중심으로 고속 리어 접지력 보강)
		const float DownforceMag = Params.DownforceCoeff * ActiveDownforceScale * GroundEffectFactor * CurrentForwardSpeed * CurrentForwardSpeed;
		const PxVec3 AeroCenterPos = ChassisPose.transform(PxVec3(-0.30f, 0.0f, 0.0f));
		PxRigidBodyExt::addForceAtPos(*ChassisActor, -UpDir * DownforceMag, AeroCenterPos, PxForceMode::eFORCE);
	}
	const float LinearSpeed = ChassisVel.magnitude();
	if (LinearSpeed > 0.5f && Params.AeroDragCoeff > 0.0f)
	{
		const float DragMag = Params.AeroDragCoeff * ActiveDragScale * LinearSpeed * LinearSpeed;
		ChassisActor->addForce(-ChassisVel.getNormalized() * DragMag, PxForceMode::eFORCE);
	}

	// 3. 지면 레이캐스트 (차체 액터 충돌 무시 필터 콜백)
	class FVehicleRaycastFilterCallback : public PxQueryFilterCallback
	{
	public:
		PxRigidActor* IgnoreActor = nullptr;
		explicit FVehicleRaycastFilterCallback(PxRigidActor* InActor) : IgnoreActor(InActor) {}

		PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape*, const PxRigidActor* actor, PxHitFlags&) override
		{
			if (actor == IgnoreActor)
			{
				return PxQueryHitType::eNONE;
			}
			return PxQueryHitType::eBLOCK;
		}

		PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&, const PxShape*, const PxRigidActor*) override
		{
			return PxQueryHitType::eBLOCK;
		}
	};

	FVehicleRaycastFilterCallback FilterCallback(ChassisActor);
	PxQueryFilterData QueryFilter;
	QueryFilter.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER;

	const PxVehicleFrame VehicleFrame = GetKurisuVehicleFrame();
	GravelTime += DeltaTime;

	const PxVehiclePhysXRoadGeometryQueryType::Enum RoadQueryType = PhysXData->UnitCylinderSweepMesh
		? PxVehiclePhysXRoadGeometryQueryType::eSWEEP
		: PxVehiclePhysXRoadGeometryQueryType::eRAYCAST;

	for (int32 i = 0; i < 4; ++i)
	{
		PxVehiclePhysXRoadGeometryQueryState RoadQueryState;
		RoadQueryState.setToDefault();

		PxVehiclePhysXRoadGeometryQueryUpdate(
			PhysXData->WheelParams[i],
			PhysXData->SuspParams[i],
			RoadQueryType,
			&FilterCallback,
			QueryFilter,
			PhysXData->MaterialFrictionParams,
			SteerAngleRad[i],
			PhysXData->RigidBodyState,
			*Scene,
			PhysXData->UnitCylinderSweepMesh,
			VehicleFrame,
			PhysXData->RoadGeomStates[i],
			&RoadQueryState
		);

		WheelStates[i].bInContact = PhysXData->RoadGeomStates[i].hitState;
		WheelStates[i].bOnGravel = false;
		if (WheelStates[i].bInContact)
		{
			// 연석 턱이나 뾰족한 삼각형 모서리의 가파른 경사 법선을 월드 수직(0,0,1) 기준으로 완화해 바디 롤에 의한 왜곡 방지
			const PxVec3 WorldUp(0.0f, 0.0f, 1.0f);
			PxVec3 HitN = PhysXData->RoadGeomStates[i].plane.n;
			const float NDotUp = HitN.dot(WorldUp);
			if (NDotUp > 0.40f && NDotUp < 0.94f)
			{
				const PxVec3 SmoothedN = (HitN * 0.30f + WorldUp * 0.70f).getNormalized();
				PhysXData->RoadGeomStates[i].plane = PxPlane(RoadQueryState.hitPosition, SmoothedN);
			}

			WheelStates[i].ContactNormal = P2KVector(PhysXData->RoadGeomStates[i].plane.n);
			if (RoadQueryState.actor && RoadQueryState.actor->userData)
			{
				const FBodyInstance* HitBody = static_cast<const FBodyInstance*>(RoadQueryState.actor->userData);
				if (HitBody->OwnerComponent)
				{
					if (HitBody->OwnerComponent->GetOwner() &&
						HitBody->OwnerComponent->GetOwner()->GetName().find("Gravel") != FString::npos)
					{
						WheelStates[i].bOnGravel = true;
					}
					else if (const UStaticMeshComponent* HitMeshComp = Cast<UStaticMeshComponent>(HitBody->OwnerComponent))
					{
						if (HitMeshComp->GetStaticMesh() && HitMeshComp->GetStaticMesh()->GetPath().find("Gravel") != FString::npos)
						{
							WheelStates[i].bOnGravel = true;
						}
					}
				}
			}
		}
	}

	// 4. 서스펜션 압축 및 반발력 계산
	const PxVec3 Gravity(0.0f, 0.0f, -9.81f);

	for (int32 i = 0; i < 4; ++i)
	{
		PxVehicleSuspensionStateUpdate(
			PhysXData->WheelParams[i],
			PhysXData->SuspParams[i],
			PhysXData->SuspCalcParams,
			PhysXData->SuspForceParams[i].stiffness,
			PhysXData->SuspForceParams[i].damping,
			SteerAngleRad[i],
			PhysXData->RoadGeomStates[i],
			PhysXData->RigidBodyState,
			DeltaTime,
			VehicleFrame,
			Gravity,
			PhysXData->SuspStates[i]
		);

		PxVehicleSuspensionComplianceState ComplianceState;
		ComplianceState.setToDefault();

		PxVehicleSuspensionForceUpdate(
			PhysXData->SuspParams[i],
			PhysXData->SuspForceParams[i],
			PhysXData->RoadGeomStates[i],
			PhysXData->SuspStates[i],
			ComplianceState,
			PhysXData->RigidBodyState,
			Gravity,
			Params.ChassisMass,
			PhysXData->SuspForces[i]
		);

		WheelStates[i].SuspensionJounce = PhysXData->SuspStates[i].jounce;
	}

	// 4.5 안티롤바 (좌우 서스펜션 압축 차이 기반 롤 억제 & 하중 이동)
	float AntiRollAdjust[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	auto ComputeAxleAntiRoll = [&](int32 LeftIdx, int32 RightIdx, float Stiffness)
	{
		if (WheelStates[LeftIdx].bInContact && WheelStates[RightIdx].bInContact && Stiffness > 0.0f)
		{
			const float JounceDiff = WheelStates[LeftIdx].SuspensionJounce - WheelStates[RightIdx].SuspensionJounce;
			const float MaxClamp = 0.75f * std::min(PhysXData->SuspForces[LeftIdx].force.magnitude(), PhysXData->SuspForces[RightIdx].force.magnitude());
			const float AntiRollForce = FMath::Clamp(JounceDiff * Stiffness, -MaxClamp, MaxClamp);
			AntiRollAdjust[LeftIdx] = AntiRollForce;
			AntiRollAdjust[RightIdx] = -AntiRollForce;
		}
	};
	ComputeAxleAntiRoll(0, 1, Params.FrontAntiRollStiffness);
	ComputeAxleAntiRoll(2, 3, Params.RearAntiRollStiffness);

	// 기어비 & 엔진 RPM 토크 스케일 계산 (8단 기준 1.0x 정규화 -> 저단 가속력 상승, 8단 최고속 유지)
	const float TopGearTotalRatio = std::max(Params.GearRatios[FVehicleParams::MaxForwardGears - 1] * Params.FinalDriveRatio, 0.1f);
	const float ActiveGearRatio = (CurrentGear == -1)
		? Params.ReverseGearRatio
		: Params.GearRatios[FMath::Clamp(CurrentGear - 1, 0, FVehicleParams::MaxForwardGears - 1)];
	const float GearTorqueMult = ((ActiveGearRatio * Params.FinalDriveRatio) / TopGearTotalRatio) * EvaluateEngineTorqueScale(EngineRPM);
	bTCActive = false;

	// 5. 차체에 서스펜션력 + 타이어 추진/제동/횡마찰력 인가
	for (int32 i = 0; i < 4; ++i)
	{
		const FWheelConfig& Wheel = Params.GetWheel(i);

		// 서스펜션 압축이 반영된 휠 로컬 위치 (PhysX 5: Offset은 최대 압축 위치이므로 TravelDist - Jounce만큼 아래로 이동)
		FVector LocalWheelPos = Wheel.Offset;
		LocalWheelPos.Z -= (Wheel.SuspensionTravelDist - WheelStates[i].SuspensionJounce);
		const PxVec3 WheelWorldPos = ChassisPose.transform(K2PVector(LocalWheelPos));

		WheelStates[i].LocalPose.Location = LocalWheelPos;

		if (!WheelStates[i].bInContact)
		{
			// 공중에 떠 있을 때는 회전 감쇠
			WheelStates[i].RotationSpeed = FMath::FInterpTo(WheelStates[i].RotationSpeed, 0.0f, DeltaTime, 2.0f);
			WheelStates[i].RotationAngle += WheelStates[i].RotationSpeed * DeltaTime;
			WheelStates[i].SlipRatio = 0.0f;
			WheelStates[i].SlipAngleRad = 0.0f;
			WheelStates[i].bOnGravel = false;
			WheelStates[i].LocalPose.Rotation = FRotator(FMath::RadiansToDegrees(WheelStates[i].RotationAngle), WheelStates[i].SteerAngle, 0.0f).Quaternion();
			continue;
		}

		// (A) 서스펜션 상향 반발력 + 안티롤바 반력 + 자갈밭 요철 서스펜션 채터링 (연석 단차 통과 시 비정상적 스파이크 상한 제한)
		PxVec3 SuspForce = PhysXData->SuspForces[i].force + UpDir * AntiRollAdjust[i];
		const float MaxWheelSuspForce = Params.ChassisMass * 9.81f * 2.2f;
		const float SuspMag = SuspForce.magnitude();
		if (SuspMag > MaxWheelSuspForce && SuspMag > 1e-4f)
		{
			SuspForce *= (MaxWheelSuspForce / SuspMag);
		}
		PxRigidBodyExt::addForceAtPos(*ChassisActor, SuspForce, WheelWorldPos, PxForceMode::eFORCE);

		// (B) 휠 접촉점 선속도 및 조향 방향 벡터
		const PxVec3 WheelVel = PxRigidBodyExt::getVelocityAtPos(*ChassisActor, WheelWorldPos);

		const PxQuat SteerQuat(SteerAngleRad[i], UpDir);
		const PxVec3 WheelForward = SteerQuat.rotate(ForwardDir);
		const PxVec3 WheelRight = SteerQuat.rotate(RightDir);

		const float LongSpeed = WheelVel.dot(WheelForward);
		const float LatSpeed = WheelVel.dot(WheelRight);

		const float NormalLoad = std::max(0.0f, SuspForce.dot(UpDir));
		if (WheelStates[i].bOnGravel && std::abs(LongSpeed) > 1.0f)
		{
			const float SpeedFactor = FMath::Clamp(std::abs(LongSpeed) / 18.0f, 0.25f, 1.65f);
			const float WheelPhase = GravelTime * (34.0f + static_cast<float>(i) * 8.1f) + WheelWorldPos.x * 2.1f + WheelWorldPos.z * 1.9f;
			const float ChatterWave = 0.60f * std::sin(WheelPhase) + 0.40f * std::cos(WheelPhase * 1.87f);
			PxRigidBodyExt::addForceAtPos(*ChassisActor, UpDir * (ChatterWave * NormalLoad * 0.25f * SpeedFactor), WheelWorldPos, PxForceMode::eFORCE);
			if (i < 2)
			{
				// 전륜 자갈 조향 저항/떨림 (핸들이 자갈에 끌리는 느낌)
				PxRigidBodyExt::addForceAtPos(*ChassisActor, RightDir * (std::sin(WheelPhase * 0.73f) * NormalLoad * 0.08f * SpeedFactor), WheelWorldPos, PxForceMode::eFORCE);
			}
			WheelStates[i].LocalPose.Location.Z += ChatterWave * 0.012f * SpeedFactor;
		}

		// (C) 종방향 추진력, 타이어 하중 민감도, 자갈 저항/그립 감쇠, 슬립비 및 TCS
		const float LoadSensitivityFactor = FMath::Clamp(
			std::pow(Params.TireNominalLoad / std::max(NormalLoad, 250.0f), Params.TireLoadSensitivity),
			0.68f,
			1.22f
		);
		const float GravelLongScale = WheelStates[i].bOnGravel ? 0.75f : 1.0f;
		const float MaxLongFriction = NormalLoad * Params.TireLongitudinalFriction * GravelLongScale * LoadSensitivityFactor;

		float WheelDriveForce = 0.0f;
		if (i >= 2) // Rear wheels (2, 3)
		{
			WheelDriveForce = (Throttle * Params.PeakTorque * GearTorqueMult * 0.5f) / Wheel.Radius;
		}

		// 브레이크 / 핸드브레이크 제동
		float TotalBrakeTorque = Brake * Wheel.MaxBrakeTorque;
		if (bHandbrake)
		{
			TotalBrakeTorque += Wheel.MaxHandbrakeTorque;
		}

		const float SignVal = (LongSpeed > 0.0f) ? 1.0f : ((LongSpeed < 0.0f) ? -1.0f : 0.0f);

		if (TotalBrakeTorque > 0.0f && std::abs(LongSpeed) > 0.05f)
		{
			const float MaxBrakeForce = TotalBrakeTorque / Wheel.Radius;
			const float BrakeForceMag = FMath::Min(MaxBrakeForce, std::abs(LongSpeed) * Params.ChassisMass * 2.0f);
			WheelDriveForce -= SignVal * BrakeForceMag;
		}
		else if (std::abs(Throttle) < 0.01f && std::abs(LongSpeed) > 0.05f)
		{
			// 스로틀을 뗐을 때 자연 감속 (엔진 브레이크 + 타이어 굴림 저항)
			const float CoastResistance = FMath::Min(350.0f, std::abs(LongSpeed) * (Params.ChassisMass * 0.35f));
			WheelDriveForce -= SignVal * CoastResistance;
		}

		if (WheelStates[i].bOnGravel && std::abs(LongSpeed) > 0.4f)
		{
			// 자갈/오프로드의 구름 저항. 네 바퀴 모두 올라가면 약 0.15~0.3g로 완만하게 감속한다
			const float GravelDragForce = FMath::Min(NormalLoad * 0.10f + std::abs(LongSpeed) * 8.0f, NormalLoad * 0.20f);
			WheelDriveForce -= SignVal * GravelDragForce;
		}

		// 종방향 슬립비 추정 및 TCS 개입
		float RawSlipRatio = 0.0f;
		if (MaxLongFriction > 1.0f)
		{
			const float OverloadRatio = (std::abs(WheelDriveForce) - MaxLongFriction) / MaxLongFriction;
			if (OverloadRatio > 0.0f)
			{
				const float ForceDir = (WheelDriveForce >= 0.0f) ? 1.0f : -1.0f;
				RawSlipRatio = ForceDir * FMath::Clamp(OverloadRatio * 0.35f, 0.0f, 0.6f);
			}
		}

		if (bTractionControlEnabled && i >= 2 && Throttle > 0.05f)
		{
			const float TCThreshold = MaxLongFriction * 0.94f;
			if (WheelDriveForce > TCThreshold || RawSlipRatio > Params.MaxTCSlipRatio)
			{
				WheelDriveForce = std::min(WheelDriveForce, TCThreshold);
				RawSlipRatio = std::min(RawSlipRatio, Params.MaxTCSlipRatio * 0.8f);
				bTCActive = true;
			}
		}

		WheelStates[i].SlipRatio = FMath::FInterpTo(WheelStates[i].SlipRatio, RawSlipRatio, DeltaTime, 12.0f);
		WheelDriveForce = FMath::Clamp(WheelDriveForce, -MaxLongFriction, MaxLongFriction);

		const PxVec3 LongForce = WheelForward * WheelDriveForce;
		PxRigidBodyExt::addForceAtPos(*ChassisActor, LongForce, WheelWorldPos, PxForceMode::eFORCE);

		// (D) 횡방향 타이어 마찰력 (하중 민감도 + 자갈 그립 저하 + 간이 Pacejka + 마찰원)
		const float GravelLatScale = WheelStates[i].bOnGravel ? 0.45f : 1.0f;
		const float LateralFrictionCoeff = ((i < 2) ? Params.TireLateralFriction : Params.RearTireLateralFriction) * GravelLatScale;
		const float MaxTotalLateral = NormalLoad * LateralFrictionCoeff * LoadSensitivityFactor;
		const float UsedLongRatio = (MaxLongFriction > 1.0f) ? FMath::Clamp(std::abs(WheelDriveForce) / MaxLongFriction, 0.0f, 1.0f) : 0.0f;
		const float EffectiveMaxLateral = MaxTotalLateral * std::sqrt(std::max(0.55f, 1.0f - 0.25f * UsedLongRatio * UsedLongRatio));
		const float EffectiveLongSpeed = std::max(std::abs(LongSpeed), 10.0f);
		const float SlipAngleRad = std::atan2(LatSpeed, EffectiveLongSpeed);
		WheelStates[i].SlipAngleRad = SlipAngleRad;

		const float PacejkaNorm = std::sin(Params.PacejkaC * std::atan(Params.PacejkaB * SlipAngleRad));
		const float LatForceMag = FMath::Clamp(-PacejkaNorm * EffectiveMaxLateral, -EffectiveMaxLateral, EffectiveMaxLateral);

		const PxVec3 LatForce = WheelRight * LatForceMag;
		// 횡력 작용점을 무게중심(액터 원점) 높이 쪽으로 올려 코너링 롤(좌우 쏠림)을 줄인다.
		// 전후·좌우 위치는 그대로라 선회(요) 모멘트는 변하지 않는다
		FVector LatApplyLocalPos = LocalWheelPos;
		LatApplyLocalPos.Z *= Params.LateralForceRollInfluence;
		PxRigidBodyExt::addForceAtPos(*ChassisActor, LatForce, ChassisPose.transform(K2PVector(LatApplyLocalPos)), PxForceMode::eFORCE);

		// (E) 휠 롤링 회전 갱신 (슬립비 반영)
		const float TargetWheelOmega = (LongSpeed * (1.0f + WheelStates[i].SlipRatio)) / Wheel.Radius;
		WheelStates[i].RotationSpeed = TargetWheelOmega;
		WheelStates[i].RotationAngle += TargetWheelOmega * DeltaTime;

		WheelStates[i].LocalPose.Rotation = FRotator(FMath::RadiansToDegrees(WheelStates[i].RotationAngle), WheelStates[i].SteerAngle, 0.0f).Quaternion();
	}
}

const FWheelSimulationState& FVehicleSimulationPipeline::GetWheelState(EVehicleWheelOrder Order) const
{
	return WheelStates[static_cast<int32>(Order)];
}

const FWheelSimulationState& FVehicleSimulationPipeline::GetWheelState(int32 Index) const
{
	return WheelStates[FMath::Clamp(Index, 0, 3)];
}

float FVehicleSimulationPipeline::GetForwardSpeed() const
{
	if (!ChassisActor)
	{
		return 0.0f;
	}

	const PxTransform ChassisPose = ChassisActor->getGlobalPose();
	const PxVec3 ForwardDir = ChassisPose.q.rotate(PxVec3(1.0f, 0.0f, 0.0f));
	return ChassisActor->getLinearVelocity().dot(ForwardDir);
}
