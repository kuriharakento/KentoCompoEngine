#include "ParticleEffectSerializer.h"
#include "effects/particle/ParticleEffect.h"
#include "effects/particle/ParticleEmitter.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/update/UpdateModules.h"
#include <fstream>

// nlohmann/json が利用可能な場合のみ有効
#ifdef USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

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
std::unique_ptr<ParticleEmitter> ParticleEffectSerializer::LoadEmitter(const json& data)
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

	// モジュール
	if (data.contains("modules") && data["modules"].is_array())
	{
		for (const auto& moduleData : data["modules"])
		{
			std::string type = moduleData.value("type", "");

			if (type == "SpawnRate")
			{
				auto m = std::make_unique<SpawnRateModule>();
				m->SetSpawnRate(moduleData.value("rate", 10.0f));
				emitter->AddModule(std::move(m));
			}
			else if (type == "SpawnBurst")
			{
				auto m = std::make_unique<SpawnBurstModule>();
				m->SetBurstCount(moduleData.value("count", 10u));
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
			else if (type == "ColorFade")
			{
				emitter->AddModule(std::make_unique<ColorFadeModule>());
			}
			// 他のモジュールも同様に追加...
		}
	}

	// レンダラー
	if (data.contains("renderer"))
	{
		const auto& rendererData = data["renderer"];
		std::string type = rendererData.value("type", "Sprite");

		if (type == "Sprite")
		{
			auto renderer = std::make_unique<SpriteRenderer>();
			renderer->Initialize(rendererData.value("texture", "Resources/images/particle.png"));
			emitter->SetRenderer(std::move(renderer));
		}
		// Ribbon, Mesh も同様に追加
	}
	else
	{
		// デフォルトレンダラー
		auto renderer = std::make_unique<SpriteRenderer>();
		renderer->Initialize("Resources/images/particle.png");
		emitter->SetRenderer(std::move(renderer));
	}

	return emitter;
}

void ParticleEffectSerializer::SaveEmitter(const ParticleEmitter& emitter, json& data)
{
	data["name"] = emitter.GetName();
	data["maxParticles"] = emitter.GetMaxParticles();
	data["position"] = {
		{"x", emitter.GetPosition().x},
		{"y", emitter.GetPosition().y},
		{"z", emitter.GetPosition().z}
	};

	// モジュール
	data["modules"] = json::array();
	// TODO: モジュールリストへのアクセス方法が必要

	// レンダラー
	auto* renderer = emitter.GetRenderer();
	if (renderer)
	{
		data["renderer"] = {
			{"type", renderer->GetType() == RendererType::Sprite ? "Sprite" : 
			         renderer->GetType() == RendererType::Ribbon ? "Ribbon" : "Mesh"}
		};
	}
}
#endif
