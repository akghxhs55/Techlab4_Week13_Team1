#pragma once

#include <filesystem>
#include "Core/EngineString.h"

class UPhysicsAsset;

namespace PhysicsAssetFile
{
	bool Read(const std::filesystem::path& Path, UPhysicsAsset& Asset, FString& OutError);
	bool Write(const std::filesystem::path& Path, const UPhysicsAsset& Asset, FString& OutError);
}
