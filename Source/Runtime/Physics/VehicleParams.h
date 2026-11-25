#pragma once

#include "Math/Vector.h"
#include "Math/Transform.h"

namespace physx
{
	struct PxVehicleFrame;
	struct PxVehicleScale;
	struct PxVehicleAxleDescription;
	struct PxVehicleRigidBodyParams;
	struct PxVehicleWheelParams;
	struct PxVehicleSuspensionParams;
	struct PxVehicleSuspensionForceParams;
	struct PxVehicleTireForceParams;
}

// 4륜 바퀴 식별용 인덱스
enum class EVehicleWheelOrder : uint8
{
	FrontLeft = 0,
	FrontRight = 1,
	RearLeft = 2,
	RearRight = 3
};

// 개별 바퀴 및 서스펜션 설정
struct FWheelConfig
{
	// 차체 중심 기준 휠 장착 위치 (X: 전후, Y: 좌우, Z: 상하 [단위: m])
	FVector Offset = FVector::ZeroVector;

	// 바퀴 물리 제원
	float Radius = 0.35f;           // 바퀴 반경 (0.35m = 35cm)
	float HalfWidth = 0.10f;        // 바퀴 반폭 (0.10m = 전체 폭 20cm)
	float Mass = 20.0f;             // 바퀴 질량 (kg)
	float DampingRate = 0.25f;      // 회전 감쇠율

	// 서스펜션 스프링 및 댐퍼 세팅
	float SuspensionTravelDist = 0.25f; // 최대 스트로크 거리 (0.25m)
	float SpringStiffness = 35000.0f;   // 스프링 강성 (N/m)
	float SpringDamping = 4500.0f;      // 댐퍼 계수 (N*s/m)
	float SprungMass = 375.0f;          // 바퀴당 지탱하는 차체 무게 (1500kg / 4)

	// 조향 및 제동
	float MaxSteerAngle = 0.0f;         // 최대 조향 각도 (도 단위, 전륜: 35도, 후륜: 0도)
	float MaxBrakeTorque = 1500.0f;     // 풋브레이크 제동 토크 (N*m)
	float MaxHandbrakeTorque = 0.0f;    // 핸드브레이크 제동 토크 (N*m, 후륜 연결)
};

// 차량 전체 제원 및 4륜 설정
struct FVehicleParams
{
	// 차체(Chassis) 제원
	float ChassisMass = 1500.0f;                            // 차체 질량 (kg)
	FVector ChassisDimensions = FVector(4.0f, 2.0f, 1.2f); // 전장 4m, 전폭 2m, 전고 1.2m
	FVector CenterOfMassOffset = FVector(0.0f, 0.0f, -0.2f);// 무게중심 오프셋 (저중심 안정화)
	float PeakTorque = 4000.0f;                             // 총 구동 토크 (N*m)

	// 공력(다운포스/항력) 및 타이어 그립·고속 조향 제원
	float DownforceCoeff = 2.6f;            // 속도^2 비례 하향력 계수 (N / (m/s)^2)
	float AeroDragCoeff = 1.6f;             // 속도^2 비례 공기저항 계수 (N / (m/s)^2)
	float TireLateralFriction = 1.75f;      // 전륜 횡방향 타이어 마찰계수 (F1 슬릭)
	float RearTireLateralFriction = 2.25f;  // 후륜 광폭 타이어 횡방향 마찰계수 (고속 오버스티어 억제)
	float TireLongitudinalFriction = 1.85f; // 종방향 가감속 트랙션 마찰계수
	float CorneringStiffness = 62000.0f;    // 슬립각 기반 코너링 강성 (N / rad)
	float MinHighSpeedSteerScale = 0.12f;   // 고속 시 최대 조향각 배율 (32도 * 0.12 ≈ 3.8도)
	float HighSpeedSteerRefSpeed = 18.0f;   // 고속 조향 감쇠 기준 속도 (m/s ≈ 65 km/h에서 50% 감쇠)

	// 4개 바퀴 설정 (0: FL, 1: FR, 2: RL, 3: RR)
	FWheelConfig FrontLeftWheel;
	FWheelConfig FrontRightWheel;
	FWheelConfig RearLeftWheel;
	FWheelConfig RearRightWheel;

	FVehicleParams();

	// 기본 4륜 세팅 (Ferrari SF26 F1 기본 적용)
	void SetupDefaultCar();

	// 페라리 F1 SF26 (Ferrari F1) 실측 레이싱 세팅 초기화
	void SetupFerrariF1();

	// 코닉세그 제메라 (Koenigsegg Gemera) 4인승 메가GT 실측 세팅 초기화
	void SetupKoenigseggGemera();

	// 바퀴 접근자
	FWheelConfig& GetWheel(EVehicleWheelOrder Order);
	const FWheelConfig& GetWheel(EVehicleWheelOrder Order) const;

	FWheelConfig& GetWheel(int32 Index);
	const FWheelConfig& GetWheel(int32 Index) const;

	// 직육면체 근사 관성 모멘트(MOI) 계산
	FVector ComputeChassisMOI() const;
};

// PhysX Vehicle 2.0 축 및 스케일 헬퍼
physx::PxVehicleFrame GetKurisuVehicleFrame();
physx::PxVehicleScale GetKurisuVehicleScale();
