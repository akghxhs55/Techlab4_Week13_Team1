#include "EnginePCH.h"
#include "PhysicsAssetFile.h"

#include <fstream>
#include <stdexcept>
#include <system_error>
#include "json.hpp"
#include "PhysicsAsset.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	FVector ReadVector(const json& Value)
	{
		if (!Value.is_array() || Value.size() != 3)
			throw std::runtime_error("expected a three-element vector");
		return FVector(Value[0].get<float>(), Value[1].get<float>(), Value[2].get<float>());
	}

	FRotator ReadRotation(const json& Value)
	{
		const FVector Degrees = ReadVector(Value);
		return FRotator(Degrees.X, Degrees.Y, Degrees.Z);
	}

	EConstraintAngularMotion ReadAngularMotion(const json& Value, const char* Field)
	{
		const FString Motion = Value.value(Field, FString("Limited"));
		if (Motion == "Locked") return EConstraintAngularMotion::Locked;
		if (Motion == "Limited") return EConstraintAngularMotion::Limited;
		if (Motion == "Free") return EConstraintAngularMotion::Free;
		throw std::runtime_error("invalid angular constraint mode");
	}

	const char* WriteAngularMotion(EConstraintAngularMotion Motion)
	{
		switch (Motion)
		{
		case EConstraintAngularMotion::Locked: return "Locked";
		case EConstraintAngularMotion::Free: return "Free";
		default: return "Limited";
		}
	}

	void ReadShapes(const json& Shapes, FAggregateGeom& Geometry)
	{
		for (const json& Shape : Shapes)
		{
			const FString Type = Shape.at("Type").get<FString>();
			const FVector Center = Shape.contains("Center") ? ReadVector(Shape.at("Center")) : FVector::ZeroVector;
			const FRotator Rotation = Shape.contains("Rotation") ? ReadRotation(Shape.at("Rotation")) : FRotator(0, 0, 0);
			if (Type == "Capsule")
			{
				FCapsuleElem Element;
				Element.Center = Center;
				Element.Rotation = Rotation;
				Element.Radius = Shape.at("Radius").get<float>();
				Element.Length = Shape.at("Length").get<float>();
				if (Element.Radius <= 0.0f || Element.Length < 0.0f)
					throw std::runtime_error("invalid capsule dimensions");
				Geometry.CapsuleElems.Add(Element);
			}
			else if (Type == "Box")
			{
				FBoxElem Element;
				Element.Center = Center;
				Element.Rotation = Rotation;
				const FVector Size = ReadVector(Shape.at("Size"));
				Element.X = Size.X; Element.Y = Size.Y; Element.Z = Size.Z;
				if (Element.X <= 0.0f || Element.Y <= 0.0f || Element.Z <= 0.0f)
					throw std::runtime_error("invalid box dimensions");
				Geometry.BoxElems.Add(Element);
			}
			else if (Type == "Sphere")
			{
				FSphereElem Element;
				Element.Center = Center;
				Element.Radius = Shape.at("Radius").get<float>();
				if (Element.Radius <= 0.0f)
					throw std::runtime_error("invalid sphere radius");
				Geometry.SphereElems.Add(Element);
			}
			else
				throw std::runtime_error("unsupported shape type");
		}
	}

	json WriteVector(const FVector& Value)
	{
		return { Value.X, Value.Y, Value.Z };
	}

	json WriteRotation(const FRotator& Value)
	{
		return { Value.Pitch, Value.Yaw, Value.Roll };
	}
}

bool PhysicsAssetFile::Read(const std::filesystem::path& Path, UPhysicsAsset& Asset, FString& OutError)
{
	OutError.clear();
	std::ifstream File(Path);
	if (!File)
	{
		OutError = "cannot open Physics Asset";
		return false;
	}
	try
	{
		const json Data = json::parse(File);
		if (Data.at("Version").get<int32>() != 1)
			throw std::runtime_error("unsupported Physics Asset version");

		for (const json& Body : Data.at("Bodies"))
		{
			auto* Setup = NewObject<USkeletalBodySetup>(&Asset);
			Asset.SkeletalBodySetups.Add(Setup);
			Setup->BoneName = Body.at("Bone").get<FString>();
			Setup->DefaultInstance.MassInKgOverride = Body.value("Mass", 1.0f);
			Setup->DefaultInstance.LinearDamping = Body.value("LinearDamping", 0.01f);
			Setup->DefaultInstance.AngularDamping = Body.value("AngularDamping", 0.05f);
			const FString Collision = Body.value("Collision", FString("QueryOnly"));
			if (Collision == "QueryOnly") Setup->DefaultInstance.CollisionEnabled = ECollisionEnabled::QueryOnly;
			else if (Collision == "PhysicsOnly") Setup->DefaultInstance.CollisionEnabled = ECollisionEnabled::PhysicsOnly;
			else if (Collision == "QueryAndPhysics") Setup->DefaultInstance.CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
			else if (Collision == "NoCollision") Setup->DefaultInstance.CollisionEnabled = ECollisionEnabled::NoCollision;
			else throw std::runtime_error("invalid collision mode");
			if (Setup->DefaultInstance.MassInKgOverride <= 0.0f)
				throw std::runtime_error("body mass must be positive");
			ReadShapes(Body.at("Shapes"), Setup->AggGeom);
		}
		for (const json& Constraint : Data.at("Constraints"))
		{
			auto* Template = NewObject<UPhysicsConstraintTemplate>(&Asset);
			Asset.ConstraintSetup.Add(Template);
			FConstraintInstance& Default = Template->DefaultInstance;
			Default.ConstraintBone1 = Constraint.at("Child").get<FString>();
			Default.ConstraintBone2 = Constraint.at("Parent").get<FString>();
			Default.TwistLimitRadians = FMath::DegreesToRadians(Constraint.value("TwistDegrees", 45.0f));
			Default.TwistLowerRadians = FMath::DegreesToRadians(Constraint.value("TwistLowerDegrees",
				-FMath::RadiansToDegrees(Default.TwistLimitRadians)));
			Default.TwistUpperRadians = FMath::DegreesToRadians(Constraint.value("TwistUpperDegrees",
				FMath::RadiansToDegrees(Default.TwistLimitRadians)));
			if (Constraint.contains("JointFrameRotation"))
				Default.JointFrameRotation = ReadRotation(Constraint.at("JointFrameRotation"));
			Default.Swing1LimitRadians = FMath::DegreesToRadians(Constraint.value("Swing1Degrees", 30.0f));
			Default.Swing2LimitRadians = FMath::DegreesToRadians(Constraint.value("Swing2Degrees", 30.0f));
			Default.TwistMotion = ReadAngularMotion(Constraint, "TwistMotion");
			Default.Swing1Motion = ReadAngularMotion(Constraint, "Swing1Motion");
			Default.Swing2Motion = ReadAngularMotion(Constraint, "Swing2Motion");
			Default.bDisableCollision = Constraint.value("DisableCollision", true);
			if (Default.TwistLimitRadians <= 0.0f || Default.TwistLimitRadians >= PI ||
				Default.TwistLowerRadians >= Default.TwistUpperRadians ||
				Default.TwistLowerRadians <= -PI || Default.TwistUpperRadians >= PI ||
				Default.Swing1LimitRadians <= 0.0f || Default.Swing1LimitRadians >= PI ||
				Default.Swing2LimitRadians <= 0.0f || Default.Swing2LimitRadians >= PI)
				throw std::runtime_error("invalid constraint angle");
		}
		return true;
	}
	catch (const std::exception& Error)
	{
		OutError = Error.what();
		return false;
	}
}

bool PhysicsAssetFile::Write(const std::filesystem::path& Path, const UPhysicsAsset& Asset, FString& OutError)
{
	OutError.clear();
	if (Path.empty() || std::filesystem::exists(Path))
	{
		OutError = "Physics Asset path is empty or already exists";
		return false;
	}
	json Data;
	Data["Version"] = 1;
	Data["Bodies"] = json::array();
	Data["Constraints"] = json::array();
	for (const USkeletalBodySetup* Setup : Asset.SkeletalBodySetups)
	{
		if (!Setup) { OutError = "Physics Asset contains a null body"; return false; }
		json Body;
		Body["Bone"] = Setup->BoneName;
		Body["Mass"] = Setup->DefaultInstance.MassInKgOverride;
		Body["LinearDamping"] = Setup->DefaultInstance.LinearDamping;
		Body["AngularDamping"] = Setup->DefaultInstance.AngularDamping;
		switch (Setup->DefaultInstance.CollisionEnabled)
		{
		case ECollisionEnabled::NoCollision: Body["Collision"] = "NoCollision"; break;
		case ECollisionEnabled::QueryOnly: Body["Collision"] = "QueryOnly"; break;
		case ECollisionEnabled::PhysicsOnly: Body["Collision"] = "PhysicsOnly"; break;
		case ECollisionEnabled::QueryAndPhysics: Body["Collision"] = "QueryAndPhysics"; break;
		}
		Body["Shapes"] = json::array();
		for (const FSphereElem& Shape : Setup->AggGeom.SphereElems)
			Body["Shapes"].push_back({ {"Type", "Sphere"}, {"Center", WriteVector(Shape.Center)}, {"Radius", Shape.Radius} });
		for (const FBoxElem& Shape : Setup->AggGeom.BoxElems)
			Body["Shapes"].push_back({ {"Type", "Box"}, {"Center", WriteVector(Shape.Center)},
				{"Rotation", WriteRotation(Shape.Rotation)}, {"Size", WriteVector(FVector(Shape.X, Shape.Y, Shape.Z))} });
		for (const FCapsuleElem& Shape : Setup->AggGeom.CapsuleElems)
			Body["Shapes"].push_back({ {"Type", "Capsule"}, {"Center", WriteVector(Shape.Center)},
				{"Rotation", WriteRotation(Shape.Rotation)}, {"Radius", Shape.Radius}, {"Length", Shape.Length} });
		Data["Bodies"].push_back(std::move(Body));
	}
	for (const UPhysicsConstraintTemplate* Template : Asset.ConstraintSetup)
	{
		if (!Template) { OutError = "Physics Asset contains a null constraint"; return false; }
		const FConstraintInstance& Constraint = Template->DefaultInstance;
		Data["Constraints"].push_back({ {"Child", Constraint.ConstraintBone1}, {"Parent", Constraint.ConstraintBone2},
			{"TwistDegrees", FMath::RadiansToDegrees(Constraint.TwistLimitRadians)},
			{"TwistLowerDegrees", FMath::RadiansToDegrees(Constraint.TwistLowerRadians)},
			{"TwistUpperDegrees", FMath::RadiansToDegrees(Constraint.TwistUpperRadians)},
			{"JointFrameRotation", WriteRotation(Constraint.JointFrameRotation)},
			{"Swing1Degrees", FMath::RadiansToDegrees(Constraint.Swing1LimitRadians)},
			{"Swing2Degrees", FMath::RadiansToDegrees(Constraint.Swing2LimitRadians)},
			{"TwistMotion", WriteAngularMotion(Constraint.TwistMotion)},
			{"Swing1Motion", WriteAngularMotion(Constraint.Swing1Motion)},
			{"Swing2Motion", WriteAngularMotion(Constraint.Swing2Motion)},
			{"DisableCollision", Constraint.bDisableCollision} });
	}
	std::filesystem::path Temp = Path;
	Temp += ".tmp";
	{
		std::ofstream File(Temp, std::ios::trunc);
		if (!File) { OutError = "cannot open temporary Physics Asset file"; return false; }
		File << Data.dump(2);
		File.close();
		if (!File)
		{
			std::filesystem::remove(Temp);
			OutError = "failed to write Physics Asset file";
			return false;
		}
	}
	std::error_code Error;
	std::filesystem::rename(Temp, Path, Error);
	if (Error)
	{
		std::filesystem::remove(Temp);
		OutError = Error.message();
		return false;
	}
	return true;
}
