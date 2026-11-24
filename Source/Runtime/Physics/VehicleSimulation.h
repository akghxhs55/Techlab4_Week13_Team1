#pragma once

#include "Math/Vector.h"
#include "Math/Transform.h"
#include "VehicleParams.h"

namespace physx
{
	class PxRigidDynamic;
	class PxScene;
}

// 개별 바퀴의 런타임 물리 시뮬레이션 상태
struct FWheelSimulationState
{
	bool bInContact = false;                    // 지면 접촉 여부
	float SuspensionJounce = 0.0f;              // 서스펜션 압축량 (m)
	float SteerAngle = 0.0f;                    // 현재 조향각 (도 단위)
	float RotationSpeed = 0.0f;                 // 바퀴 회전 각속도 (rad/s)
	float RotationAngle = 0.0f;                 // 바퀴 누적 회전각 (rad, 렌더링용)
	FTransform LocalPose = FTransform::Identity;// 차체 기준 휠 렌더링 로컬 트랜스폼
	FVector ContactPoint = FVector::ZeroVector; // 지면 접점
	FVector ContactNormal = FVector(0.0f, 0.0f, 1.0f); // 지면 법선
};

// 차량 시뮬레이션 파이프라인 (지면 레이캐스트, 서스펜션, 타이어 마찰, 차체 힘 인가)
class FVehicleSimulationPipeline
{
public:
	FVehicleSimulationPipeline();
	~FVehicleSimulationPipeline();

	// 파라미터 및 대상 액터 초기화
	void Initialize(const FVehicleParams& InParams, physx::PxRigidDynamic* InChassisActor);

	// 매 물리 틱 업데이트 (서스펜션 레이캐스트 -> 반발력/타이어 마찰 계산 -> 차체 힘 인가)
	void Update(float DeltaTime, physx::PxScene* Scene, float Throttle, float Steer, float Brake, bool bHandbrake);

	// 결과 조회
	const FWheelSimulationState& GetWheelState(EVehicleWheelOrder Order) const;
	const FWheelSimulationState& GetWheelState(int32 Index) const;

	float GetForwardSpeed() const; // 전진 속도 (m/s)
	float GetForwardSpeedKmh() const { return GetForwardSpeed() * 3.6f; } // 전진 속도 (km/h)

	const FVehicleParams& GetParams() const { return Params; }
	void SetParams(const FVehicleParams& InParams);

private:
	FVehicleParams Params;
	physx::PxRigidDynamic* ChassisActor = nullptr;

	FWheelSimulationState WheelStates[4];

	// PIMPL로 PhysX Vehicle SDK 내부 헤더 의존성 격리
	struct FPhysXVehicleData* PhysXData = nullptr;

	void SetupPhysXData();
	void CleanupPhysXData();
};
