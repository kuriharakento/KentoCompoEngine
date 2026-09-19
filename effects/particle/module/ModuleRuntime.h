#pragma once

#include "effects/particle/module/IModule.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace KCE
{
enum class ModuleParameterType : uint8_t
{
	Float, UInt, Int, Bool, Vector3, Vector4, String, Enum, Curve, Gradient, StructArray
};

using ModuleParameterValue = std::variant<float, uint32_t, int32_t, bool, Vector3, Vector4, std::string>;

enum class DynamicBindingMode : uint8_t { Constant, RandomRange, Curve, EmitterParameter };

struct DynamicBindingKey
{
	float time = 0.0f;
	Vector4 value = {};
};

struct DynamicParameterBinding
{
	std::string moduleId;
	std::string parameterId;
	ModuleParameterType type = ModuleParameterType::Float;
	DynamicBindingMode mode = DynamicBindingMode::Constant;
	ModuleParameterValue fallback = 0.0f;
	ModuleParameterValue minimum = 0.0f;
	ModuleParameterValue maximum = 0.0f;
	std::string emitterParameter;
	std::vector<DynamicBindingKey> keys;

	ModuleParameterValue Evaluate(float normalizedAge, uint32_t seed, const class ModuleParameterStore* parameters) const;
};

class ModuleParameterStore
{
public:
	void Set(std::string name, ModuleParameterValue value) { values_[std::move(name)] = std::move(value); }
	const ModuleParameterValue* Find(const std::string& name) const
	{
		auto it = values_.find(name); return it == values_.end() ? nullptr : &it->second;
	}
	template<class T> const T* FindAs(const std::string& name) const
	{
		const auto* value = Find(name); return value ? std::get_if<T>(value) : nullptr;
	}
	bool Remove(const std::string& name) { return values_.erase(name) != 0; }
	void Clear() { values_.clear(); }
	const std::unordered_map<std::string, ModuleParameterValue>& GetValues() const { return values_; }
	std::unordered_map<std::string, ModuleParameterValue>& GetValues() { return values_; }

private:
	std::unordered_map<std::string, ModuleParameterValue> values_;
};

struct ModuleParameterSchema
{
	std::string id;
	std::string displayName;
	ModuleParameterType type = ModuleParameterType::Float;
	ModuleParameterValue defaultValue = 0.0f;
	float minimum = 0.0f;
	float maximum = 0.0f;
	bool hasRange = false;
	bool dynamicInput = false;
};

struct ModuleDescriptor
{
	std::string id;
	std::string displayName;
	ModulePhase phase = ModulePhase::Update;
	int32_t defaultPriority = 0;
	uint32_t version = 1;
	bool cpuSupported = true;
	bool pureGpuSupported = false;
	std::string kernelId;
	std::vector<std::string> requiredAttributes;
	std::vector<std::string> producedAttributes;
	std::vector<ModuleParameterSchema> parameters;
	std::function<std::unique_ptr<IModule>()> factory;
};

struct CompiledModulePass
{
	const ModuleDescriptor* descriptor = nullptr;
	IModule* instance = nullptr;
	uint32_t parameterOffset = 0;
	uint32_t parameterSize = 0;
};

struct CompiledEmitter
{
	uint32_t schemaVersion = 1;
	uint64_t layoutHash = 0;
	std::vector<CompiledModulePass> passes;
	std::vector<uint8_t> packedParameters;
	std::vector<std::string> errors;
	bool valid = false;

	void Execute(ModulePhase phase, ParticleContext& context) const;
};

class ModuleDescriptorRegistry
{
public:
	static const ModuleDescriptorRegistry& GetInstance();
	const ModuleDescriptor* Find(const std::string& id) const;
	const std::vector<ModuleDescriptor>& GetDescriptors() const { return descriptors_; }
	CompiledEmitter Compile(const std::vector<std::unique_ptr<IModule>>& modules,
		const std::vector<DynamicParameterBinding>& bindings = {}) const;
	std::string GenerateCapabilityMarkdown() const;

private:
	ModuleDescriptorRegistry();
	std::vector<ModuleDescriptor> descriptors_;
	std::unordered_map<std::string, size_t> lookup_;
};
} // namespace KCE
