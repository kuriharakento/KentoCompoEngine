#include "ParticleEffectSerializer.h"
#include "effects/particle/ParticleEffect.h"
#include "effects/particle/ParticleEmitter.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/renderer/TrailRenderer.h"
#include "effects/particle/renderer/MeshRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/spawn/SpawnShapeModules.h"
#include "effects/particle/module/spawn/SubEmitterModule.h"
#include "effects/particle/module/update/UpdateModules.h"
#include "effects/particle/module/update/BehaviorModules.h"
#include "effects/particle/module/update/AdvancedModules.h"
#include "effects/particle/module/update/ForceFieldModules.h"
#include "effects/particle/module/update/RibbonModules.h"
#include "effects/particle/module/update/TextureSheetModule.h"
#include "effects/particle/module/update/MotionEffectModules.h"
#include "effects/particle/module/update/NaturalBehaviorModules.h"
#include "effects/particle/module/ModuleRuntime.h"
#include "time/Timer.h"
#include <fstream>
#include <cmath>
#include <type_traits>

// nlohmann/json を有効化
#define USE_NLOHMANN_JSON
#include "externals/nlohmann/json.hpp"

namespace KCE
{
using json = nlohmann::json;

// 前方宣言（プライベートヘルパー関数）
static std::unique_ptr<ParticleEmitter> LoadEmitter(const json& data);
static void SaveEmitter(const ParticleEmitter& emitter, json& data);
static constexpr uint32_t kParticleAssetFormatVersion = 2;
static constexpr size_t kMaxSerializedEmitters = 1024;
static constexpr size_t kMaxSerializedModules = 256;
static constexpr uint32_t kMaxSerializedParticles = 1000000;

static bool IsFiniteNumber(const json& value)
{
	if (!value.is_number()) return true;
	return std::isfinite(value.get<double>());
}

static bool ValidateFiniteRecursive(const json& value)
{
	if (!IsFiniteNumber(value)) return false;
	if (value.is_array()) for (const auto& child : value) if (!ValidateFiniteRecursive(child)) return false;
	if (value.is_object()) for (auto it = value.begin(); it != value.end(); ++it) if (!ValidateFiniteRecursive(it.value())) return false;
	return true;
}

static bool ValidateRendererData(const json& data)
{
	if (!data.is_object()) return false;
	const std::string type = data.value("type", "Sprite");
	if (type != "Sprite" && type != "Ribbon" && type != "Mesh") return false;
	const int blendMode = data.value("blendMode", 1);
	if (blendMode < static_cast<int>(BlendMode::Alpha) || blendMode > static_cast<int>(BlendMode::ColorDodge)) return false;
	const std::string texture = data.value("texture", "./Resources/uvChecker.png");
	if (texture.empty() || texture.size() > 1024) return false;

	if (data.contains("tintColor") && !data["tintColor"].is_object()) return false;
	if (type == "Ribbon")
	{
		const int textureMode = data.value("textureMode", 0);
		if (textureMode < static_cast<int>(RibbonTextureMode::Stretch) || textureMode > static_cast<int>(RibbonTextureMode::Tile)) return false;
		if (data.value("trailWidth", 0.5f) < 0.0f || data.value("trailLifetime", 1.0f) <= 0.0f ||
			data.value("recordInterval", 0.016f) <= 0.0f || data.value("minSegmentDistance", 0.1f) < 0.0f ||
			data.value("tileScale", 1.0f) <= 0.0f) return false;
	}
	if (type == "Mesh")
	{
		const int primitiveType = data.value("primitiveType", 0);
		if (primitiveType < static_cast<int>(PrimitiveType::Plane) || primitiveType > static_cast<int>(PrimitiveType::Custom) ||
			data.value("scale", 1.0f) <= 0.0f) return false;
		if (data.contains("primitiveOptions"))
		{
			const auto& options = data["primitiveOptions"];
			if (!options.is_object()) return false;
			const uint32_t segments = options.value("segments", 16u);
			const uint32_t rings = options.value("rings", 8u);
			const uint32_t points = options.value("points", 5u);
			if (segments < 3 || segments > 4096 || rings < 1 || rings > 4096 || points < 2 || points > 4096 ||
				options.value("innerRadius", 0.5f) < 0.0f || options.value("outerRadius", 1.0f) <= 0.0f ||
				options.value("tubeRadius", 0.3f) < 0.0f || options.value("turns", 2.0f) <= 0.0f) return false;
			if (options.contains("cubeSize") && !options["cubeSize"].is_object()) return false;
			if (options.contains("cubeFaceVisible"))
			{
				const auto& faces = options["cubeFaceVisible"];
				if (!faces.is_array() || faces.size() != 6) return false;
				for (const auto& face : faces) if (!face.is_boolean()) return false;
			}
		}
	}
	return true;
}

static bool ValidateModuleParameter(const json& module, const ModuleParameterSchema& schema)
{
	if (!module.contains(schema.id)) return true;
	const auto& value = module[schema.id];
	auto inRange = [&](double number)
	{
		return !schema.hasRange || (number >= static_cast<double>(schema.minimum) && number <= static_cast<double>(schema.maximum));
	};
	switch (schema.type)
	{
	case ModuleParameterType::Float:
		return value.is_number() && inRange(value.get<double>());
	case ModuleParameterType::UInt:
		return value.is_number_unsigned() && inRange(value.get<double>());
	case ModuleParameterType::Int:
	case ModuleParameterType::Enum:
		return value.is_number_integer() && inRange(value.get<double>());
	case ModuleParameterType::Bool:
		return value.is_boolean();
	case ModuleParameterType::Vector3:
		// InitialRotation v1 also stores a scalar compatibility field beside
		// minAngle_v3/maxAngle_v3. Preserve that asset format until migration.
		return value.is_number() || (value.is_object() && value.contains("x") && value.contains("y") && value.contains("z") &&
			value["x"].is_number() && value["y"].is_number() && value["z"].is_number());
	case ModuleParameterType::Vector4:
		return value.is_object() &&
			((value.contains("x") && value.contains("y") && value.contains("z") && value.contains("w") &&
				value["x"].is_number() && value["y"].is_number() && value["z"].is_number() && value["w"].is_number()) ||
			 (value.contains("r") && value.contains("g") && value.contains("b") && value.contains("a") &&
				value["r"].is_number() && value["g"].is_number() && value["b"].is_number() && value["a"].is_number()));
	case ModuleParameterType::String:
		return value.is_string() && value.get_ref<const std::string&>().size() <= 1024;
	case ModuleParameterType::Curve:
	case ModuleParameterType::Gradient:
		return value.is_object() && value.size() <= 4096;
	case ModuleParameterType::StructArray:
		return value.is_array() && value.size() <= 1024;
	}
	return false;
}

static bool ValidateModuleData(const json& module, const ModuleDescriptor& descriptor)
{
	if (!module.is_object() || module.size() > 256) return false;
	for (const auto& parameter : descriptor.parameters)
	{
		if (!ValidateModuleParameter(module, parameter)) return false;
	}
	return true;
}

static json SaveBindingValue(const ModuleParameterValue& value)
{
	return std::visit([](const auto& typed) -> json
	{
		using T = std::decay_t<decltype(typed)>;
		if constexpr (std::is_same_v<T, Vector3>) return { {"x", typed.x}, {"y", typed.y}, {"z", typed.z} };
		else if constexpr (std::is_same_v<T, Vector4>) return { {"x", typed.x}, {"y", typed.y}, {"z", typed.z}, {"w", typed.w} };
		else return typed;
	}, value);
}

static bool LoadBindingValue(const json& value, ModuleParameterType type, ModuleParameterValue& output)
{
	if (type == ModuleParameterType::Float && value.is_number()) { output = value.get<float>(); return true; }
	if (type == ModuleParameterType::Vector3 && value.is_object())
	{
		output = Vector3{ value.value("x", 0.0f), value.value("y", 0.0f), value.value("z", 0.0f) }; return true;
	}
	if (type == ModuleParameterType::Vector4 && value.is_object())
	{
		output = Vector4{ value.value("x", 0.0f), value.value("y", 0.0f), value.value("z", 0.0f), value.value("w", 0.0f) }; return true;
	}
	return false;
}

std::unique_ptr<ParticleEffect> ParticleEffectSerializer::Load(const std::string& path)
{
#ifdef USE_NLOHMANN_JSON
	std::ifstream file(path);
	if (!file.is_open())
	{
		return nullptr;
	}

	try
	{
		json data = json::parse(file);
		if (!data.is_object() || !ValidateFiniteRecursive(data)) return nullptr;
		const uint32_t formatVersion = data.value("formatVersion", 1u);
		if (formatVersion == 0 || formatVersion > kParticleAssetFormatVersion) return nullptr;
		if (data.contains("emitters") && (!data["emitters"].is_array() || data["emitters"].size() > kMaxSerializedEmitters)) return nullptr;

		const int deltaTimeType = data.value("deltaTimeType", 0);
		if (deltaTimeType < 0 || deltaTimeType > 1) return nullptr;
		auto effect = std::make_unique<ParticleEffect>();
		effect->Initialize(data.value("name", "UnnamedEffect"));

		// 位置
		if (data.contains("position"))
		{
			Vector3 pos;
			pos.x = data["position"].value("x", 0.0f);
			pos.y = data["position"].value("y", 0.0f);
			pos.z = data["position"].value("z", 0.0f);
			effect->SetPosition(pos);
		}

		// タイムスケール設定
		effect->SetDeltaTimeType(static_cast<DeltaTimeType>(deltaTimeType));

		// エミッター
		if (data.contains("emitters") && data["emitters"].is_array())
		{
			for (const auto& emitterData : data["emitters"])
			{
				auto emitter = LoadEmitter(emitterData);
				if (!emitter) return nullptr;
				effect->AddEmitter(std::move(emitter));
			}
		}
		for (size_t i = 0; i < effect->GetEmitterCount(); ++i)
		{
			const auto* emitter = effect->GetEmitter(i);
			const int followIndex = emitter->GetFollowEmitterIndex();
			const int eventSourceIndex = emitter->GetGPUEventSourceEmitterIndex();
			if (followIndex < -1 || followIndex >= static_cast<int>(effect->GetEmitterCount())) return nullptr;
			// GPU event producers must precede consumers so the same command list can
			// transition the producer stream to SRV before the consumer dispatch.
			if (eventSourceIndex < -1 || eventSourceIndex >= static_cast<int>(i)) return nullptr;
		}

		return effect;
	}
	catch (const std::exception&)
	{
		return nullptr;
	}
#else
	// JSONライブラリがない場合は空のエフェクトを返す
	(void)path;
	auto effect = std::make_unique<ParticleEffect>();
	effect->Initialize("DefaultEffect");
	return effect;
#endif
}

bool ParticleEffectSerializer::Save(const ParticleEffect& effect, const std::string& path)
{
#ifdef USE_NLOHMANN_JSON
	json data;
	data["formatVersion"] = kParticleAssetFormatVersion;
	data["moduleSchemaVersion"] = 1;

	data["name"] = effect.GetName();

	// 位置
	data["position"] = {
		{"x", effect.GetPosition().x},
		{"y", effect.GetPosition().y},
		{"z", effect.GetPosition().z}
	};

	// タイムスケール設定
	data["deltaTimeType"] = static_cast<int>(effect.GetDeltaTimeType());

	// エミッター
	data["emitters"] = json::array();
	for (size_t i = 0; i < effect.GetEmitterCount(); ++i)
	{
		json emitterData;
		SaveEmitter(*effect.GetEmitter(i), emitterData);
		data["emitters"].push_back(emitterData);
	}

	std::ofstream file(path);
	if (!file.is_open())
	{
		return false;
	}

	file << data.dump(4);
	return true;
#else
	(void)effect;
	(void)path;
	return false;
#endif
}

#ifdef USE_NLOHMANN_JSON
static std::unique_ptr<ParticleEmitter> LoadEmitter(const json& data)
{
	if (!data.is_object()) return nullptr;
	if (data.contains("renderer") && !ValidateRendererData(data["renderer"])) return nullptr;
	const uint32_t maxParticles = data.value("maxParticles", 1000u);
	const int simulationMode = data.value("simulationMode", 0);
	const int loopBehavior = data.value("loopBehavior", 1);
	const int inactiveResponse = data.value("inactiveResponse", 0);
	const uint32_t gpuEventTrigger = data.value("gpuEventTrigger", 1u);
	const float gpuEventProbability = data.value("gpuEventProbability", 1.0f);
	const float duration = data.value("duration", 0.0f);
	const float startDelay = data.value("startDelay", 0.0f);
	const float minMoveDistance = data.value("minMoveDistance", 0.05f);
	const int loopCount = data.value("loopCount", 1);
	if (maxParticles == 0 || maxParticles > kMaxSerializedParticles || simulationMode < 0 || simulationMode > 1 ||
		loopBehavior < 0 || loopBehavior > 2 || inactiveResponse < 0 || inactiveResponse > 1 ||
		gpuEventTrigger > 1 || gpuEventProbability < 0.0f || gpuEventProbability > 1.0f ||
		duration < 0.0f || startDelay < 0.0f || minMoveDistance < 0.0f || loopCount < 1) return nullptr;
	if (data.contains("modules") && (!data["modules"].is_array() || data["modules"].size() > kMaxSerializedModules)) return nullptr;
	auto emitter = std::make_unique<ParticleEmitter>();
	emitter->Initialize(data.value("name", "UnnamedEmitter"));
	if (data.contains("parameters"))
	{
		if (!data["parameters"].is_array() || data["parameters"].size() > 256) return nullptr;
		for (const auto& parameter : data["parameters"])
		{
			if (!parameter.is_object()) return nullptr;
			const std::string name = parameter.value("name", "");
			const std::string type = parameter.value("type", "");
			if (name.empty() || name.size() > 128 || !parameter.contains("value")) return nullptr;
			const auto& value = parameter["value"];
			if (type == "float" && value.is_number()) emitter->GetParameterStore().Set(name, value.get<float>());
			else if (type == "uint" && value.is_number_unsigned()) emitter->GetParameterStore().Set(name, value.get<uint32_t>());
			else if (type == "int" && value.is_number_integer()) emitter->GetParameterStore().Set(name, value.get<int32_t>());
			else if (type == "bool" && value.is_boolean()) emitter->GetParameterStore().Set(name, value.get<bool>());
			else if (type == "string" && value.is_string() && value.get_ref<const std::string&>().size() <= 1024) emitter->GetParameterStore().Set(name, value.get<std::string>());
			else if (type == "vector3" && value.is_object()) emitter->GetParameterStore().Set(name, Vector3{ value.value("x", 0.0f), value.value("y", 0.0f), value.value("z", 0.0f) });
			else if (type == "vector4" && value.is_object()) emitter->GetParameterStore().Set(name, Vector4{ value.value("x", 0.0f), value.value("y", 0.0f), value.value("z", 0.0f), value.value("w", 0.0f) });
			else return nullptr;
		}
	}

	// 基本設定
	emitter->SetMaxParticles(maxParticles);
	emitter->SetSimulationMode(static_cast<SimulationMode>(simulationMode));

	if (data.contains("position"))
	{
		Vector3 pos;
		pos.x = data["position"].value("x", 0.0f);
		pos.y = data["position"].value("y", 0.0f);
		pos.z = data["position"].value("z", 0.0f);
		emitter->SetPosition(pos);
	}

	// Follow Offset
	if (data.contains("followOffset"))
	{
		Vector3 offset;
		offset.x = data["followOffset"].value("x", 0.0f);
		offset.y = data["followOffset"].value("y", 0.0f);
		offset.z = data["followOffset"].value("z", 0.0f);
		emitter->SetFollowOffset(offset);
	}

	// Follow Emitter Index (同じエフェクト内の別エミッターを追従)
	emitter->SetFollowEmitterIndex(data.value("followEmitterIndex", -1));
	emitter->SetGPUEventSourceEmitterIndex(data.value("gpuEventSourceEmitterIndex", -1));
	emitter->SetGPUEventTrigger(gpuEventTrigger);
	emitter->SetGPUEventProbability(gpuEventProbability);
	emitter->SetGPUEventVelocityInheritance(data.value("gpuEventInheritVelocity", false), data.value("gpuEventVelocityScale", 1.0f));
	emitter->SetGPUEventInheritColor(data.value("gpuEventInheritColor", false));

	// 移動時のみ生成
	emitter->SetSpawnOnlyWhenMoving(data.value("spawnOnlyWhenMoving", false));
	emitter->SetMinMoveDistance(minMoveDistance);

	// ライフサイクル設定
	emitter->SetDuration(duration);
	emitter->SetStartDelay(startDelay);
	emitter->SetLoopBehavior(static_cast<LoopBehavior>(data.value("loopBehavior", 1))); // default: Infinite
	emitter->SetLoopCount(loopCount);
	emitter->SetInactiveResponse(static_cast<InactiveResponse>(data.value("inactiveResponse", 0))); // default: Complete

	// モジュール
	if (data.contains("modules") && data["modules"].is_array())
	{
		for (const auto& moduleData : data["modules"])
		{
			if (!moduleData.is_object()) return nullptr;
			std::string type = moduleData.value("type", "");
			const ModuleDescriptor* descriptor = ModuleDescriptorRegistry::GetInstance().Find(type);
			if (!descriptor) return nullptr;
			const uint32_t moduleVersion = moduleData.value("version", 1u);
			if (moduleVersion == 0 || moduleVersion > descriptor->version) return nullptr;
			if (!ValidateModuleData(moduleData, *descriptor)) return nullptr;

			if (type == "SpawnRate")
			{
				auto m = std::make_unique<SpawnRateModule>();
				m->SetRate(moduleData.value("rate", 10.0f));
				emitter->AddModule(std::move(m));
			}
			else if (type == "SpawnBurst")
			{
				auto m = std::make_unique<SpawnBurstModule>();
				m->SetCount(moduleData.value("count", 10u));
				m->SetDelay(moduleData.value("delay", 0.0f));
				m->SetInterval(moduleData.value("interval", 0.0f));
				m->SetLoops(moduleData.value("loops", 1));
				emitter->AddModule(std::move(m));
			}
			else if (type == "SpawnShape")
			{
				auto m = std::make_unique<SpawnShapeModule>();
				m->SetShapeType(static_cast<SpawnShapeType>(moduleData.value("shapeType", 0)));
				m->SetInnerRadius(moduleData.value("innerRadius", 0.0f));
				m->SetOuterRadius(moduleData.value("outerRadius", 1.0f));
				if (moduleData.contains("boxSize"))
				{
					Vector3 bs;
					bs.x = moduleData["boxSize"].value("x", 1.0f);
					bs.y = moduleData["boxSize"].value("y", 1.0f);
					bs.z = moduleData["boxSize"].value("z", 1.0f);
					m->SetBoxSize(bs);
				}
				m->SetConeHeight(moduleData.value("coneHeight", 2.0f));
				if (moduleData.contains("lineStart") && moduleData.contains("lineEnd"))
				{
					Vector3 ls, le;
					ls.x = moduleData["lineStart"].value("x", 0.0f);
					ls.y = moduleData["lineStart"].value("y", 0.0f);
					ls.z = moduleData["lineStart"].value("z", 0.0f);
					le.x = moduleData["lineEnd"].value("x", 0.0f);
					le.y = moduleData["lineEnd"].value("y", 1.0f);
					le.z = moduleData["lineEnd"].value("z", 0.0f);
					m->SetLine(ls, le);
				}
				m->SetEmitFromSurface(moduleData.value("emitFromSurface", false));
				m->SetInitialSpeed(moduleData.value("initialSpeed", 0.0f));
				m->SetSpawnLocation(static_cast<SpawnLocation>(moduleData.value("spawnLocation", 0)));
				m->SetArcAngle(moduleData.value("arcAngle", 360.0f));
				emitter->AddModule(std::move(m));
			}
			else if (type == "InitialPosition")
			{
				Vector3 minOffset{}, maxOffset{};
				if (moduleData.contains("min"))
				{
					minOffset = { moduleData["min"].value("x", 0.0f), moduleData["min"].value("y", 0.0f), moduleData["min"].value("z", 0.0f) };
				}
				if (moduleData.contains("max"))
				{
					maxOffset = { moduleData["max"].value("x", 0.0f), moduleData["max"].value("y", 0.0f), moduleData["max"].value("z", 0.0f) };
				}
				emitter->AddModule(std::make_unique<InitialPositionModule>(minOffset, maxOffset));
			}
			else if (type == "InitialLifetime")
			{
				float minL = moduleData.value("min", 1.0f);
				float maxL = moduleData.value("max", 2.0f);
				emitter->AddModule(std::make_unique<InitialLifetimeModule>(minL, maxL));
			}
			else if (type == "InitialVelocity")
			{
				Vector3 minV, maxV;
				if (moduleData.contains("min"))
				{
					minV.x = moduleData["min"].value("x", 0.0f);
					minV.y = moduleData["min"].value("y", 0.0f);
					minV.z = moduleData["min"].value("z", 0.0f);
				}
				if (moduleData.contains("max"))
				{
					maxV.x = moduleData["max"].value("x", 0.0f);
					maxV.y = moduleData["max"].value("y", 0.0f);
					maxV.z = moduleData["max"].value("z", 0.0f);
				}
				emitter->AddModule(std::make_unique<InitialVelocityModule>(minV, maxV));
			}
			else if (type == "InitialScale")
			{
				Vector3 minS = { 1.0f, 1.0f, 1.0f }, maxS = { 1.0f, 1.0f, 1.0f };
				if (moduleData.contains("min"))
				{
					minS.x = moduleData["min"].value("x", 1.0f);
					minS.y = moduleData["min"].value("y", 1.0f);
					minS.z = moduleData["min"].value("z", 1.0f);
				}
				if (moduleData.contains("max"))
				{
					maxS.x = moduleData["max"].value("x", 1.0f);
					maxS.y = moduleData["max"].value("y", 1.0f);
					maxS.z = moduleData["max"].value("z", 1.0f);
				}
				emitter->AddModule(std::make_unique<InitialScaleModule>(minS, maxS));
			}
			else if (type == "InitialColor")
			{
				auto m = std::make_unique<InitialColorModule>();
				if (moduleData.contains("min"))
				{
					Vector4 minC;
					minC.x = moduleData["min"].value("r", 1.0f);
					minC.y = moduleData["min"].value("g", 1.0f);
					minC.z = moduleData["min"].value("b", 1.0f);
					minC.w = moduleData["min"].value("a", 1.0f);
					m->SetMinColor(minC);
				}
				if (moduleData.contains("max"))
				{
					Vector4 maxC;
					maxC.x = moduleData["max"].value("r", 1.0f);
					maxC.y = moduleData["max"].value("g", 1.0f);
					maxC.z = moduleData["max"].value("b", 1.0f);
					maxC.w = moduleData["max"].value("a", 1.0f);
					m->SetMaxColor(maxC);
				}
				emitter->AddModule(std::move(m));
			}
			else if (type == "AssignRibbonId")
			{
				auto m = std::make_unique<AssignRibbonIdModule>();
				m->SetGroupCount(moduleData.value("groupCount", 1u));
				emitter->AddModule(std::move(m));
			}
			else if (type == "SubEmitter")
			{
				if (moduleData.contains("configs") && (!moduleData["configs"].is_array() || moduleData["configs"].size() > 64)) return nullptr;
				auto m = std::make_unique<SubEmitterModule>();
				if (moduleData.contains("configs")) for (const auto& configData : moduleData["configs"])
				{
					if (!configData.is_object()) return nullptr;
					SubEmitterConfig config;
					config.effectPath = configData.value("effectPath", "");
					const int trigger = configData.value("trigger", static_cast<int>(SubEmitterTrigger::OnDeath));
					config.probability = configData.value("probability", 1.0f);
					config.continuousRate = configData.value("continuousRate", 10.0f);
					if (config.effectPath.size() > 1024 || trigger < 0 || trigger > 3 ||
						config.probability < 0.0f || config.probability > 1.0f || config.continuousRate <= 0.0f) return nullptr;
					config.trigger = static_cast<SubEmitterTrigger>(trigger);
					config.inheritPosition = configData.value("inheritPosition", true);
					config.inheritVelocity = configData.value("inheritVelocity", false);
					config.inheritVelocityScale = configData.value("inheritVelocityScale", 0.5f);
					config.inheritColor = configData.value("inheritColor", false);
					config.inheritScale = configData.value("inheritScale", false);
					m->AddConfig(config);
				}
				emitter->AddModule(std::move(m));
			}
			else if (type == "Gravity")
			{
				auto m = std::make_unique<GravityModule>();
				Vector3 minG = { 0, -9.8f, 0 }, maxG = { 0, -9.8f, 0 };
				if (moduleData.contains("min"))
				{
					minG.x = moduleData["min"].value("x", 0.0f);
					minG.y = moduleData["min"].value("y", -9.8f);
					minG.z = moduleData["min"].value("z", 0.0f);
				}
				else if (moduleData.contains("gravity"))
				{ // 互換性
					minG.x = moduleData["gravity"].value("x", 0.0f);
					minG.y = moduleData["gravity"].value("y", -9.8f);
					minG.z = moduleData["gravity"].value("z", 0.0f);
				}
				
				if (moduleData.contains("max"))
				{
					maxG.x = moduleData["max"].value("x", minG.x);
					maxG.y = moduleData["max"].value("y", minG.y);
					maxG.z = moduleData["max"].value("z", minG.z);
				}
				else
				{
					maxG = minG;
				}
				m->SetGravityRange(minG, maxG);
				emitter->AddModule(std::move(m));
			}
			else if (type == "Drag")
			{
				auto m = std::make_unique<DragModule>();
				float minD = moduleData.value("min", moduleData.value("drag", 0.1f));
				float maxD = moduleData.value("max", minD);
				m->SetDragRange(minD, maxD);
				emitter->AddModule(std::move(m));
			}
			else if (type == "ColorFade")
			{
				auto m = std::make_unique<ColorFadeModule>();
				m->SetUseInitialColor(moduleData.value("useInitialColor", false));
				if (moduleData.contains("start"))
				{
					Vector4 sc;
					sc.x = moduleData["start"].value("r", 1.0f);
					sc.y = moduleData["start"].value("g", 1.0f);
					sc.z = moduleData["start"].value("b", 1.0f);
					sc.w = moduleData["start"].value("a", 1.0f);
					m->SetStartColor(sc);
				}
				if (moduleData.contains("end"))
				{
					Vector4 ec;
					ec.x = moduleData["end"].value("r", 0.0f);
					ec.y = moduleData["end"].value("g", 0.0f);
					ec.z = moduleData["end"].value("b", 0.0f);
					ec.w = moduleData["end"].value("a", 0.0f);
					m->SetEndColor(ec);
				}
				m->SetEasingType(static_cast<EasingType>(moduleData.value("easingType", 0)));
				if (moduleData.contains("gradient"))
				{
					if (!moduleData["gradient"].is_array() || moduleData["gradient"].size() > 64) return nullptr;
					ColorGradient gradient;
					for (const auto& key : moduleData["gradient"])
					{
						if (!key.is_object()) return nullptr;
						gradient.AddKey(key.value("time", 0.0f), { key.value("r", 1.0f), key.value("g", 1.0f), key.value("b", 1.0f), key.value("a", 1.0f) });
					}
					m->SetGradient(gradient);
				}
				emitter->AddModule(std::move(m));
			}
			else if (type == "ScaleOverLifetime")
			{
				auto m = std::make_unique<ScaleOverLifetimeModule>();
				if (moduleData.contains("start"))
				{
					Vector3 ss;
					ss.x = moduleData["start"].value("x", 1.0f);
					ss.y = moduleData["start"].value("y", 1.0f);
					ss.z = moduleData["start"].value("z", 1.0f);
					m->SetStartScale(ss);
				}
				if (moduleData.contains("end"))
				{
					Vector3 es;
					es.x = moduleData["end"].value("x", 0.0f);
					es.y = moduleData["end"].value("y", 0.0f);
					es.z = moduleData["end"].value("z", 0.0f);
					m->SetEndScale(es);
				}
				m->SetEasingType(static_cast<EasingType>(moduleData.value("easingType", 0)));
				if (moduleData.contains("curve"))
				{
					if (!moduleData["curve"].is_array() || moduleData["curve"].size() > 64) return nullptr;
					AnimationCurve curve;
					for (const auto& key : moduleData["curve"])
					{
						if (!key.is_object()) return nullptr;
						curve.AddKey(key.value("time", 0.0f), key.value("value", 0.0f));
					}
					m->SetCurve(curve);
				}
				emitter->AddModule(std::move(m));
			}
			// Phase 3: New modules
			else if (type == "Acceleration")
			{
				auto m = std::make_unique<AccelerationModule>();
				if (moduleData.contains("acceleration"))
				{
					Vector3 acc;
					acc.x = moduleData["acceleration"].value("x", 0.0f);
					acc.y = moduleData["acceleration"].value("y", 0.0f);
					acc.z = moduleData["acceleration"].value("z", 0.0f);
					m->SetAcceleration(acc);
				}
				emitter->AddModule(std::move(m));
			}
			else if (type == "CurlNoise")
			{
				auto m = std::make_unique<CurlNoiseModule>();
				m->SetStrength(moduleData.value("strength", 1.0f));
				m->SetFrequency(moduleData.value("frequency", 1.0f));
				m->SetOctaves(moduleData.value("octaves", 3));
				m->SetScrollSpeed(moduleData.value("scrollSpeed", 1.0f));
				emitter->AddModule(std::move(m));
			}
			else if (type == "SizeBySpeed")
			{
				auto m = std::make_unique<SizeBySpeedModule>();
				m->SetSpeedRange(moduleData.value("minSpeed", 0.0f), moduleData.value("maxSpeed", 10.0f));
				Vector3 minS = { 0.5f, 0.5f, 0.5f }, maxS = { 2.0f, 2.0f, 2.0f };
				if (moduleData.contains("minScale"))
				{
					minS.x = moduleData["minScale"].value("x", 0.5f);
					minS.y = moduleData["minScale"].value("y", 0.5f);
					minS.z = moduleData["minScale"].value("z", 0.5f);
				}
				if (moduleData.contains("maxScale"))
				{
					maxS.x = moduleData["maxScale"].value("x", 2.0f);
					maxS.y = moduleData["maxScale"].value("y", 2.0f);
					maxS.z = moduleData["maxScale"].value("z", 2.0f);
				}
				m->SetScaleRange(minS, maxS);
				emitter->AddModule(std::move(m));
			}
			else if (type == "ColorBySpeed")
			{
				auto m = std::make_unique<ColorBySpeedModule>();
				m->SetSpeedRange(moduleData.value("minSpeed", 0.0f), moduleData.value("maxSpeed", 10.0f));
				Vector4 minC = { 1, 1, 1, 1 }, maxC = { 1, 0, 0, 1 };
				if (moduleData.contains("minColor"))
				{
					minC.x = moduleData["minColor"].value("r", 1.0f);
					minC.y = moduleData["minColor"].value("g", 1.0f);
					minC.z = moduleData["minColor"].value("b", 1.0f);
					minC.w = moduleData["minColor"].value("a", 1.0f);
				}
				if (moduleData.contains("maxColor"))
				{
					maxC.x = moduleData["maxColor"].value("r", 1.0f);
					maxC.y = moduleData["maxColor"].value("g", 0.0f);
					maxC.z = moduleData["maxColor"].value("b", 0.0f);
					maxC.w = moduleData["maxColor"].value("a", 1.0f);
				}
				m->SetColorRange(minC, maxC);
				emitter->AddModule(std::move(m));
			}
			else if (type == "Collision")
			{
				auto m = std::make_unique<CollisionModule>();
				m->SetMode(static_cast<CollisionMode>(moduleData.value("mode", 1)));
				m->SetBounce(moduleData.value("bounce", 0.5f));
				m->SetFriction(moduleData.value("friction", 0.9f));
				m->SetPlaneHeight(moduleData.value("planeHeight", 0.0f));
				if (moduleData.contains("boxCenter"))
				{
					Vector3 c;
					c.x = moduleData["boxCenter"].value("x", 0.0f);
					c.y = moduleData["boxCenter"].value("y", 0.0f);
					c.z = moduleData["boxCenter"].value("z", 0.0f);
					m->SetBoxCenter(c);
				}
				if (moduleData.contains("boxSize"))
				{
					Vector3 s;
					s.x = moduleData["boxSize"].value("x", 10.0f);
					s.y = moduleData["boxSize"].value("y", 10.0f);
					s.z = moduleData["boxSize"].value("z", 10.0f);
					m->SetBoxSize(s);
				}
				m->SetKillOnCollision(moduleData.value("killOnCollision", false));
				emitter->AddModule(std::move(m));
			}
			else if (type == "KillZone")
			{
				auto m = std::make_unique<KillZoneModule>();
				m->SetZoneType(static_cast<KillZoneType>(moduleData.value("zoneType", 0)));
				if (moduleData.contains("center"))
				{
					Vector3 c;
					c.x = moduleData["center"].value("x", 0.0f);
					c.y = moduleData["center"].value("y", 0.0f);
					c.z = moduleData["center"].value("z", 0.0f);
					m->SetCenter(c);
				}
				if (moduleData.contains("boxSize"))
				{
					Vector3 s;
					s.x = moduleData["boxSize"].value("x", 5.0f);
					s.y = moduleData["boxSize"].value("y", 5.0f);
					s.z = moduleData["boxSize"].value("z", 5.0f);
					m->SetBoxSize(s);
				}
				m->SetRadius(moduleData.value("radius", 5.0f));
				m->SetKillInside(moduleData.value("killInside", true));
				emitter->AddModule(std::move(m));
			}
			else if (type == "SprintToTarget")
			{
				auto m = std::make_unique<SprintToTargetModule>();
				if (moduleData.contains("target"))
				{
					Vector3 t;
					t.x = moduleData["target"].value("x", 0.0f);
					t.y = moduleData["target"].value("y", 0.0f);
					t.z = moduleData["target"].value("z", 0.0f);
					m->SetTarget(t);
				}
				m->SetAcceleration(moduleData.value("acceleration", 5.0f));
				m->SetArriveRadius(moduleData.value("arriveRadius", 0.5f));
				m->SetKillOnArrive(moduleData.value("killOnArrive", false));
				m->SetMaxDistance(moduleData.value("maxDistance", 10.0f));
				m->SetSpeedBoost(moduleData.value("speedBoost", 1.0f));
				m->SetUseSpeedCurve(moduleData.value("useSpeedCurve", false));
				emitter->AddModule(std::move(m));
			}
			// AdvancedModules
			else if (type == "RotationOverLifetime")
			{
				auto m = std::make_unique<RotationOverLifetimeModule>();
				float startSpeed = moduleData.value("startSpeed", moduleData.value("rotationSpeed", 180.0f));
				float endSpeed = moduleData.value("endSpeed", startSpeed);
				m->SetRotationSpeedRange(startSpeed, endSpeed);
				m->SetEasingType(static_cast<EasingType>(moduleData.value("easingType", 0)));
				emitter->AddModule(std::move(m));
			}
			else if (type == "FaceVelocity")
			{
				auto m = std::make_unique<FaceVelocityModule>();
				m->SetUse2DAlignment(moduleData.value("use2DAlignment", false));
				emitter->AddModule(std::move(m));
			}
			else if (type == "Jitter")
			{
				auto m = std::make_unique<JitterModule>();
				if (moduleData.contains("amount"))
				{
					Vector3 a;
					a.x = moduleData["amount"].value("x", 0.1f);
					a.y = moduleData["amount"].value("y", 0.1f);
					a.z = moduleData["amount"].value("z", 0.1f);
					m->SetAmount(a);
				}
				emitter->AddModule(std::move(m));
			}
			else if (type == "ForceOverLifetime")
			{
				auto m = std::make_unique<ForceOverLifetimeModule>();
				if (moduleData.contains("direction"))
				{
					Vector3 d;
					d.x = moduleData["direction"].value("x", 0.0f);
					d.y = moduleData["direction"].value("y", 1.0f);
					d.z = moduleData["direction"].value("z", 0.0f);
					m->SetDirection(d);
				}
				m->SetStrengths(moduleData.value("startStrength", 1.0f), moduleData.value("endStrength", 0.0f));
				m->SetEasingType(static_cast<EasingType>(moduleData.value("easingType", 0)));
				emitter->AddModule(std::move(m));
			}
			else if (type == "Orbit")
			{
				auto m = std::make_unique<OrbitModule>();
				m->SetOrbitSpeed(moduleData.value("orbitSpeed", 90.0f));
				if (moduleData.contains("orbitAxis"))
				{
					Vector3 axis;
					axis.x = moduleData["orbitAxis"].value("x", 0.0f);
					axis.y = moduleData["orbitAxis"].value("y", 1.0f);
					axis.z = moduleData["orbitAxis"].value("z", 0.0f);
					m->SetOrbitAxis(axis);
				}
				emitter->AddModule(std::move(m));
			}
			else if (type == "Noise")
			{
				auto m = std::make_unique<NoiseModule>();
				m->SetStrength(moduleData.value("strength", 1.0f));
				m->SetFrequency(moduleData.value("frequency", 1.0f));
				emitter->AddModule(std::move(m));
			}
			else if (type == "VelocityLimit")
			{
				auto m = std::make_unique<VelocityLimitModule>();
				m->SetMaxSpeed(moduleData.value("maxSpeed", 10.0f));
				emitter->AddModule(std::move(m));
			}
			// ForceFieldModules
			else if (type == "Attractor")
			{
				auto m = std::make_unique<AttractorModule>();
				if (moduleData.contains("target"))
				{
					Vector3 t;
					t.x = moduleData["target"].value("x", 0.0f);
					t.y = moduleData["target"].value("y", 0.0f);
					t.z = moduleData["target"].value("z", 0.0f);
					m->SetTarget(t);
				}
				m->SetStrength(moduleData.value("strength", 1.0f));
				m->SetRange(moduleData.value("range", 10.0f));
				m->SetFalloffType(static_cast<FalloffType>(moduleData.value("falloffType", 2)));
				emitter->AddModule(std::move(m));
			}
			else if (type == "Vortex")
			{
				auto m = std::make_unique<VortexModule>();
				if (moduleData.contains("axis"))
				{
					Vector3 a;
					a.x = moduleData["axis"].value("x", 0.0f);
					a.y = moduleData["axis"].value("y", 1.0f);
					a.z = moduleData["axis"].value("z", 0.0f);
					m->SetAxis(a);
				}
				if (moduleData.contains("center"))
				{
					Vector3 c;
					c.x = moduleData["center"].value("x", 0.0f);
					c.y = moduleData["center"].value("y", 0.0f);
					c.z = moduleData["center"].value("z", 0.0f);
					m->SetCenter(c);
				}
				m->SetStrength(moduleData.value("strength", 1.0f));
				m->SetRange(moduleData.value("range", 10.0f));
				emitter->AddModule(std::move(m));
			}
			// SpawnShapeModules - InitialRotation
			else if (type == "InitialRotation")
			{
				auto m = std::make_unique<InitialRotationModule>();
				Vector3 minAng = {0, 0, 0};
				Vector3 maxAng = {360.0f, 360.0f, 360.0f};

				if (moduleData.contains("minAngle_v3"))
				{
					minAng.x = moduleData["minAngle_v3"].value("x", 0.0f);
					minAng.y = moduleData["minAngle_v3"].value("y", 0.0f);
					minAng.z = moduleData["minAngle_v3"].value("z", 0.0f);
				}
				else if (moduleData.contains("minAngle"))
				{ // 互換性
					minAng.z = moduleData.value("minAngle", 0.0f);
				}

				if (moduleData.contains("maxAngle_v3"))
				{
					maxAng.x = moduleData["maxAngle_v3"].value("x", 360.0f);
					maxAng.y = moduleData["maxAngle_v3"].value("y", 360.0f);
					maxAng.z = moduleData["maxAngle_v3"].value("z", 360.0f);
				}
				else if (moduleData.contains("maxAngle"))
				{ // 互換性
					maxAng.z = moduleData.value("maxAngle", 360.0f);
				}

				m->SetRotationRange(minAng, maxAng);
				emitter->AddModule(std::move(m));
			}
			else if (type == "RibbonInterpolation")
			{
				auto m = std::make_unique<RibbonInterpolationModule>();
				m->SetMaxDistance(moduleData.value("maxDistance", 0.1f));
				emitter->AddModule(std::move(m));
			}
			else if (type == "MultiSourceRibbon")
			{
				auto m = std::make_unique<MultiSourceRibbonModule>();
				m->SetSpawnRate(moduleData.value("spawnRate", 60.0f));
				m->SetParticleLifetime(moduleData.value("particleLifetime", 0.2f));
				m->SetSpawnOnlyWhenMoving(moduleData.value("spawnOnlyWhenMoving", true));
				m->SetMinMoveDistance(moduleData.value("minMoveDistance", 0.05f));
				if (moduleData.contains("initialColor"))
				{
					Vector4 color;
					color.x = moduleData["initialColor"].value("r", 1.0f);
					color.y = moduleData["initialColor"].value("g", 0.8f);
					color.z = moduleData["initialColor"].value("b", 0.2f);
					color.w = moduleData["initialColor"].value("a", 1.0f);
					m->SetInitialColor(color);
				}
				emitter->AddModule(std::move(m));
			}
			else if (type == "TextureSheet")
			{
				auto m = std::make_unique<TextureSheetModule>();
				uint32_t columns = moduleData.value("columns", 4u);
				uint32_t rows = moduleData.value("rows", 4u);
				m->SetGridSize(columns, rows);
				m->SetFrameRate(moduleData.value("frameRate", 30.0f));
				m->SetPlayMode(static_cast<TextureSheetPlayMode>(moduleData.value("playMode", 0)));
				m->SetStartFrame(moduleData.value("startFrame", 0u));
				emitter->AddModule(std::move(m));
			}
			// Motion Effect Modules
			else if (type == "RadialVelocity")
			{
				auto m = std::make_unique<RadialVelocityModule>();
				float minSpeed = moduleData.value("minSpeed", 5.0f);
				float maxSpeed = moduleData.value("maxSpeed", 5.0f);
				m->SetSpeedRange(minSpeed, maxSpeed);
				emitter->AddModule(std::move(m));
			}
			else if (type == "VelocityOverLifetime")
			{
				auto m = std::make_unique<VelocityOverLifetimeModule>();
				m->SetStartMultiplier(moduleData.value("startMultiplier", 1.0f));
				m->SetEndMultiplier(moduleData.value("endMultiplier", 0.0f));
				emitter->AddModule(std::move(m));
			}
			else if (type == "StretchByVelocity")
			{
				auto m = std::make_unique<StretchByVelocityModule>();
				m->SetStretchFactor(moduleData.value("stretchFactor", 0.1f));
				m->SetMinStretch(moduleData.value("minStretch", 1.0f));
				m->SetMaxStretch(moduleData.value("maxStretch", 5.0f));
				m->SetPreserveVolume(moduleData.value("preserveVolume", false));
				emitter->AddModule(std::move(m));
			}
			else if (type == "Wind")
			{
				auto m = std::make_unique<WindModule>();
				if (moduleData.contains("direction"))
				{
					Vector3 dir;
					dir.x = moduleData["direction"].value("x", 1.0f);
					dir.y = moduleData["direction"].value("y", 0.0f);
					dir.z = moduleData["direction"].value("z", 0.0f);
					m->SetDirection(dir);
				}
				m->SetStrength(moduleData.value("strength", 1.0f));
				m->SetTurbulence(moduleData.value("turbulence", 0.0f));
				m->SetTurbulenceFrequency(moduleData.value("turbulenceFrequency", 1.0f));
				emitter->AddModule(std::move(m));
			}
			else if (type == "Flicker")
			{
				auto m = std::make_unique<FlickerModule>();
				m->SetFrequency(moduleData.value("frequency", 10.0f));
				m->SetMinAlpha(moduleData.value("minAlpha", 0.3f));
				m->SetMaxAlpha(moduleData.value("maxAlpha", 1.0f));
				m->SetRandomPhase(moduleData.value("randomPhase", true));
				m->SetUseNoise(moduleData.value("useNoise", false));
				emitter->AddModule(std::move(m));
			}
			else if (type == "AlphaFade")
			{
				auto m = std::make_unique<AlphaFadeModule>();
				m->SetStartAlpha(moduleData.value("startAlpha", 1.0f));
				m->SetEndAlpha(moduleData.value("endAlpha", 0.0f));
				m->SetEaseIn(moduleData.value("easeIn", false));
				m->SetEaseOut(moduleData.value("easeOut", true));
				emitter->AddModule(std::move(m));
			}
			else if (type == "RotationBySpeed")
			{
				auto m = std::make_unique<RotationBySpeedModule>();
				m->SetRotationPerSpeed(moduleData.value("rotationPerSpeed", 90.0f));
				m->SetMinSpeed(moduleData.value("minSpeed", 0.0f));
				m->SetMaxSpeed(moduleData.value("maxSpeed", 0.0f));
				emitter->AddModule(std::move(m));
			}
			else if (type == "SineWave")
			{
				auto m = std::make_unique<SineWaveModule>();
				m->SetAmplitude(moduleData.value("amplitude", 1.0f));
				m->SetFrequency(moduleData.value("frequency", 2.0f));
				if (moduleData.contains("axis"))
				{
					Vector3 axis;
					axis.x = moduleData["axis"].value("x", 1.0f);
					axis.y = moduleData["axis"].value("y", 0.0f);
					axis.z = moduleData["axis"].value("z", 0.0f);
					m->SetAxis(axis);
				}
				m->SetRandomPhase(moduleData.value("randomPhase", true));
				emitter->AddModule(std::move(m));
			}
			else if (type == "Spiral")
			{
				auto m = std::make_unique<SpiralModule>();
				m->SetRadius(moduleData.value("radius", 1.0f));
				m->SetSpeed(moduleData.value("speed", 180.0f));
				m->SetLift(moduleData.value("lift", 1.0f));
				m->SetRandomPhase(moduleData.value("randomPhase", true));
				m->SetExpandRadius(moduleData.value("expandRadius", false));
				m->SetExpansionRate(moduleData.value("expansionRate", 0.5f));
				emitter->AddModule(std::move(m));
			}
			else if (type == "Twist")
			{
				auto m = std::make_unique<TwistModule>();
				m->SetTwistSpeed(moduleData.value("twistSpeed", 90.0f));
				m->SetTwistStrength(moduleData.value("twistStrength", 1.0f));
				m->SetHeightAxis(moduleData.value("heightAxis", 1));
				emitter->AddModule(std::move(m));
			}
			else
			{
				// Registry entries must never be silently ignored by the migration loader.
				return nullptr;
			}
		}
	}

	if (data.contains("dynamicInputs"))
	{
		const auto& bindings = data["dynamicInputs"];
		if (!bindings.is_array() || bindings.size() > 1024) return nullptr;
		for (const auto& entry : bindings)
		{
			if (!entry.is_object()) return nullptr;
			DynamicParameterBinding binding;
			binding.moduleId = entry.value("module", "");
			binding.parameterId = entry.value("parameter", "");
			const int type = entry.value("type", -1);
			const int mode = entry.value("mode", -1);
			if (type < static_cast<int>(ModuleParameterType::Float) || type > static_cast<int>(ModuleParameterType::StructArray) ||
				mode < static_cast<int>(DynamicBindingMode::Constant) || mode > static_cast<int>(DynamicBindingMode::EmitterParameter)) return nullptr;
			binding.type = static_cast<ModuleParameterType>(type);
			binding.mode = static_cast<DynamicBindingMode>(mode);
			binding.emitterParameter = entry.value("emitterParameter", "");
			if (!entry.contains("fallback") || !entry.contains("minimum") || !entry.contains("maximum") ||
				!LoadBindingValue(entry["fallback"], binding.type, binding.fallback) ||
				!LoadBindingValue(entry["minimum"], binding.type, binding.minimum) ||
				!LoadBindingValue(entry["maximum"], binding.type, binding.maximum)) return nullptr;
			if (entry.contains("keys"))
			{
				if (!entry["keys"].is_array() || entry["keys"].size() > 1024) return nullptr;
				for (const auto& key : entry["keys"])
				{
					if (!key.is_object() || !key.contains("value")) return nullptr;
					DynamicBindingKey parsed;
					parsed.time = key.value("time", -1.0f);
					if (parsed.time < 0.0f || parsed.time > 1.0f || !key["value"].is_object()) return nullptr;
					parsed.value = { key["value"].value("x", 0.0f), key["value"].value("y", 0.0f), key["value"].value("z", 0.0f), key["value"].value("w", 0.0f) };
					binding.keys.push_back(parsed);
				}
			}
			if (!emitter->SetDynamicBinding(std::move(binding))) return nullptr;
		}
	}

	// レンダラー
	if (data.contains("renderer"))
	{
		const auto& rendererData = data["renderer"];
		std::string type = rendererData.value("type", "Sprite");
		BlendMode blendMode = static_cast<BlendMode>(rendererData.value("blendMode", 1));
		std::string texturePath = rendererData.value("texture", "./Resources/uvChecker.png");

		if (type == "Sprite")
		{
			auto renderer = std::make_unique<SpriteRenderer>();
			renderer->Initialize(texturePath);
			renderer->SetBlendMode(blendMode);
			emitter->SetRenderer(std::move(renderer));
		}
		else if (type == "Ribbon")
		{
			auto renderer = std::make_unique<TrailRenderer>();
			renderer->Initialize(texturePath);
			renderer->SetBlendMode(blendMode);
			renderer->SetBillboard(rendererData.value("billboard", true));
			renderer->SetTrailWidth(rendererData.value("trailWidth", 0.5f));
			renderer->SetTrailLifetime(rendererData.value("trailLifetime", 1.0f));
			renderer->SetWidthFade(rendererData.value("widthFade", true));
			renderer->SetAlphaFade(rendererData.value("alphaFade", true));
			renderer->SetRecordInterval(rendererData.value("recordInterval", 0.016f));
			renderer->SetMinSegmentDistance(rendererData.value("minSegmentDistance", 0.1f));
			renderer->SetTextureMode(static_cast<RibbonTextureMode>(rendererData.value("textureMode", 0)));
			renderer->SetTileScale(rendererData.value("tileScale", 1.0f));
			emitter->SetRenderer(std::move(renderer));
		}
		else if (type == "Mesh")
		{
			auto renderer = std::make_unique<MeshRenderer>();
			renderer->Initialize(texturePath);
			renderer->SetBlendMode(blendMode);
			// Load primitive options
			PrimitiveOptions options;
			if (rendererData.contains("primitiveOptions"))
			{
				const auto& opts = rendererData["primitiveOptions"];
				options.segments = opts.value("segments", 16u);
				options.rings = opts.value("rings", 8u);
				options.innerRadius = opts.value("innerRadius", 0.5f);
				options.outerRadius = opts.value("outerRadius", 1.0f);
				options.tubeRadius = opts.value("tubeRadius", 0.3f);
				options.turns = opts.value("turns", 2.0f);
				options.points = opts.value("points", 5u);
				options.withCaps = opts.value("withCaps", true);
				options.withCaps = opts.value("withCaps", true);
				options.doubleSided = opts.value("doubleSided", false);

				// Cube Options
				if (opts.contains("cubeSize"))
				{
					options.cubeSize.x = opts["cubeSize"].value("x", 1.0f);
					options.cubeSize.y = opts["cubeSize"].value("y", 1.0f);
					options.cubeSize.z = opts["cubeSize"].value("z", 1.0f);
				}
				if (opts.contains("cubeFaceVisible") && opts["cubeFaceVisible"].is_array())
				{
					auto faces = opts["cubeFaceVisible"];
					for (int i = 0; i < 6 && i < faces.size(); ++i)
					{
						options.cubeFaceVisible[i] = faces[i].get<bool>();
					}
				}
			}
			renderer->SetPrimitive(static_cast<PrimitiveType>(rendererData.value("primitiveType", 0)), options);
			renderer->SetBillboard(rendererData.value("billboard", false));
			renderer->SetScale(rendererData.value("scale", 1.0f));
			emitter->SetRenderer(std::move(renderer));
		}
		else
		{
			return nullptr;
		}

		if (auto* renderer = emitter->GetRenderer())
		{
			if (rendererData.contains("emissive"))
			{
				const auto& emissive = rendererData["emissive"];
				if (!emissive.is_object()) return nullptr;
				EmissiveSettings settings;
				settings.enabled = emissive.value("enabled", false);
				const std::string source = emissive.value("source", "Uniform");
				if (source == "Uniform") settings.source = EmissiveSource::Uniform;
				else if (source == "BaseTextureMask") settings.source = EmissiveSource::BaseTextureMask;
				else if (source == "EmissiveTexture") settings.source = EmissiveSource::EmissiveTexture;
				else return nullptr;
				if (emissive.contains("color"))
				{
					const auto& color = emissive["color"];
					if (!color.is_object()) return nullptr;
					settings.color = { color.value("r", 1.0f), color.value("g", 1.0f), color.value("b", 1.0f) };
				}
				settings.intensity = emissive.value("intensity", 1.0f);
				settings.bloomContribution = emissive.value("bloomContribution", 1.0f);
				renderer->SetEmissiveSettings(settings);
				renderer->SetEmissiveTexture(emissive.value("texture", ""));
			}
			if (rendererData.contains("tintColor"))
			{
				Vector4 tint;
				tint.x = rendererData["tintColor"].value("r", 1.0f);
				tint.y = rendererData["tintColor"].value("g", 1.0f);
				tint.z = rendererData["tintColor"].value("b", 1.0f);
				tint.w = rendererData["tintColor"].value("a", 1.0f);
				renderer->SetTintColor(tint);
			}
		}
	}
	else
	{
		// デフォルトレンダラー
		auto renderer = std::make_unique<SpriteRenderer>();
		renderer->Initialize("./Resources/uvChecker.png");
		emitter->SetRenderer(std::move(renderer));
	}

	return emitter;
}

static void SaveEmitter(const ParticleEmitter& emitter, json& data)
{
	data["name"] = emitter.GetName();
	data["maxParticles"] = emitter.GetMaxParticles();
	data["simulationMode"] = static_cast<int>(emitter.GetSimulationMode());
	data["position"] = {
		{"x", emitter.GetPosition().x},
		{"y", emitter.GetPosition().y},
		{"z", emitter.GetPosition().z}
	};
	data["followOffset"] = {
		{"x", emitter.GetFollowOffset().x},
		{"y", emitter.GetFollowOffset().y},
		{"z", emitter.GetFollowOffset().z}
	};
	data["followEmitterIndex"] = emitter.GetFollowEmitterIndex();
	data["gpuEventSourceEmitterIndex"] = emitter.GetGPUEventSourceEmitterIndex();
	data["gpuEventTrigger"] = emitter.GetGPUEventTrigger();
	data["gpuEventProbability"] = emitter.GetGPUEventProbability();
	data["gpuEventInheritVelocity"] = emitter.GetGPUEventInheritVelocity();
	data["gpuEventVelocityScale"] = emitter.GetGPUEventVelocityScale();
	data["gpuEventInheritColor"] = emitter.GetGPUEventInheritColor();
	data["parameters"] = json::array();
	for (const auto& [name, value] : emitter.GetParameterStore().GetValues())
	{
		json entry;
		entry["name"] = name;
		std::visit([&](const auto& typedValue)
		{
			using T = std::decay_t<decltype(typedValue)>;
			if constexpr (std::is_same_v<T, float>) { entry["type"] = "float"; entry["value"] = typedValue; }
			else if constexpr (std::is_same_v<T, uint32_t>) { entry["type"] = "uint"; entry["value"] = typedValue; }
			else if constexpr (std::is_same_v<T, int32_t>) { entry["type"] = "int"; entry["value"] = typedValue; }
			else if constexpr (std::is_same_v<T, bool>) { entry["type"] = "bool"; entry["value"] = typedValue; }
			else if constexpr (std::is_same_v<T, std::string>) { entry["type"] = "string"; entry["value"] = typedValue; }
			else if constexpr (std::is_same_v<T, Vector3>) { entry["type"] = "vector3"; entry["value"] = { {"x", typedValue.x}, {"y", typedValue.y}, {"z", typedValue.z} }; }
			else if constexpr (std::is_same_v<T, Vector4>) { entry["type"] = "vector4"; entry["value"] = { {"x", typedValue.x}, {"y", typedValue.y}, {"z", typedValue.z}, {"w", typedValue.w} }; }
		}, value);
		data["parameters"].push_back(std::move(entry));
	}
	data["dynamicInputs"] = json::array();
	for (const auto& binding : emitter.GetDynamicBindings())
	{
		json entry = {
			{"module", binding.moduleId}, {"parameter", binding.parameterId},
			{"type", static_cast<int>(binding.type)}, {"mode", static_cast<int>(binding.mode)},
			{"emitterParameter", binding.emitterParameter}, {"fallback", SaveBindingValue(binding.fallback)},
			{"minimum", SaveBindingValue(binding.minimum)}, {"maximum", SaveBindingValue(binding.maximum)},
			{"keys", json::array()}
		};
		for (const auto& key : binding.keys)
		{
			entry["keys"].push_back({ {"time", key.time}, {"value", {{"x", key.value.x}, {"y", key.value.y}, {"z", key.value.z}, {"w", key.value.w}}} });
		}
		data["dynamicInputs"].push_back(std::move(entry));
	}

	// 移動時のみ生成
	data["spawnOnlyWhenMoving"] = emitter.GetSpawnOnlyWhenMoving();
	data["minMoveDistance"] = emitter.GetMinMoveDistance();

	// ライフサイクル設定
	data["duration"] = emitter.GetDuration();
	data["startDelay"] = emitter.GetStartDelay();
	data["loopBehavior"] = static_cast<int>(emitter.GetLoopBehavior());
	data["loopCount"] = emitter.GetLoopCount();
	data["inactiveResponse"] = static_cast<int>(emitter.GetInactiveResponse());

	// モジュール
	data["modules"] = json::array();
	for (size_t i = 0; i < emitter.GetModuleCount(); ++i)
	{
		const auto* module = emitter.GetModule(i);
		if (!module) continue;

		json moduleData;
		std::string type = module->GetName();
		moduleData["type"] = type;
		if (const auto* descriptor = ModuleDescriptorRegistry::GetInstance().Find(type))
		{
			moduleData["version"] = descriptor->version;
		}

		// 各モジュールタイプ別にパラメータを保存
		if (auto* m = dynamic_cast<const SpawnRateModule*>(module))
		{
			moduleData["rate"] = m->GetRate();
		}
		else if (auto* m = dynamic_cast<const SpawnBurstModule*>(module))
		{
			moduleData["count"] = m->GetCount();
			moduleData["delay"] = m->GetDelay();
			moduleData["interval"] = m->GetInterval();
			moduleData["loops"] = m->GetLoops();
		}
		else if (auto* m = dynamic_cast<const SpawnShapeModule*>(module))
		{
			moduleData["shapeType"] = static_cast<int>(m->GetShapeType());
			moduleData["innerRadius"] = m->GetInnerRadius();
			moduleData["outerRadius"] = m->GetOuterRadius();
			Vector3 bs = m->GetBoxSize();
			moduleData["boxSize"] = {{"x", bs.x}, {"y", bs.y}, {"z", bs.z}};
			moduleData["coneHeight"] = m->GetConeHeight();
			Vector3 ls = m->GetLineStart();
			Vector3 le = m->GetLineEnd();
			moduleData["lineStart"] = {{"x", ls.x}, {"y", ls.y}, {"z", ls.z}};
			moduleData["lineEnd"] = {{"x", le.x}, {"y", le.y}, {"z", le.z}};
			moduleData["emitFromSurface"] = m->GetEmitFromSurface();
			moduleData["initialSpeed"] = m->GetInitialSpeed();
			moduleData["spawnLocation"] = static_cast<int>(m->GetSpawnLocation());
			moduleData["arcAngle"] = m->GetArcAngle();
		}
		else if (auto* m = dynamic_cast<const InitialPositionModule*>(module))
		{
			const Vector3 minOffset = m->GetMinOffset();
			const Vector3 maxOffset = m->GetMaxOffset();
			moduleData["min"] = {{"x", minOffset.x}, {"y", minOffset.y}, {"z", minOffset.z}};
			moduleData["max"] = {{"x", maxOffset.x}, {"y", maxOffset.y}, {"z", maxOffset.z}};
		}
		else if (auto* m = dynamic_cast<const InitialLifetimeModule*>(module))
		{
			moduleData["min"] = m->GetMinLifetime();
			moduleData["max"] = m->GetMaxLifetime();
		}
		else if (auto* m = dynamic_cast<const InitialVelocityModule*>(module))
		{
			Vector3 minV = m->GetMinVelocity();
			Vector3 maxV = m->GetMaxVelocity();
			moduleData["min"] = {{"x", minV.x}, {"y", minV.y}, {"z", minV.z}};
			moduleData["max"] = {{"x", maxV.x}, {"y", maxV.y}, {"z", maxV.z}};
		}
		else if (auto* m = dynamic_cast<const InitialScaleModule*>(module))
		{
			Vector3 minS = m->GetMinScale();
			Vector3 maxS = m->GetMaxScale();
			moduleData["min"] = {{"x", minS.x}, {"y", minS.y}, {"z", minS.z}};
			moduleData["max"] = {{"x", maxS.x}, {"y", maxS.y}, {"z", maxS.z}};
		}
		else if (auto* m = dynamic_cast<const InitialColorModule*>(module))
		{
			Vector4 minC = m->GetMinColor();
			Vector4 maxC = m->GetMaxColor();
			moduleData["min"] = {{"r", minC.x}, {"g", minC.y}, {"b", minC.z}, {"a", minC.w}};
			moduleData["max"] = {{"r", maxC.x}, {"g", maxC.y}, {"b", maxC.z}, {"a", maxC.w}};
		}
		else if (auto* m = dynamic_cast<const AssignRibbonIdModule*>(module))
		{
			moduleData["groupCount"] = m->GetGroupCount();
		}
		else if (auto* m = dynamic_cast<const SubEmitterModule*>(module))
		{
			moduleData["configs"] = json::array();
			for (const auto& config : m->GetConfigs())
			{
				moduleData["configs"].push_back({
					{ "effectPath", config.effectPath },
					{ "trigger", static_cast<int>(config.trigger) },
					{ "inheritPosition", config.inheritPosition },
					{ "inheritVelocity", config.inheritVelocity },
					{ "inheritVelocityScale", config.inheritVelocityScale },
					{ "inheritColor", config.inheritColor },
					{ "inheritScale", config.inheritScale },
					{ "probability", config.probability },
					{ "continuousRate", config.continuousRate }
				});
			}
		}
		else if (auto* m = dynamic_cast<const GravityModule*>(module))
		{
			Vector3 minG = m->GetMinGravity();
			Vector3 maxG = m->GetMaxGravity();
			moduleData["min"] = {{"x", minG.x}, {"y", minG.y}, {"z", minG.z}};
			moduleData["max"] = {{"x", maxG.x}, {"y", maxG.y}, {"z", maxG.z}};
		}
		else if (auto* m = dynamic_cast<const DragModule*>(module))
		{
			moduleData["min"] = m->GetMinDrag();
			moduleData["max"] = m->GetMaxDrag();
		}
		else if (auto* m = dynamic_cast<const ColorFadeModule*>(module))
		{
			moduleData["useInitialColor"] = m->GetUseInitialColor();
			Vector4 start = m->GetStartColor();
			Vector4 end = m->GetEndColor();
			moduleData["start"] = {{"r", start.x}, {"g", start.y}, {"b", start.z}, {"a", start.w}};
			moduleData["end"] = {{"r", end.x}, {"g", end.y}, {"b", end.z}, {"a", end.w}};
			moduleData["easingType"] = static_cast<int>(m->GetEasingType());
			if (m->HasGradient())
			{
				moduleData["gradient"] = json::array();
				for (const auto& key : m->GetGradient().keys) moduleData["gradient"].push_back({ {"time", key.time}, {"r", key.color.x}, {"g", key.color.y}, {"b", key.color.z}, {"a", key.color.w} });
			}
		}
		else if (auto* m = dynamic_cast<const ScaleOverLifetimeModule*>(module))
		{
			Vector3 start = m->GetStartScale();
			Vector3 end = m->GetEndScale();
			moduleData["start"] = {{"x", start.x}, {"y", start.y}, {"z", start.z}};
			moduleData["end"] = {{"x", end.x}, {"y", end.y}, {"z", end.z}};
			moduleData["easingType"] = static_cast<int>(m->GetEasingType());
			if (m->HasCurve())
			{
				moduleData["curve"] = json::array();
				for (const auto& key : m->GetCurve().keys) moduleData["curve"].push_back({ {"time", key.time}, {"value", key.value} });
			}
		}
		// Phase 3: New modules
		else if (auto* m = dynamic_cast<const AccelerationModule*>(module))
		{
			Vector3 acc = m->GetAcceleration();
			moduleData["acceleration"] = {{"x", acc.x}, {"y", acc.y}, {"z", acc.z}};
		}
		else if (auto* m = dynamic_cast<const CurlNoiseModule*>(module))
		{
			moduleData["strength"] = m->GetStrength();
			moduleData["frequency"] = m->GetFrequency();
			moduleData["octaves"] = m->GetOctaves();
			moduleData["scrollSpeed"] = m->GetScrollSpeed();
		}
		else if (auto* m = dynamic_cast<const SizeBySpeedModule*>(module))
		{
			moduleData["minSpeed"] = m->GetMinSpeed();
			moduleData["maxSpeed"] = m->GetMaxSpeed();
			Vector3 minS = m->GetMinScale();
			Vector3 maxS = m->GetMaxScale();
			moduleData["minScale"] = {{"x", minS.x}, {"y", minS.y}, {"z", minS.z}};
			moduleData["maxScale"] = {{"x", maxS.x}, {"y", maxS.y}, {"z", maxS.z}};
		}
		else if (auto* m = dynamic_cast<const ColorBySpeedModule*>(module))
		{
			moduleData["minSpeed"] = m->GetMinSpeed();
			moduleData["maxSpeed"] = m->GetMaxSpeed();
			Vector4 minC = m->GetMinColor();
			Vector4 maxC = m->GetMaxColor();
			moduleData["minColor"] = {{"r", minC.x}, {"g", minC.y}, {"b", minC.z}, {"a", minC.w}};
			moduleData["maxColor"] = {{"r", maxC.x}, {"g", maxC.y}, {"b", maxC.z}, {"a", maxC.w}};
		}
		else if (auto* m = dynamic_cast<const CollisionModule*>(module))
		{
			moduleData["mode"] = static_cast<int>(m->GetMode());
			moduleData["bounce"] = m->GetBounce();
			moduleData["friction"] = m->GetFriction();
			moduleData["planeHeight"] = m->GetPlaneHeight();
			Vector3 bc = m->GetBoxCenter();
			Vector3 bs = m->GetBoxSize();
			moduleData["boxCenter"] = {{"x", bc.x}, {"y", bc.y}, {"z", bc.z}};
			moduleData["boxSize"] = {{"x", bs.x}, {"y", bs.y}, {"z", bs.z}};
			moduleData["killOnCollision"] = m->GetKillOnCollision();
		}
		else if (auto* m = dynamic_cast<const KillZoneModule*>(module))
		{
			moduleData["zoneType"] = static_cast<int>(m->GetZoneType());
			Vector3 c = m->GetCenter();
			Vector3 bs = m->GetBoxSize();
			moduleData["center"] = {{"x", c.x}, {"y", c.y}, {"z", c.z}};
			moduleData["boxSize"] = {{"x", bs.x}, {"y", bs.y}, {"z", bs.z}};
			moduleData["radius"] = m->GetRadius();
			moduleData["killInside"] = m->GetKillInside();
		}
		else if (auto* m = dynamic_cast<const SprintToTargetModule*>(module))
		{
			Vector3 t = m->GetTarget();
			moduleData["target"] = {{"x", t.x}, {"y", t.y}, {"z", t.z}};
			moduleData["acceleration"] = m->GetAcceleration();
			moduleData["arriveRadius"] = m->GetArriveRadius();
			moduleData["killOnArrive"] = m->GetKillOnArrive();
			moduleData["maxDistance"] = m->GetMaxDistance();
			moduleData["speedBoost"] = m->GetSpeedBoost();
			moduleData["useSpeedCurve"] = m->GetUseSpeedCurve();
		}
		// AdvancedModules
		else if (auto* m = dynamic_cast<const RotationOverLifetimeModule*>(module))
		{
			moduleData["startSpeed"] = m->GetStartSpeed();
			moduleData["endSpeed"] = m->GetEndSpeed();
			moduleData["easingType"] = static_cast<int>(m->GetEasingType());
		}
		else if (auto* m = dynamic_cast<const JitterModule*>(module))
		{
			Vector3 a = m->GetAmount();
			moduleData["amount"] = {{"x", a.x}, {"y", a.y}, {"z", a.z}};
		}
		else if (auto* m = dynamic_cast<const ForceOverLifetimeModule*>(module))
		{
			Vector3 d = m->GetDirection();
			moduleData["direction"] = {{"x", d.x}, {"y", d.y}, {"z", d.z}};
			moduleData["startStrength"] = m->GetStartStrength();
			moduleData["endStrength"] = m->GetEndStrength();
			moduleData["easingType"] = static_cast<int>(m->GetEasingType());
		}
		else if (auto* m = dynamic_cast<const FaceVelocityModule*>(module))
		{
			moduleData["use2DAlignment"] = m->IsUse2DAlignment();
		}
		else if (auto* m = dynamic_cast<const OrbitModule*>(module))
		{
			moduleData["orbitSpeed"] = m->GetOrbitSpeed();
			Vector3 axis = m->GetOrbitAxis();
			moduleData["orbitAxis"] = {{"x", axis.x}, {"y", axis.y}, {"z", axis.z}};
		}
		else if (auto* m = dynamic_cast<const NoiseModule*>(module))
		{
			moduleData["strength"] = m->GetStrength();
			moduleData["frequency"] = m->GetFrequency();
		}
		else if (auto* m = dynamic_cast<const VelocityLimitModule*>(module))
		{
			moduleData["maxSpeed"] = m->GetMaxSpeed();
		}
		// ForceFieldModules
		else if (auto* m = dynamic_cast<const AttractorModule*>(module))
		{
			Vector3 t = m->GetTarget();
			moduleData["target"] = {{"x", t.x}, {"y", t.y}, {"z", t.z}};
			moduleData["strength"] = m->GetStrength();
			moduleData["range"] = m->GetRange();
			moduleData["falloffType"] = static_cast<int>(m->GetFalloffType());
		}
		else if (auto* m = dynamic_cast<const VortexModule*>(module))
		{
			Vector3 axis = m->GetAxis();
			Vector3 center = m->GetCenter();
			moduleData["axis"] = {{"x", axis.x}, {"y", axis.y}, {"z", axis.z}};
			moduleData["center"] = {{"x", center.x}, {"y", center.y}, {"z", center.z}};
			moduleData["strength"] = m->GetStrength();
			moduleData["range"] = m->GetRange();
		}
		// SpawnShapeModules - InitialRotation
		else if (auto* m = dynamic_cast<const InitialRotationModule*>(module))
		{
			Vector3 minAng = m->GetMinAngle();
			Vector3 maxAng = m->GetMaxAngle();
			moduleData["minAngle_v3"] = {{"x", minAng.x}, {"y", minAng.y}, {"z", minAng.z}};
			moduleData["maxAngle_v3"] = {{"x", maxAng.x}, {"y", maxAng.y}, {"z", maxAng.z}};
			
			// 後方互換性のためZ成分だけを古いキーにも保存（オプション）
			moduleData["minAngle"] = minAng.z;
			moduleData["maxAngle"] = maxAng.z;
		}
		else if (auto* m = dynamic_cast<const RibbonInterpolationModule*>(module))
		{
			moduleData["maxDistance"] = m->GetMaxDistance();
		}
		else if (auto* m = dynamic_cast<const MultiSourceRibbonModule*>(module))
		{
			moduleData["spawnRate"] = m->GetSpawnRate();
			moduleData["particleLifetime"] = m->GetParticleLifetime();
			moduleData["spawnOnlyWhenMoving"] = m->GetSpawnOnlyWhenMoving();
			moduleData["minMoveDistance"] = m->GetMinMoveDistance();
			Vector4 color = m->GetInitialColor();
			moduleData["initialColor"] = {{"r", color.x}, {"g", color.y}, {"b", color.z}, {"a", color.w}};
		}
		else if (auto* m = dynamic_cast<const TextureSheetModule*>(module))
		{
			moduleData["columns"] = m->GetColumns();
			moduleData["rows"] = m->GetRows();
			moduleData["frameRate"] = m->GetFrameRate();
			moduleData["playMode"] = static_cast<int>(m->GetPlayMode());
		}
		// Motion Effect Modules
		else if (auto* m = dynamic_cast<const RadialVelocityModule*>(module))
		{
			moduleData["minSpeed"] = m->GetMinSpeed();
			moduleData["maxSpeed"] = m->GetMaxSpeed();
		}
		else if (auto* m = dynamic_cast<const VelocityOverLifetimeModule*>(module))
		{
			moduleData["startMultiplier"] = m->GetStartMultiplier();
			moduleData["endMultiplier"] = m->GetEndMultiplier();
		}
		else if (auto* m = dynamic_cast<const StretchByVelocityModule*>(module))
		{
			moduleData["stretchFactor"] = m->GetStretchFactor();
			moduleData["minStretch"] = m->GetMinStretch();
			moduleData["maxStretch"] = m->GetMaxStretch();
			moduleData["preserveVolume"] = m->GetPreserveVolume();
		}
		else if (auto* m = dynamic_cast<const WindModule*>(module))
		{
			Vector3 dir = m->GetDirection();
			moduleData["direction"] = {{"x", dir.x}, {"y", dir.y}, {"z", dir.z}};
			moduleData["strength"] = m->GetStrength();
			moduleData["turbulence"] = m->GetTurbulence();
			moduleData["turbulenceFrequency"] = m->GetTurbulenceFrequency();
		}
		else if (auto* m = dynamic_cast<const FlickerModule*>(module))
		{
			moduleData["frequency"] = m->GetFrequency();
			moduleData["minAlpha"] = m->GetMinAlpha();
			moduleData["maxAlpha"] = m->GetMaxAlpha();
			moduleData["randomPhase"] = m->GetRandomPhase();
			moduleData["useNoise"] = m->GetUseNoise();
		}
		else if (auto* m = dynamic_cast<const AlphaFadeModule*>(module))
		{
			moduleData["startAlpha"] = m->GetStartAlpha();
			moduleData["endAlpha"] = m->GetEndAlpha();
			moduleData["easeIn"] = m->GetEaseIn();
			moduleData["easeOut"] = m->GetEaseOut();
		}
		else if (auto* m = dynamic_cast<const RotationBySpeedModule*>(module))
		{
			moduleData["rotationPerSpeed"] = m->GetRotationPerSpeed();
			moduleData["minSpeed"] = m->GetMinSpeed();
			moduleData["maxSpeed"] = m->GetMaxSpeed();
		}
		else if (auto* m = dynamic_cast<const SineWaveModule*>(module))
		{
			moduleData["amplitude"] = m->GetAmplitude();
			moduleData["frequency"] = m->GetFrequency();
			Vector3 axis = m->GetAxis();
			moduleData["axis"] = {{"x", axis.x}, {"y", axis.y}, {"z", axis.z}};
			moduleData["randomPhase"] = m->GetRandomPhase();
		}
		else if (auto* m = dynamic_cast<const SpiralModule*>(module))
		{
			moduleData["radius"] = m->GetRadius();
			moduleData["speed"] = m->GetSpeed();
			moduleData["lift"] = m->GetLift();
			moduleData["randomPhase"] = m->GetRandomPhase();
			moduleData["expandRadius"] = m->GetExpandRadius();
			moduleData["expansionRate"] = m->GetExpansionRate();
		}
		else if (auto* m = dynamic_cast<const TwistModule*>(module))
		{
			moduleData["twistSpeed"] = m->GetTwistSpeed();
			moduleData["twistStrength"] = m->GetTwistStrength();
			moduleData["heightAxis"] = m->GetHeightAxis();
		}

		data["modules"].push_back(moduleData);
	}

	// レンダラー
	auto* renderer = emitter.GetRenderer();
	if (renderer)
	{
		std::string typeStr;
		switch (renderer->GetType())
		{
		case RendererType::Sprite: typeStr = "Sprite"; break;
		case RendererType::Ribbon: typeStr = "Ribbon"; break;
		case RendererType::Mesh: typeStr = "Mesh"; break;
		default: typeStr = "Sprite"; break;
		}

		data["renderer"] = {
			{"type", typeStr},
			{"texture", renderer->GetTexturePath()},
			{"blendMode", static_cast<int>(renderer->GetBlendMode())}
		};
		const EmissiveSettings& emissive = renderer->GetEmissiveSettings();
		const char* source = "Uniform";
		if (emissive.source == EmissiveSource::BaseTextureMask) source = "BaseTextureMask";
		else if (emissive.source == EmissiveSource::EmissiveTexture) source = "EmissiveTexture";
		data["renderer"]["emissive"] = {
			{"enabled", emissive.enabled},
			{"source", source},
			{"color", {{"r", emissive.color.x}, {"g", emissive.color.y}, {"b", emissive.color.z}}},
			{"intensity", emissive.intensity},
			{"bloomContribution", emissive.bloomContribution}
		};
		data["renderer"]["emissive"]["texture"] = renderer->GetEmissiveTexturePath();

		// Trail(Ribbon)固有の設定
		if (auto* trailRenderer = dynamic_cast<const TrailRenderer*>(renderer))
		{
			data["renderer"]["billboard"] = trailRenderer->GetBillboard();
			data["renderer"]["trailWidth"] = trailRenderer->GetTrailWidth();
			data["renderer"]["trailLifetime"] = trailRenderer->GetTrailLifetime();
			data["renderer"]["widthFade"] = trailRenderer->GetWidthFade();
			data["renderer"]["alphaFade"] = trailRenderer->GetAlphaFade();
			data["renderer"]["recordInterval"] = trailRenderer->GetRecordInterval();
			data["renderer"]["minSegmentDistance"] = trailRenderer->GetMinSegmentDistance();
			data["renderer"]["textureMode"] = static_cast<int>(trailRenderer->GetTextureMode());
			data["renderer"]["tileScale"] = trailRenderer->GetTileScale();
		}
		// Mesh固有の設定
		else if (auto* meshRenderer = dynamic_cast<const MeshRenderer*>(renderer))
		{
			data["renderer"]["primitiveType"] = static_cast<int>(meshRenderer->GetPrimitiveType());
			data["renderer"]["billboard"] = meshRenderer->GetBillboard();
			data["renderer"]["scale"] = meshRenderer->GetScale();
			PrimitiveOptions opts = meshRenderer->GetOptions();
			data["renderer"]["primitiveOptions"] = {
				{"segments", opts.segments},
				{"rings", opts.rings},
				{"innerRadius", opts.innerRadius},
				{"outerRadius", opts.outerRadius},
				{"tubeRadius", opts.tubeRadius},
				{"turns", opts.turns},
				{"points", opts.points},
				{"withCaps", opts.withCaps},
				{"doubleSided", opts.doubleSided},
				{"cubeSize", {{"x", opts.cubeSize.x}, {"y", opts.cubeSize.y}, {"z", opts.cubeSize.z}}},
				{"cubeFaceVisible", {opts.cubeFaceVisible[0], opts.cubeFaceVisible[1], opts.cubeFaceVisible[2],
									 opts.cubeFaceVisible[3], opts.cubeFaceVisible[4], opts.cubeFaceVisible[5]}}
			};
		}

		if (renderer)
		{
			Vector4 tint = renderer->GetTintColor();
			data["renderer"]["tintColor"] = {{"r", tint.x}, {"g", tint.y}, {"b", tint.z}, {"a", tint.w}};
		}
	}
}
#endif
} // namespace KCE
