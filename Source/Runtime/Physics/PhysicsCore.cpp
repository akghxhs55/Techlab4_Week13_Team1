#include "EnginePCH.h"
#include "PhysicsCore.h"

#include "PxPhysicsAPI.h"

using namespace physx;

namespace
{
	// PhysX 메시지 -> 엔진 로그
	class FPhysXErrorCallback final : public PxErrorCallback
	{
	public:
		void reportError(PxErrorCode::Enum Code, const char* Message, const char* File, int Line) override
		{
			const char* Msg = Message ? Message : "";
			const char* Src = File ? File : "";
			switch (Code)
			{
			case PxErrorCode::eDEBUG_INFO:
				KRS_LOG(Info, "[PhysX] {}", Msg);
				break;
			case PxErrorCode::eDEBUG_WARNING:
			case PxErrorCode::ePERF_WARNING:
				KRS_LOG(Warning, "[PhysX] {} ({}:{})", Msg, Src, Line);
				break;
			default:
				KRS_LOG(Error, "[PhysX] {} ({}:{})", Msg, Src, Line);
				break;
			}
		}
	};

	PxDefaultAllocator GPhysXAllocator;
	FPhysXErrorCallback GPhysXErrorCallback;
}

bool FPhysicsCore::Init()
{
	if (IsInitialized())
	{
		return true;
	}

	Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, GPhysXAllocator, GPhysXErrorCallback);
	if (!Foundation)
	{
		KRS_LOG(Error, "[PhysX] Failed to create Foundation");
		return false;
	}

	Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *Foundation, PxTolerancesScale(), false, nullptr);
	if (!Physics)
	{
		KRS_LOG(Error, "[PhysX] Failed to create Physics");
		Shutdown();
		return false;
	}

	if (!PxInitExtensions(*Physics, nullptr))
	{
		KRS_LOG(Error, "[PhysX] Failed to initialize Extensions");
		Shutdown();
		return false;
	}
	bExtensionInitialized = true;

	CpuDispatcher = PxDefaultCpuDispatcherCreate(2);
	if (!CpuDispatcher)
	{
		KRS_LOG(Error, "[PhysX] Failed to create CpuDispatcher");
		Shutdown();
		return false;
	}

	DefaultMaterial = Physics->createMaterial(0.7f, 0.7f, 0.3f);

	KRS_LOG(Info, "[PhysX] Initialized {}.{}.{}", PX_PHYSICS_VERSION_MAJOR, PX_PHYSICS_VERSION_MINOR, PX_PHYSICS_VERSION_BUGFIX);

	return true;
}

void FPhysicsCore::Shutdown()
{
	if (DefaultMaterial)
	{
		DefaultMaterial->release();
		DefaultMaterial = nullptr;
	}

	if (CpuDispatcher)
	{
		CpuDispatcher->release();
		CpuDispatcher = nullptr;
	}

	if (bExtensionInitialized)
	{
		PxCloseExtensions();
		bExtensionInitialized = false;
	}

	if (Physics)
	{
		Physics->release();
		Physics = nullptr;
	}

	if (Foundation)
	{
		Foundation->release();
		Foundation = nullptr;
	}
}

physx::PxCpuDispatcher* FPhysicsCore::GetCpuDispatcher()
{
	return CpuDispatcher;
}
