#include "PathManager.h"

namespace KCE
{
namespace
{
std::filesystem::path s_applicationResourceRoot = "application/Resources";
}

void PathManager::SetApplicationResourceRoot(const std::filesystem::path& path)
{
	s_applicationResourceRoot = path.lexically_normal();
}

const std::filesystem::path& PathManager::GetApplicationResourceRoot()
{
	return s_applicationResourceRoot;
}

std::filesystem::path PathManager::ResolveApplicationResource(const std::filesystem::path& relativePath)
{
	// すでに絶対パス、またはそのままで存在するなら使用
	if (relativePath.is_absolute() || std::filesystem::exists(relativePath))
	{
		return relativePath;
	}

	std::string relStr = relativePath.generic_string();
	std::filesystem::path cleanRelPath = relativePath;

	// 旧固定プレフィックスの除去
	if (relStr.rfind("../engine/Resources/", 0) == 0)
	{
		cleanRelPath = relStr.substr(20);
	}
	else if (relStr.rfind("application/Resources/", 0) == 0)
	{
		cleanRelPath = relStr.substr(22);
	}
	else if (relStr.rfind("./Resources/", 0) == 0)
	{
		cleanRelPath = relStr.substr(12);
	}
	else if (relStr.rfind("Resources/", 0) == 0)
	{
		cleanRelPath = relStr.substr(10);
	}

	std::filesystem::path combined = (s_applicationResourceRoot / cleanRelPath).lexically_normal();
	if (std::filesystem::exists(combined))
	{
		return combined;
	}

	return combined;
}
} // namespace KCE
