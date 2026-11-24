#include "EnginePCH.h"
#include "VehicleParams.h"

#include "PhysXConversion.h"
#include "vehicle/PxVehicleAPI.h"

using namespace physx;

FVehicleParams::FVehicleParams()
{
	SetupDefaultCar();
}

void FVehicleParams::SetupDefaultCar()
{
	ChassisMass = 1500.0f;
	ChassisDimensions = FVector(4.0f, 2.0f, 1.2f);
	CenterOfMassOffset = FVector(0.0f, 0.0f, -0.2f);

	// 전륜 좌측 (Wheel 0)
	FrontLeftWheel.Offset = FVector(1.3f, -0.85f, -0.2f);
	FrontLeftWheel.Radius = 0.35f;
	FrontLeftWheel.HalfWidth = 0.10f;
	FrontLeftWheel.Mass = 20.0f;
	FrontLeftWheel.DampingRate = 0.25f;
	FrontLeftWheel.SuspensionTravelDist = 0.25f;
	FrontLeftWheel.SpringStiffness = 35000.0f;
	FrontLeftWheel.SpringDamping = 4500.0f;
	FrontLeftWheel.SprungMass = ChassisMass * 0.25f;
	FrontLeftWheel.MaxSteerAngle = 35.0f;
	FrontLeftWheel.MaxBrakeTorque = 1500.0f;
	FrontLeftWheel.MaxHandbrakeTorque = 0.0f;

	// 전륜 우측 (Wheel 1)
	FrontRightWheel = FrontLeftWheel;
	FrontRightWheel.Offset = FVector(1.3f, 0.85f, -0.2f);

	// 후륜 좌측 (Wheel 2)
	RearLeftWheel = FrontLeftWheel;
	RearLeftWheel.Offset = FVector(-1.3f, -0.85f, -0.2f);
	RearLeftWheel.MaxSteerAngle = 0.0f;
	RearLeftWheel.MaxHandbrakeTorque = 2500.0f;

	// 후륜 우측 (Wheel 3)
	RearRightWheel = RearLeftWheel;
	RearRightWheel.Offset = FVector(-1.3f, 0.85f, -0.2f);
}

FWheelConfig& FVehicleParams::GetWheel(EVehicleWheelOrder Order)
{
	switch (Order)
	{
	case EVehicleWheelOrder::FrontLeft:  return FrontLeftWheel;
	case EVehicleWheelOrder::FrontRight: return FrontRightWheel;
	case EVehicleWheelOrder::RearLeft:   return RearLeftWheel;
	case EVehicleWheelOrder::RearRight:  return RearRightWheel;
	default: return FrontLeftWheel;
	}
}

const FWheelConfig& FVehicleParams::GetWheel(EVehicleWheelOrder Order) const
{
	switch (Order)
	{
	case EVehicleWheelOrder::FrontLeft:  return FrontLeftWheel;
	case EVehicleWheelOrder::FrontRight: return FrontRightWheel;
	case EVehicleWheelOrder::RearLeft:   return RearLeftWheel;
	case EVehicleWheelOrder::RearRight:  return RearRightWheel;
	default: return FrontLeftWheel;
	}
}

FWheelConfig& FVehicleParams::GetWheel(int32 Index)
{
	return GetWheel(static_cast<EVehicleWheelOrder>(Index));
}

const FWheelConfig& FVehicleParams::GetWheel(int32 Index) const
{
	return GetWheel(static_cast<EVehicleWheelOrder>(Index));
}

FVector FVehicleParams::ComputeChassisMOI() const
{
	const float X = ChassisDimensions.X;
	const float Y = ChassisDimensions.Y;
	const float Z = ChassisDimensions.Z;
	const float Factor = ChassisMass / 12.0f;

	return FVector(
		Factor * (Y * Y + Z * Z), // Roll (X축 관성)
		Factor * (X * X + Z * Z), // Pitch (Y축 관성)
		Factor * (X * X + Y * Y)  // Yaw (Z축 관성)
	);
}

PxVehicleFrame GetKurisuVehicleFrame()
{
	PxVehicleFrame Frame;
	Frame.lngAxis = PxVehicleAxes::ePosX; // +X = 전진 (Longitudinal)
	Frame.latAxis = PxVehicleAxes::ePosY; // +Y = 우측 (Lateral)
	Frame.vrtAxis = PxVehicleAxes::ePosZ; // +Z = 상향 (Vertical)
	return Frame;
}

PxVehicleScale GetKurisuVehicleScale()
{
	PxVehicleScale Scale;
	Scale.scale = 1.0f; // 1 unit = 1 meter
	return Scale;
}
