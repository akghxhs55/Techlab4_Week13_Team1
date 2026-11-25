#include "EnginePCH.h"
#include "VehicleSimulation.h"

#include <cmath>

#include "PhysXConversion.h"
#include "PhysicsCore.h"
#include "Math/EngineMath.h"

#include "PxPhysicsAPI.h"
#include "vehicle/PxVehicleAPI.h"
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
		delete PhysXData;
		PhysXData = nullptr;
	}
}

void FVehicleSimulationPipeline::SetupPhysXData()
{
	CleanupPhysXData();
	PhysXData = new FPhysXVehicleData();

	// 2개 액슬(전륜 0,1 / 후륜 2,3) 구성
	PhysXData->AxleDescription.setToDefault();
	const PxU32 frontWheels[2] = { 0, 1 };
	PhysXData->AxleDescription.addAxle(2, frontWheels);
	const PxU32 rearWheels[2] = { 2, 3 };
	PhysXData->AxleDescription.addAxle(2, rearWheels);

	// 서스펜션 계산 설정
	PhysXData->SuspCalcParams.suspensionJounceCalculationType = PxVehicleSuspensionJounceCalculationType::eRAYCAST;
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
		WheelStates[i].LocalPose = FTransform(FQuat::Identity(), Wheel.Offset);
	}
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
	const float SpeedRatio = std::abs(CurrentForwardSpeed) / std::max(Params.HighSpeedSteerRefSpeed, 1.0f);
	const float SteerSpeedScale = std::max(Params.MinHighSpeedSteerScale, 1.0f / (1.0f + std::pow(SpeedRatio, 1.6f)));

	float SteerAngleRad[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	WheelStates[0].SteerAngle = Steer * Params.FrontLeftWheel.MaxSteerAngle * SteerSpeedScale;
	SteerAngleRad[0] = FMath::DegreesToRadians(WheelStates[0].SteerAngle);

	WheelStates[1].SteerAngle = Steer * Params.FrontRightWheel.MaxSteerAngle * SteerSpeedScale;
	SteerAngleRad[1] = FMath::DegreesToRadians(WheelStates[1].SteerAngle);

	WheelStates[2].SteerAngle = 0.0f;
	SteerAngleRad[2] = 0.0f;

	WheelStates[3].SteerAngle = 0.0f;
	SteerAngleRad[3] = 0.0f;

	// 2.5 공력 다운포스(Downforce) 및 공기저항(Drag) 인가
	if (CurrentForwardSpeed > 1.0f && Params.DownforceCoeff > 0.0f)
	{
		// 전진 속도^2에 비례하는 하향력 (약간의 후방 공력 중심으로 고속 리어 접지력 보강)
		const float DownforceMag = Params.DownforceCoeff * CurrentForwardSpeed * CurrentForwardSpeed;
		const PxVec3 AeroCenterPos = ChassisPose.transform(PxVec3(-0.30f, 0.0f, 0.0f));
		PxRigidBodyExt::addForceAtPos(*ChassisActor, -UpDir * DownforceMag, AeroCenterPos, PxForceMode::eFORCE);
	}
	const float LinearSpeed = ChassisVel.magnitude();
	if (LinearSpeed > 0.5f && Params.AeroDragCoeff > 0.0f)
	{
		const float DragMag = Params.AeroDragCoeff * LinearSpeed * LinearSpeed;
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

	for (int32 i = 0; i < 4; ++i)
	{
		PxVehiclePhysXRoadGeometryQueryUpdate(
			PhysXData->WheelParams[i],
			PhysXData->SuspParams[i],
			PxVehiclePhysXRoadGeometryQueryType::eRAYCAST,
			&FilterCallback,
			QueryFilter,
			PhysXData->MaterialFrictionParams,
			SteerAngleRad[i],
			PhysXData->RigidBodyState,
			*Scene,
			nullptr,
			VehicleFrame,
			PhysXData->RoadGeomStates[i],
			nullptr
		);

		WheelStates[i].bInContact = PhysXData->RoadGeomStates[i].hitState;
		if (WheelStates[i].bInContact)
		{
			WheelStates[i].ContactNormal = P2KVector(PhysXData->RoadGeomStates[i].plane.n);
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
			WheelStates[i].LocalPose.Rotation = FRotator(FMath::RadiansToDegrees(WheelStates[i].RotationAngle), WheelStates[i].SteerAngle, 0.0f).Quaternion();
			continue;
		}

		// (A) 서스펜션 상향 반발력 인가
		const PxVec3 SuspForce = PhysXData->SuspForces[i].force;
		PxRigidBodyExt::addForceAtPos(*ChassisActor, SuspForce, WheelWorldPos, PxForceMode::eFORCE);

		// (B) 휠 접촉점 선속도 및 조향 방향 벡터
		const PxVec3 WheelVel = PxRigidBodyExt::getVelocityAtPos(*ChassisActor, WheelWorldPos);

		const PxQuat SteerQuat(SteerAngleRad[i], UpDir);
		const PxVec3 WheelForward = SteerQuat.rotate(ForwardDir);
		const PxVec3 WheelRight = SteerQuat.rotate(RightDir);

		const float LongSpeed = WheelVel.dot(WheelForward);
		const float LatSpeed = WheelVel.dot(WheelRight);

		// (C) 종방향 추진력 및 제동력 (후륜 구동 분배 + 타이어 종방향 트랙션 한계)
		const float NormalLoad = SuspForce.magnitude();
		const float MaxLongFriction = NormalLoad * Params.TireLongitudinalFriction;

		float WheelDriveForce = 0.0f;
		if (i >= 2) // Rear wheels (2, 3)
		{
			WheelDriveForce = (Throttle * Params.PeakTorque * 0.5f) / Wheel.Radius;
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

		WheelDriveForce = FMath::Clamp(WheelDriveForce, -MaxLongFriction, MaxLongFriction);

		const PxVec3 LongForce = WheelForward * WheelDriveForce;
		PxRigidBodyExt::addForceAtPos(*ChassisActor, LongForce, WheelWorldPos, PxForceMode::eFORCE);

		// (D) 횡방향 타이어 마찰력 (슬립각 기반 코너링 포스 + 전후 그립 밸런스 + 마찰원)
		const float LateralFrictionCoeff = (i < 2) ? Params.TireLateralFriction : Params.RearTireLateralFriction;
		const float MaxTotalLateral = NormalLoad * LateralFrictionCoeff;
		const float UsedLongRatio = (MaxLongFriction > 1.0f) ? FMath::Clamp(std::abs(WheelDriveForce) / MaxLongFriction, 0.0f, 1.0f) : 0.0f;
		// 풀가속 중에도 리어 횡그립이 급감하지 않도록 마찰원 감쇠 완만화 (최소 74% 이상 유지)
		const float EffectiveMaxLateral = MaxTotalLateral * std::sqrt(std::max(0.55f, 1.0f - 0.25f * UsedLongRatio * UsedLongRatio));
		const float EffectiveLongSpeed = std::max(std::abs(LongSpeed), 8.0f);
		const float SlipAngleRad = std::atan2(LatSpeed, EffectiveLongSpeed);
		const float LatForceMag = FMath::Clamp(-SlipAngleRad * Params.CorneringStiffness, -EffectiveMaxLateral, EffectiveMaxLateral);

		const PxVec3 LatForce = WheelRight * LatForceMag;
		PxRigidBodyExt::addForceAtPos(*ChassisActor, LatForce, WheelWorldPos, PxForceMode::eFORCE);

		// (E) 휠 롤링 회전 갱신
		const float TargetWheelOmega = LongSpeed / Wheel.Radius;
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
