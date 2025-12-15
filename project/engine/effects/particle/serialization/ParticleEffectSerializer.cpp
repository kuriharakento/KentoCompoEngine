#include "ParticleEffectSerializer.h"
#include "effects/particle/ParticleEffect.h"
#include "effects/particle/ParticleEmitter.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/renderer/TrailRenderer.h"
#include "effects/particle/renderer/MeshRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/spawn/SpawnShapeModules.h"
#include "effects/particle/module/update/UpdateModules.h"
#include "effects/particle/module/update/BehaviorModules.h"
#include "effects/particle/module/update/AdvancedModules.h"
#include "effects/particle/module/update/ForceFieldModules.h"
#include "effects/particle/module/update/RibbonModules.h"
#include "effects/particle/module/update/TextureSheetModule.h"
#include <fstream>

// nlohmann/json を有効化
#define USE_NLOHMANN_JSON
#include "externals/nlohmann/json.hpp"
using json = nlohmann::json;

// 前方宣言（プライベートヘルパー関数）
static std::unique_ptr<ParticleEmitter> LoadEmitter(const json& data);
static void SaveEmitter(const ParticleEmitter& emitter, json& data);

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

		// エミッター
		if (data.contains("emitters") && data["emitters"].is_array())
		{
			for (const auto& emitterData : data["emitters"])
			{
				auto emitter = LoadEmitter(emitterData);
				if (emitter)
				{
					effect->AddEmitter(std::move(emitter));
				}
			}
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

	data["name"] = effect.GetName();

	// 位置
	data["position"] = {
		{"x", effect.GetPosition().x},
		{"y", effect.GetPosition().y},
		{"z", effect.GetPosition().z}
	};

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
	auto emitter = std::make_unique<ParticleEmitter>();
	emitter->Initialize(data.value("name", "UnnamedEmitter"));

	// 基本設定
	emitter->SetMaxParticles(data.value("maxParticles", 1000u));

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

	// 移動時のみ生成
	emitter->SetSpawnOnlyWhenMoving(data.value("spawnOnlyWhenMoving", false));
	emitter->SetMinMoveDistance(data.value("minMoveDistance", 0.05f));

	// モジュール
	if (data.contains("modules") && data["modules"].is_array())
	{
		for (const auto& moduleData : data["modules"])
		{
			std::string type = moduleData.value("type", "");

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
				emitter->AddModule(std::move(m));
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
			else if (type == "Gravity")
			{
				Vector3 g = { 0, -9.8f, 0 };
				if (moduleData.contains("gravity"))
				{
					g.x = moduleData["gravity"].value("x", 0.0f);
					g.y = moduleData["gravity"].value("y", -9.8f);
					g.z = moduleData["gravity"].value("z", 0.0f);
				}
				emitter->AddModule(std::make_unique<GravityModule>(g));
			}
			else if (type == "Drag")
			{
				auto m = std::make_unique<DragModule>();
				m->SetDrag(moduleData.value("drag", 0.1f));
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
				m->SetRotationSpeed(moduleData.value("rotationSpeed", 180.0f));
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
				float minAngle = moduleData.value("minAngle", 0.0f);
				float maxAngle = moduleData.value("maxAngle", 360.0f);
				m->SetRotationRange(minAngle, maxAngle);
				emitter->AddModule(std::move(m));
			}
			else if (type == "RibbonInterpolation")
			{
				auto m = std::make_unique<RibbonInterpolationModule>();
				m->SetMaxDistance(moduleData.value("maxDistance", 0.1f));
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
				options.doubleSided = opts.value("doubleSided", false);
			}
			renderer->SetPrimitive(static_cast<PrimitiveType>(rendererData.value("primitiveType", 0)), options);
			renderer->SetBillboard(rendererData.value("billboard", false));
			renderer->SetScale(rendererData.value("scale", 1.0f));
			if (rendererData.contains("tintColor"))
			{
				Vector4 tint;
				tint.x = rendererData["tintColor"].value("r", 1.0f);
				tint.y = rendererData["tintColor"].value("g", 1.0f);
				tint.z = rendererData["tintColor"].value("b", 1.0f);
				tint.w = rendererData["tintColor"].value("a", 1.0f);
				renderer->SetTintColor(tint);
			}
			emitter->SetRenderer(std::move(renderer));
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

	// 移動時のみ生成
	data["spawnOnlyWhenMoving"] = emitter.GetSpawnOnlyWhenMoving();
	data["minMoveDistance"] = emitter.GetMinMoveDistance();

	// モジュール
	data["modules"] = json::array();
	for (size_t i = 0; i < emitter.GetModuleCount(); ++i)
	{
		const auto* module = emitter.GetModule(i);
		if (!module) continue;

		json moduleData;
		std::string type = module->GetName();
		moduleData["type"] = type;

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
		else if (auto* m = dynamic_cast<const GravityModule*>(module))
		{
			Vector3 g = m->GetGravity();
			moduleData["gravity"] = {{"x", g.x}, {"y", g.y}, {"z", g.z}};
		}
		else if (auto* m = dynamic_cast<const DragModule*>(module))
		{
			moduleData["drag"] = m->GetDrag();
		}
		else if (auto* m = dynamic_cast<const ColorFadeModule*>(module))
		{
			moduleData["useInitialColor"] = m->GetUseInitialColor();
			Vector4 start = m->GetStartColor();
			Vector4 end = m->GetEndColor();
			moduleData["start"] = {{"r", start.x}, {"g", start.y}, {"b", start.z}, {"a", start.w}};
			moduleData["end"] = {{"r", end.x}, {"g", end.y}, {"b", end.z}, {"a", end.w}};
		}
		else if (auto* m = dynamic_cast<const ScaleOverLifetimeModule*>(module))
		{
			Vector3 start = m->GetStartScale();
			Vector3 end = m->GetEndScale();
			moduleData["start"] = {{"x", start.x}, {"y", start.y}, {"z", start.z}};
			moduleData["end"] = {{"x", end.x}, {"y", end.y}, {"z", end.z}};
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
			moduleData["rotationSpeed"] = m->GetRotationSpeed();
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
			moduleData["minAngle"] = m->GetMinAngle();
			moduleData["maxAngle"] = m->GetMaxAngle();
		}
		else if (auto* m = dynamic_cast<const RibbonInterpolationModule*>(module))
		{
			moduleData["maxDistance"] = m->GetMaxDistance();
		}
		else if (auto* m = dynamic_cast<const TextureSheetModule*>(module))
		{
			moduleData["columns"] = m->GetColumns();
			moduleData["rows"] = m->GetRows();
			moduleData["frameRate"] = m->GetFrameRate();
			moduleData["playMode"] = static_cast<int>(m->GetPlayMode());
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
				{"doubleSided", opts.doubleSided}
			};
			Vector4 tint = meshRenderer->GetTintColor();
			data["renderer"]["tintColor"] = {{"r", tint.x}, {"g", tint.y}, {"b", tint.z}, {"a", tint.w}};
		}
	}
}
#endif
