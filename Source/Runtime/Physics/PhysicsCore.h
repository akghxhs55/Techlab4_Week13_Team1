#pragma once

namespace physx
{
	class PxFoundation;
	class PxPhysics;
	class PxMaterial;
}

class FPhysicsCore
{
public:
	static bool Init();
	static void Shutdown();

	static bool IsInitialized() { return Physics != nullptr; }
	static physx::PxPhysics* GetPhysics() { return Physics; }
	static physx::PxMaterial* GetDefaultMaterial() { return DefaultMaterial; }

	static int32 RunConversionSelfTest();
	static bool RunFallingBoxTest();

private:
	inline static physx::PxFoundation* Foundation = nullptr;
	inline static physx::PxPhysics* Physics = nullptr;
	inline static physx::PxMaterial* DefaultMaterial = nullptr;
	inline static bool bExtensionInitialized = false;
};
