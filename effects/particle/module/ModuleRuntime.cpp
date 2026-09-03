#include "ModuleRuntime.h"
#include "effects/particle/ParticleEmitter.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/spawn/SpawnShapeModules.h"
#include "effects/particle/module/spawn/SubEmitterModule.h"
#include "effects/particle/module/update/UpdateModules.h"
#include "effects/particle/module/update/AdvancedModules.h"
#include "effects/particle/module/update/BehaviorModules.h"
#include "effects/particle/module/update/ForceFieldModules.h"
#include "effects/particle/module/update/RibbonModules.h"
#include "effects/particle/module/update/TextureSheetModule.h"
#include "effects/particle/module/update/MotionEffectModules.h"
#include "effects/particle/module/update/NaturalBehaviorModules.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <unordered_set>
#include <sstream>

namespace KCE
{
namespace
{
float BindingRandom01(uint32_t seed)
{
	seed ^= seed >> 16; seed *= 0x7feb352du; seed ^= seed >> 15; seed *= 0x846ca68bu; seed ^= seed >> 16;
	return static_cast<float>(seed & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

Vector4 SampleBindingCurve(const std::vector<DynamicBindingKey>& keys, float time)
{
	if (keys.empty()) return {};
	time = (std::clamp)(time, 0.0f, 1.0f);
	if (time <= keys.front().time) return keys.front().value;
	if (time >= keys.back().time) return keys.back().value;
	for (size_t i = 0; i + 1 < keys.size(); ++i)
	{
		if (time < keys[i].time || time > keys[i + 1].time) continue;
		const float span = keys[i + 1].time - keys[i].time;
		const float ratio = span > 0.0f ? (time - keys[i].time) / span : 0.0f;
		return { keys[i].value.x + (keys[i + 1].value.x - keys[i].value.x) * ratio,
			keys[i].value.y + (keys[i + 1].value.y - keys[i].value.y) * ratio,
			keys[i].value.z + (keys[i + 1].value.z - keys[i].value.z) * ratio,
			keys[i].value.w + (keys[i + 1].value.w - keys[i].value.w) * ratio };
	}
	return keys.back().value;
}
uint64_t HashBytes(uint64_t hash, const void* data, size_t size)
{
	const auto* bytes = static_cast<const uint8_t*>(data);
	for (size_t i = 0; i < size; ++i) { hash ^= bytes[i]; hash *= 1099511628211ull; }
	return hash;
}

uint64_t HashString(uint64_t hash, const std::string& value)
{
	return HashBytes(hash, value.data(), value.size());
}

ModuleDescriptor MakeDescriptor(const char* id, ModulePhase phase, bool gpu,
	std::initializer_list<const char*> required = {}, std::initializer_list<const char*> produced = {})
{
	ModuleDescriptor result;
	result.id = id;
	result.displayName = id;
	result.phase = phase;
	result.pureGpuSupported = gpu;
	result.kernelId = gpu ? std::string("Particle/") + id : std::string();
	for (const char* value : required) result.requiredAttributes.emplace_back(value);
	for (const char* value : produced) result.producedAttributes.emplace_back(value);
	return result;
}

ModuleParameterSchema FloatParameter(const char* id, float value, float minValue, float maxValue, bool dynamic = true)
{
	return { id, id, ModuleParameterType::Float, value, minValue, maxValue, true, dynamic };
}

ModuleParameterSchema UIntParameter(const char* id, uint32_t value, uint32_t minValue, uint32_t maxValue)
{
	return { id, id, ModuleParameterType::UInt, value, static_cast<float>(minValue), static_cast<float>(maxValue), true, false };
}

ModuleParameterSchema IntParameter(const char* id, int32_t value, int32_t minValue, int32_t maxValue)
{
	return { id, id, ModuleParameterType::Int, value, static_cast<float>(minValue), static_cast<float>(maxValue), true, false };
}

ModuleParameterSchema BoolParameter(const char* id, bool value)
{
	return { id, id, ModuleParameterType::Bool, value, 0.0f, 1.0f, false, false };
}

ModuleParameterSchema Vector3Parameter(const char* id, Vector3 value = {}, bool dynamic = true)
{
	return { id, id, ModuleParameterType::Vector3, value, 0.0f, 0.0f, false, dynamic };
}

ModuleParameterSchema Vector4Parameter(const char* id, Vector4 value = { 1, 1, 1, 1 }, bool dynamic = true)
{
	return { id, id, ModuleParameterType::Vector4, value, 0.0f, 0.0f, false, dynamic };
}

ModuleParameterSchema EnumParameter(const char* id, int32_t value, int32_t minValue, int32_t maxValue)
{
	auto result = IntParameter(id, value, minValue, maxValue); result.type = ModuleParameterType::Enum; return result;
}

ModuleParameterSchema ComplexParameter(const char* id, ModuleParameterType type)
{
	return { id, id, type, std::string{}, 0.0f, 0.0f, false, false };
}

bool ParameterValueMatchesType(const ModuleParameterSchema& parameter)
{
	switch (parameter.type)
	{
	case ModuleParameterType::Float: return std::holds_alternative<float>(parameter.defaultValue);
	case ModuleParameterType::UInt: return std::holds_alternative<uint32_t>(parameter.defaultValue);
	case ModuleParameterType::Int:
	case ModuleParameterType::Enum: return std::holds_alternative<int32_t>(parameter.defaultValue);
	case ModuleParameterType::Bool: return std::holds_alternative<bool>(parameter.defaultValue);
	case ModuleParameterType::Vector3: return std::holds_alternative<Vector3>(parameter.defaultValue);
	case ModuleParameterType::Vector4: return std::holds_alternative<Vector4>(parameter.defaultValue);
	case ModuleParameterType::String:
	case ModuleParameterType::Curve:
	case ModuleParameterType::Gradient:
	case ModuleParameterType::StructArray: return std::holds_alternative<std::string>(parameter.defaultValue);
	}
	return false;
}
}

ModuleParameterValue DynamicParameterBinding::Evaluate(float normalizedAge, uint32_t seed, const ModuleParameterStore* parameters) const
{
	if (mode == DynamicBindingMode::EmitterParameter)
	{
		if (parameters) if (const ModuleParameterValue* value = parameters->Find(emitterParameter); value && value->index() == fallback.index()) return *value;
		return fallback;
	}
	if (mode == DynamicBindingMode::Constant) return fallback;
	if (mode == DynamicBindingMode::Curve)
	{
		const Vector4 value = SampleBindingCurve(keys, normalizedAge);
		if (type == ModuleParameterType::Float) return value.x;
		if (type == ModuleParameterType::Vector3) return Vector3{ value.x, value.y, value.z };
		if (type == ModuleParameterType::Vector4) return value;
		return fallback;
	}
	if (mode == DynamicBindingMode::RandomRange)
	{
		const float rx = BindingRandom01(seed);
		if (const auto* low = std::get_if<float>(&minimum)) if (const auto* high = std::get_if<float>(&maximum)) return *low + (*high - *low) * rx;
		if (const auto* low = std::get_if<Vector3>(&minimum)) if (const auto* high = std::get_if<Vector3>(&maximum)) return Vector3{
			low->x + (high->x - low->x) * rx, low->y + (high->y - low->y) * BindingRandom01(seed ^ 0x9e3779b9u), low->z + (high->z - low->z) * BindingRandom01(seed ^ 0x85ebca6bu) };
		if (const auto* low = std::get_if<Vector4>(&minimum)) if (const auto* high = std::get_if<Vector4>(&maximum)) return Vector4{
			low->x + (high->x - low->x) * rx, low->y + (high->y - low->y) * BindingRandom01(seed ^ 0x9e3779b9u),
			low->z + (high->z - low->z) * BindingRandom01(seed ^ 0x85ebca6bu), low->w + (high->w - low->w) * BindingRandom01(seed ^ 0xc2b2ae35u) };
	}
	return fallback;
}

void CompiledEmitter::Execute(ModulePhase phase, ParticleContext& context) const
{
	if (!valid) return;
	for (const auto& pass : passes)
	{
		if (pass.descriptor && pass.descriptor->phase == phase && pass.instance) pass.instance->Execute(context);
	}
}

const ModuleDescriptorRegistry& ModuleDescriptorRegistry::GetInstance()
{
	static ModuleDescriptorRegistry instance;
	return instance;
}

ModuleDescriptorRegistry::ModuleDescriptorRegistry()
{
	auto add = [&](ModuleDescriptor descriptor)
	{
		if (!lookup_.emplace(descriptor.id, descriptors_.size()).second)
			throw std::logic_error("Duplicate particle module descriptor: " + descriptor.id);
		descriptors_.push_back(std::move(descriptor));
	};

	auto spawnRate = MakeDescriptor("SpawnRate", ModulePhase::Spawn, true, {}, { "SpawnCount" });
	spawnRate.parameters = { FloatParameter("rate", 10.0f, 0.0f, 1000000.0f) }; add(std::move(spawnRate));
	auto burst = MakeDescriptor("SpawnBurst", ModulePhase::Spawn, true, {}, { "SpawnCount" });
	burst.parameters = { UIntParameter("count", 10, 0, 1000000), FloatParameter("delay", 0, 0, 86400, false), FloatParameter("interval", 0, 0, 86400, false), IntParameter("loops", 1, -1, 1000000) }; add(std::move(burst));
	add(MakeDescriptor("SpawnShape", ModulePhase::Spawn, true, {}, { "Position", "Velocity" }));
	add(MakeDescriptor("InitialPosition", ModulePhase::Spawn, false, {}, { "Position" }));
	add(MakeDescriptor("InitialVelocity", ModulePhase::Spawn, true, {}, { "Velocity" }));
	add(MakeDescriptor("InitialLifetime", ModulePhase::Spawn, true, {}, { "Lifetime" }));
	add(MakeDescriptor("InitialColor", ModulePhase::Spawn, true, {}, { "Color" }));
	add(MakeDescriptor("InitialScale", ModulePhase::Spawn, true, {}, { "Scale" }));
	add(MakeDescriptor("InitialRotation", ModulePhase::Spawn, false, {}, { "Rotation" }));
	add(MakeDescriptor("AssignRibbonId", ModulePhase::Spawn, true, {}, { "RibbonId" }));
	add(MakeDescriptor("SubEmitter", ModulePhase::Update, false));

	const char* gpuUpdates[] = { "Gravity", "Drag", "ColorFade", "ScaleOverLifetime", "RotationOverLifetime", "Noise", "VelocityOverLifetime", "StretchByVelocity", "Flicker", "AlphaFade", "FaceVelocity" };
	for (const char* id : gpuUpdates) add(MakeDescriptor(id, ModulePhase::Update, true, { "Age", "Lifetime" }));
	const char* cpuUpdates[] = { "Acceleration", "CurlNoise", "SizeBySpeed", "ColorBySpeed", "Collision", "KillZone", "SprintToTarget", "Attractor", "Vortex", "Orbit", "VelocityLimit", "TextureSheet", "Wind", "RotationBySpeed", "SineWave", "Spiral", "Twist", "Jitter", "ForceOverLifetime" };
	for (const char* id : cpuUpdates) add(MakeDescriptor(id, ModulePhase::Update, false));
	add(MakeDescriptor("RibbonInterpolation", ModulePhase::Spawn, false));
	add(MakeDescriptor("MultiSourceRibbon", ModulePhase::Spawn, false));
	add(MakeDescriptor("RadialVelocity", ModulePhase::Spawn, false, { "Position" }, { "Velocity" }));

	auto setParams = [&](const char* id, std::initializer_list<ModuleParameterSchema> parameters)
	{
		descriptors_[lookup_.at(id)].parameters.assign(parameters.begin(), parameters.end());
	};
	setParams("SpawnShape", { EnumParameter("shapeType", 0, 0, 5), FloatParameter("innerRadius", 0, 0, 100000), FloatParameter("outerRadius", 1, 0, 100000), Vector3Parameter("boxSize", {1,1,1}), FloatParameter("coneHeight", 1, 0, 100000), Vector3Parameter("lineStart"), Vector3Parameter("lineEnd", {0,1,0}), BoolParameter("emitFromSurface", false), FloatParameter("initialSpeed", 0, -100000, 100000), EnumParameter("spawnLocation", 0, 0, 2), FloatParameter("arcAngle", 360, 0, 360) });
	setParams("InitialPosition", { Vector3Parameter("min"), Vector3Parameter("max") });
	setParams("InitialVelocity", { Vector3Parameter("min"), Vector3Parameter("max") });
	setParams("InitialLifetime", { FloatParameter("min", 1, 0.001f, 100000), FloatParameter("max", 1, 0.001f, 100000) });
	setParams("InitialColor", { Vector4Parameter("min"), Vector4Parameter("max") });
	setParams("InitialScale", { Vector3Parameter("min", {1,1,1}), Vector3Parameter("max", {1,1,1}) });
	setParams("InitialRotation", { Vector3Parameter("minAngle"), Vector3Parameter("maxAngle") });
	setParams("AssignRibbonId", { UIntParameter("groupCount", 1, 1, 65536) });
	setParams("SubEmitter", { ComplexParameter("configs", ModuleParameterType::StructArray) });
	setParams("Gravity", { Vector3Parameter("min", {0,-9.8f,0}), Vector3Parameter("max", {0,-9.8f,0}) });
	setParams("Drag", { FloatParameter("min", 0.1f, 0, 1000), FloatParameter("max", 0.1f, 0, 1000) });
	setParams("ColorFade", { BoolParameter("useInitialColor", true), Vector4Parameter("start"), Vector4Parameter("end", {1,1,1,0}), EnumParameter("easingType", 0, 0, 12), ComplexParameter("gradient", ModuleParameterType::Gradient) });
	setParams("ScaleOverLifetime", { Vector3Parameter("start", {1,1,1}), Vector3Parameter("end", {1,1,1}), EnumParameter("easingType", 0, 0, 12), ComplexParameter("curve", ModuleParameterType::Curve) });
	setParams("Acceleration", { Vector3Parameter("acceleration") });
	setParams("CurlNoise", { FloatParameter("strength", 1, 0, 100000), FloatParameter("frequency", 1, 0, 100000), IntParameter("octaves", 1, 1, 16), FloatParameter("scrollSpeed", 0, -100000, 100000) });
	setParams("SizeBySpeed", { FloatParameter("minSpeed", 0, 0, 100000), FloatParameter("maxSpeed", 10, 0, 100000), Vector3Parameter("minScale", {1,1,1}), Vector3Parameter("maxScale", {1,1,1}) });
	setParams("ColorBySpeed", { FloatParameter("minSpeed", 0, 0, 100000), FloatParameter("maxSpeed", 10, 0, 100000), Vector4Parameter("minColor"), Vector4Parameter("maxColor") });
	setParams("Collision", { EnumParameter("mode", 0, 0, 2), FloatParameter("bounce", 0.5f, 0, 1), FloatParameter("friction", 0, 0, 1), FloatParameter("planeHeight", 0, -100000, 100000), Vector3Parameter("boxCenter"), Vector3Parameter("boxSize", {1,1,1}), BoolParameter("killOnCollision", false) });
	setParams("KillZone", { EnumParameter("zoneType", 0, 0, 1), Vector3Parameter("center"), Vector3Parameter("boxSize", {1,1,1}), FloatParameter("radius", 1, 0, 100000), BoolParameter("killInside", true) });
	setParams("SprintToTarget", { Vector3Parameter("target"), FloatParameter("acceleration", 1, 0, 100000), FloatParameter("arriveRadius", 0.1f, 0, 100000), BoolParameter("killOnArrive", false), FloatParameter("maxDistance", 1000, 0, 1000000), FloatParameter("speedBoost", 1, 0, 100000), BoolParameter("useSpeedCurve", false) });
	setParams("RotationOverLifetime", { FloatParameter("startSpeed", 0, -100000, 100000), FloatParameter("endSpeed", 0, -100000, 100000), EnumParameter("easingType", 0, 0, 12) });
	setParams("FaceVelocity", { BoolParameter("use2DAlignment", false) });
	setParams("Jitter", { Vector3Parameter("amount") });
	setParams("ForceOverLifetime", { Vector3Parameter("direction", {0,1,0}), FloatParameter("startStrength", 0, -100000, 100000), FloatParameter("endStrength", 0, -100000, 100000), EnumParameter("easingType", 0, 0, 12) });
	setParams("Orbit", { FloatParameter("orbitSpeed", 90, -100000, 100000), Vector3Parameter("orbitAxis", {0,1,0}) });
	setParams("Noise", { FloatParameter("strength", 1, 0, 100000), FloatParameter("frequency", 1, 0, 100000) });
	setParams("VelocityLimit", { FloatParameter("maxSpeed", 10, 0, 100000) });
	setParams("Attractor", { Vector3Parameter("target"), FloatParameter("strength", 1, -100000, 100000), FloatParameter("range", 10, 0, 100000), EnumParameter("falloffType", 0, 0, 2) });
	setParams("Vortex", { Vector3Parameter("axis", {0,1,0}), Vector3Parameter("center"), FloatParameter("strength", 1, -100000, 100000), FloatParameter("range", 10, 0, 100000) });
	setParams("RibbonInterpolation", { FloatParameter("maxDistance", 1, 0.001f, 100000) });
	setParams("MultiSourceRibbon", { FloatParameter("spawnRate", 60, 0, 100000), FloatParameter("particleLifetime", 1, 0.001f, 100000), BoolParameter("spawnOnlyWhenMoving", false), FloatParameter("minMoveDistance", 0.05f, 0, 100000), Vector4Parameter("initialColor") });
	setParams("TextureSheet", { UIntParameter("columns", 1, 1, 4096), UIntParameter("rows", 1, 1, 4096), FloatParameter("frameRate", 30, 0, 100000), EnumParameter("playMode", 0, 0, 2) });
	setParams("RadialVelocity", { FloatParameter("minSpeed", 0, -100000, 100000), FloatParameter("maxSpeed", 1, -100000, 100000) });
	setParams("VelocityOverLifetime", { FloatParameter("startMultiplier", 1, -100000, 100000), FloatParameter("endMultiplier", 1, -100000, 100000) });
	setParams("StretchByVelocity", { FloatParameter("stretchFactor", 1, 0, 100000), FloatParameter("minStretch", 1, 0, 100000), FloatParameter("maxStretch", 10, 0, 100000), BoolParameter("preserveVolume", false) });
	setParams("Wind", { Vector3Parameter("direction", {1,0,0}), FloatParameter("strength", 1, -100000, 100000), FloatParameter("turbulence", 0, 0, 100000), FloatParameter("turbulenceFrequency", 1, 0, 100000) });
	setParams("Flicker", { FloatParameter("frequency", 10, 0, 100000), FloatParameter("minAlpha", 0, 0, 1), FloatParameter("maxAlpha", 1, 0, 1), BoolParameter("randomPhase", true), BoolParameter("useNoise", false) });
	setParams("AlphaFade", { FloatParameter("startAlpha", 1, 0, 1), FloatParameter("endAlpha", 0, 0, 1), BoolParameter("easeIn", false), BoolParameter("easeOut", false) });
	setParams("RotationBySpeed", { FloatParameter("rotationPerSpeed", 90, -100000, 100000), FloatParameter("minSpeed", 0, 0, 100000), FloatParameter("maxSpeed", 0, 0, 100000) });
	setParams("SineWave", { FloatParameter("amplitude", 1, -100000, 100000), FloatParameter("frequency", 2, 0, 100000), Vector3Parameter("axis", {1,0,0}), BoolParameter("randomPhase", true) });
	setParams("Spiral", { FloatParameter("radius", 1, 0, 100000), FloatParameter("speed", 180, -100000, 100000), FloatParameter("lift", 1, -100000, 100000), BoolParameter("randomPhase", true), BoolParameter("expandRadius", false), FloatParameter("expansionRate", 0.5f, -100000, 100000) });
	setParams("Twist", { FloatParameter("twistSpeed", 90, -100000, 100000), FloatParameter("twistStrength", 1, -100000, 100000), IntParameter("heightAxis", 1, 0, 2) });

	auto setFactory = [&](const char* id, std::function<std::unique_ptr<IModule>()> factory)
	{
		descriptors_[lookup_.at(id)].factory = std::move(factory);
	};
#define KCE_MODULE_FACTORY(Id, Type) setFactory(Id, [] { return std::make_unique<Type>(); })
	KCE_MODULE_FACTORY("SpawnRate", SpawnRateModule); KCE_MODULE_FACTORY("SpawnBurst", SpawnBurstModule);
	KCE_MODULE_FACTORY("SpawnShape", SpawnShapeModule); KCE_MODULE_FACTORY("InitialPosition", InitialPositionModule);
	KCE_MODULE_FACTORY("InitialVelocity", InitialVelocityModule); KCE_MODULE_FACTORY("InitialLifetime", InitialLifetimeModule);
	KCE_MODULE_FACTORY("InitialColor", InitialColorModule); KCE_MODULE_FACTORY("InitialScale", InitialScaleModule);
	KCE_MODULE_FACTORY("InitialRotation", InitialRotationModule); KCE_MODULE_FACTORY("AssignRibbonId", AssignRibbonIdModule);
	KCE_MODULE_FACTORY("SubEmitter", SubEmitterModule); KCE_MODULE_FACTORY("Gravity", GravityModule);
	KCE_MODULE_FACTORY("Drag", DragModule); KCE_MODULE_FACTORY("ColorFade", ColorFadeModule);
	KCE_MODULE_FACTORY("ScaleOverLifetime", ScaleOverLifetimeModule); KCE_MODULE_FACTORY("RotationOverLifetime", RotationOverLifetimeModule);
	KCE_MODULE_FACTORY("Noise", NoiseModule); KCE_MODULE_FACTORY("VelocityOverLifetime", VelocityOverLifetimeModule);
	KCE_MODULE_FACTORY("StretchByVelocity", StretchByVelocityModule); KCE_MODULE_FACTORY("Flicker", FlickerModule);
	KCE_MODULE_FACTORY("AlphaFade", AlphaFadeModule); KCE_MODULE_FACTORY("FaceVelocity", FaceVelocityModule);
	KCE_MODULE_FACTORY("Acceleration", AccelerationModule); KCE_MODULE_FACTORY("CurlNoise", CurlNoiseModule);
	KCE_MODULE_FACTORY("SizeBySpeed", SizeBySpeedModule); KCE_MODULE_FACTORY("ColorBySpeed", ColorBySpeedModule);
	KCE_MODULE_FACTORY("Collision", CollisionModule); KCE_MODULE_FACTORY("KillZone", KillZoneModule);
	KCE_MODULE_FACTORY("SprintToTarget", SprintToTargetModule); KCE_MODULE_FACTORY("Attractor", AttractorModule);
	KCE_MODULE_FACTORY("Vortex", VortexModule); KCE_MODULE_FACTORY("Orbit", OrbitModule);
	KCE_MODULE_FACTORY("VelocityLimit", VelocityLimitModule); KCE_MODULE_FACTORY("RibbonInterpolation", RibbonInterpolationModule);
	KCE_MODULE_FACTORY("MultiSourceRibbon", MultiSourceRibbonModule); KCE_MODULE_FACTORY("TextureSheet", TextureSheetModule);
	KCE_MODULE_FACTORY("RadialVelocity", RadialVelocityModule); KCE_MODULE_FACTORY("Wind", WindModule);
	KCE_MODULE_FACTORY("RotationBySpeed", RotationBySpeedModule); KCE_MODULE_FACTORY("SineWave", SineWaveModule);
	KCE_MODULE_FACTORY("Spiral", SpiralModule); KCE_MODULE_FACTORY("Twist", TwistModule);
	KCE_MODULE_FACTORY("Jitter", JitterModule); KCE_MODULE_FACTORY("ForceOverLifetime", ForceOverLifetimeModule);
#undef KCE_MODULE_FACTORY
	// Dynamic bindings are exposed only where the common runtime overlay is
	// implemented for both CPU execution and Pure GPU constant/program packing.
	// This prevents an asset from accepting a binding that would be silently
	// ignored by one simulation path.
	for (auto& descriptor : descriptors_)
	{
		if (!descriptor.pureGpuSupported)
			for (auto& parameter : descriptor.parameters) parameter.dynamicInput = false;
	}
	for (const auto& descriptor : descriptors_)
	{
		if (!descriptor.factory) throw std::logic_error("Particle module has no factory: " + descriptor.id);
		if (descriptor.parameters.empty()) throw std::logic_error("Particle module has no parameter schema: " + descriptor.id);
		std::unordered_set<std::string> parameterIds;
		for (const auto& parameter : descriptor.parameters)
		{
			if (parameter.id.empty() || !parameterIds.insert(parameter.id).second)
				throw std::logic_error("Invalid or duplicate particle parameter ID: " + descriptor.id + "." + parameter.id);
			if (!ParameterValueMatchesType(parameter))
				throw std::logic_error("Particle parameter default type mismatch: " + descriptor.id + "." + parameter.id);
		}
		auto instance = descriptor.factory();
		if (!instance || descriptor.id != instance->GetName() || descriptor.phase != instance->GetPhase())
			throw std::logic_error("Particle module descriptor/factory mismatch: " + descriptor.id);
	}
}

const ModuleDescriptor* ModuleDescriptorRegistry::Find(const std::string& id) const
{
	auto it = lookup_.find(id);
	return it == lookup_.end() ? nullptr : &descriptors_[it->second];
}

std::string ModuleDescriptorRegistry::GenerateCapabilityMarkdown() const
{
	std::ostringstream output;
	output << "# Particle Module Capability Matrix (Generated)\n\n";
	output << "This file is generated from `ModuleDescriptorRegistry`; do not maintain capability flags by hand.\n\n";
	output << "- Module count: " << descriptors_.size() << "\n\n";
	output << "| Module | Phase | CPU | Pure GPU | Kernel | Parameters |\n";
	output << "|---|---|---:|---:|---|---:|\n";
	for (const auto& descriptor : descriptors_)
	{
		output << "| " << descriptor.id << " | " << (descriptor.phase == ModulePhase::Spawn ? "Spawn" : "Update")
			<< " | " << (descriptor.cpuSupported ? "Yes" : "No")
			<< " | " << (descriptor.pureGpuSupported ? "Yes" : "No")
			<< " | " << (descriptor.kernelId.empty() ? "-" : descriptor.kernelId)
			<< " | " << descriptor.parameters.size() << " |\n";
	}
	return output.str();
}

CompiledEmitter ModuleDescriptorRegistry::Compile(const std::vector<std::unique_ptr<IModule>>& modules,
	const std::vector<DynamicParameterBinding>& bindings) const
{
	CompiledEmitter output;
	uint64_t hash = 1469598103934665603ull;
	for (const auto& module : modules)
	{
		if (!module) { output.errors.emplace_back("Null module instance"); continue; }
		const ModuleDescriptor* descriptor = Find(module->GetName());
		if (!descriptor)
		{
			output.errors.emplace_back(std::string("Unknown module: ") + module->GetName());
			continue;
		}
		if (descriptor->phase != module->GetPhase())
		{
			output.errors.emplace_back(std::string("Stage mismatch: ") + module->GetName());
			continue;
		}
		CompiledModulePass pass;
		pass.descriptor = descriptor;
		pass.instance = module.get();
		pass.parameterOffset = static_cast<uint32_t>(output.packedParameters.size());
		const uint32_t header[3] = { descriptor->version, static_cast<uint32_t>(descriptor->phase), static_cast<uint32_t>(module->GetPriority()) };
		const auto* begin = reinterpret_cast<const uint8_t*>(header);
		output.packedParameters.insert(output.packedParameters.end(), begin, begin + sizeof(header));
		pass.parameterSize = sizeof(header);
		output.passes.push_back(pass);
		hash = HashString(hash, descriptor->id);
		hash = HashBytes(hash, header, sizeof(header));
	}
	std::stable_sort(output.passes.begin(), output.passes.end(), [](const CompiledModulePass& a, const CompiledModulePass& b)
	{
		if (a.descriptor->phase != b.descriptor->phase) return a.descriptor->phase < b.descriptor->phase;
		return a.instance->GetPriority() < b.instance->GetPriority();
	});
	for (const auto& binding : bindings)
	{
		const ModuleDescriptor* descriptor = Find(binding.moduleId);
		if (!descriptor) { output.errors.emplace_back("Dynamic binding references unknown module: " + binding.moduleId); continue; }
		const auto schema = std::find_if(descriptor->parameters.begin(), descriptor->parameters.end(),
			[&](const ModuleParameterSchema& parameter) { return parameter.id == binding.parameterId; });
		if (schema == descriptor->parameters.end() || !schema->dynamicInput || schema->type != binding.type)
		{
			output.errors.emplace_back("Dynamic binding schema mismatch: " + binding.moduleId + "." + binding.parameterId);
			continue;
		}
		hash = HashString(hash, binding.moduleId);
		hash = HashString(hash, binding.parameterId);
		hash = HashBytes(hash, &binding.type, sizeof(binding.type));
		hash = HashBytes(hash, &binding.mode, sizeof(binding.mode));
		hash = HashString(hash, binding.emitterParameter);
		for (const auto& key : binding.keys) hash = HashBytes(hash, &key, sizeof(key));
	}
	output.layoutHash = hash;
	output.valid = output.errors.empty();
	return output;
}
} // namespace KCE
