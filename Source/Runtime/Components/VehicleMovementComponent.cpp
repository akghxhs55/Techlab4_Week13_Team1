#include "EnginePCH.h"
#include "VehicleMovementComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Physics/PhysScene.h"
#include "Math/EngineMath.h"
#include "UObject/Casts.h"

#include <cmath>

UVehicleMovementComponent::UVehicleMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	Velocity = FVector::ZeroVector;

	VehicleParams.SetupDefaultCar();
}

void UVehicleMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UVehicleMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	TryInitVehicle();
}

bool UVehicleMovementComponent::TryInitVehicle()
{
	if (bVehicleInitialized)
	{
		return true;
	}

	UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(UpdatedComponent);
	if (!Prim && GetOwner())
	{
		Prim = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
		if (Prim && !UpdatedComponent)
		{
			SetUpdatedComponent(Prim);
		}
	}

	if (!Prim)
	{
		return false;
	}

	// 물리 시뮬레이션 보장
	if (!Prim->IsSimulatingPhysics())
	{
		Prim->SetSimulatePhysics(true);
	}

	if (Prim->BodyInstance.IsValidBodyInstance())
	{
		physx::PxRigidDynamic* RigidDynamic = Prim->BodyInstance.GetPxRigidDynamic();
		if (RigidDynamic)
		{
			// 에디터 수정 프로퍼티 반영
			VehicleParams.PeakTorque = PeakTorque;
			VehicleParams.ChassisMass = ChassisMass;
			VehicleParams.FrontLeftWheel.MaxSteerAngle = MaxSteerAngle;
			VehicleParams.FrontRightWheel.MaxSteerAngle = MaxSteerAngle;

			for (int32 i = 0; i < 4; ++i)
			{
				FWheelConfig& Wheel = VehicleParams.GetWheel(i);
				Wheel.SpringStiffness = SuspensionStiffness;
				Wheel.SpringDamping = SuspensionDamping;
				Wheel.Radius = WheelRadius;
			}

			SimulationPipeline.Initialize(VehicleParams, RigidDynamic);
			bVehicleInitialized = true;
			return true;
		}
	}

	return false;
}

void UVehicleMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (!bVehicleInitialized)
	{
		if (!TryInitVehicle())
		{
			return;
		}
	}

	// 1. Pawn 입력 벡터 처리 (WASD 플레이어 컨트롤러 대응)
	const FVector InputVector = ConsumeInputVector();
	if (InputVector.SizeSquared() > 0.0001f && UpdatedComponent)
	{
		const FQuat WorldRot = UpdatedComponent->GetWorldRotation().Quaternion();
		const FVector ForwardDir = WorldRot.GetForwardVector();
		const FVector RightDir = WorldRot.GetRightVector();
		const float ForwardInput = InputVector.Dot(ForwardDir);
		const float RightInput = InputVector.Dot(RightDir);

		const float ForwardSpeed = GetForwardSpeed();
		if (ForwardInput > 0.0f)
		{
			RawThrottleInput = ForwardInput;
			RawBrakeInput = 0.0f;
		}
		else if (ForwardInput < 0.0f)
		{
			if (bReverseAsBrake && ForwardSpeed > 1.0f)
			{
				RawThrottleInput = 0.0f;
				RawBrakeInput = -ForwardInput;
			}
			else
			{
				RawThrottleInput = ForwardInput;
				RawBrakeInput = 0.0f;
			}
		}
		else
		{
			RawThrottleInput = 0.0f;
			RawBrakeInput = 0.0f;
		}

		RawSteeringInput = FMath::Clamp(RightInput, -1.0f, 1.0f);
	}

	// 2. 조작 입력 스무딩 보간
	CurrentThrottle = FMath::FInterpTo(CurrentThrottle, RawThrottleInput, DeltaTime, ThrottleInputRate);
	CurrentSteer = FMath::FInterpTo(CurrentSteer, RawSteeringInput, DeltaTime, SteeringInputRate);
	CurrentBrake = FMath::FInterpTo(CurrentBrake, RawBrakeInput, DeltaTime, 10.0f);

	// 3. PhysX 시뮬레이션 파이프라인 구동
	UWorld* World = GetWorld();
	if (World && World->GetPhysicsScene())
	{
		physx::PxScene* PxScene = World->GetPhysicsScene()->GetPxScene();
		if (PxScene)
		{
			SimulationPipeline.Update(DeltaTime, PxScene, CurrentThrottle, CurrentSteer, CurrentBrake, bRawHandbrakeInput);
		}
	}

	// 4. 컴포넌트 Velocity 동기화
	if (UpdatedComponent)
	{
		Velocity = UpdatedComponent->GetWorldRotation().Quaternion().GetForwardVector() * GetForwardSpeed();
	}
}

void UVehicleMovementComponent::SetThrottleInput(float InThrottle)
{
	RawThrottleInput = FMath::Clamp(InThrottle, -1.0f, 1.0f);
}

void UVehicleMovementComponent::SetSteeringInput(float InSteering)
{
	RawSteeringInput = FMath::Clamp(InSteering, -1.0f, 1.0f);
}

void UVehicleMovementComponent::SetBrakeInput(float InBrake)
{
	RawBrakeInput = FMath::Clamp(InBrake, 0.0f, 1.0f);
}

void UVehicleMovementComponent::SetHandbrakeInput(bool bInHandbrake)
{
	bRawHandbrakeInput = bInHandbrake;
}

void UVehicleMovementComponent::SetVehicleParams(const FVehicleParams& InParams)
{
	VehicleParams = InParams;
	if (bVehicleInitialized)
	{
		SimulationPipeline.SetParams(VehicleParams);
	}
}

float UVehicleMovementComponent::GetForwardSpeed() const
{
	if (!bVehicleInitialized)
	{
		return 0.0f;
	}
	return SimulationPipeline.GetForwardSpeed();
}

float UVehicleMovementComponent::GetForwardSpeedKmH() const
{
	return GetForwardSpeed() * 3.6f;
}

bool UVehicleMovementComponent::IsInAir() const
{
	if (!bVehicleInitialized)
	{
		return true;
	}

	for (int32 i = 0; i < 4; ++i)
	{
		if (SimulationPipeline.GetWheelState(i).bInContact)
		{
			return false;
		}
	}
	return true;
}

const FWheelSimulationState& UVehicleMovementComponent::GetWheelState(EVehicleWheelOrder Order) const
{
	return SimulationPipeline.GetWheelState(Order);
}

const FWheelSimulationState& UVehicleMovementComponent::GetWheelState(int32 WheelIndex) const
{
	return SimulationPipeline.GetWheelState(WheelIndex);
}

FTransform UVehicleMovementComponent::GetWheelLocalTransform(int32 WheelIndex) const
{
	if (!bVehicleInitialized)
	{
		return FTransform::Identity;
	}
	return SimulationPipeline.GetWheelState(WheelIndex).LocalPose;
}

FTransform UVehicleMovementComponent::GetWheelWorldTransform(int32 WheelIndex) const
{
	const FTransform LocalPose = GetWheelLocalTransform(WheelIndex);
	if (UpdatedComponent)
	{
		const FVector BodyLoc = UpdatedComponent->GetWorldLocation();
		const FQuat BodyRot = UpdatedComponent->GetWorldRotation().Quaternion();

		FTransform WorldPose;
		WorldPose.Location = BodyLoc + BodyRot.RotateVector(LocalPose.Location);
		WorldPose.Rotation = BodyRot * LocalPose.Rotation;
		WorldPose.Scale = LocalPose.Scale;
		return WorldPose;
	}
	return LocalPose;
}
