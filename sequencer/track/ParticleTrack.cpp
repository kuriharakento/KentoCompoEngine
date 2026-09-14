#include "sequencer/track/ParticleTrack.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

#include "base/Logger.h"
#include "effects/particle/ParticleEffect.h"
#include "effects/particle/ParticleManager.h"
#include "gameobject/base/GameObject.h"

namespace KCE
{
namespace
{
/** @brief エフェクト名・役名の入力欄の大きさ */
constexpr size_t kNameBufferSize = 128;
/** @brief 位置の入力欄のドラッグ速度 */
constexpr float kPositionDragSpeed = 0.05f;
/**
 * @brief パーティクルエディタがエフェクトを保存する場所（作業ディレクトリからの相対）
 * @details ParticleEditor::SaveEffect と同じ場所。ここに <名前>.json があれば、定義が未登録でも出せる。
 *          ParticleEffectSerializer は PathManager を通さずにこのパスをそのまま開くので、
 *          シーケンスやプレハブの application/Resources の下ではない。PathManager に揃えると保存したファイルが見つからなくなる
 */
const char* const kParticleEffectDirectory = "Resources/json/particle";
/** @brief エフェクトのファイルの拡張子 */
const char* const kParticleEffectExtension = ".json";

/** @brief エフェクト名からファイルの場所を作る */
std::filesystem::path MakeEffectFilePath(const std::string& effect)
{
	return std::filesystem::path(kParticleEffectDirectory) / (effect + kParticleEffectExtension);
}

/**
 * @brief Play() で出せるように、定義が無ければファイルから登録する
 * @details CreateEmpty() で作っただけのエフェクトは定義が無く、Play() では出ない。
 *          ファイルを見に行くのは Play() が失敗したときだけ（キューを出す瞬間にしか呼ばない）
 * @return 定義が登録済み、または今登録できたら真
 */
bool EnsureEffectDefinition(ParticleManager& particles, const std::string& effect)
{
	if (particles.HasEffectDefinition(effect))
	{
		return true;
	}
	std::error_code error;
	const std::filesystem::path file = MakeEffectFilePath(effect);
	if (!std::filesystem::exists(file, error))
	{
		return false;
	}
	particles.LoadEffectDefinition(effect, file.string());
	return true;
}

#ifdef USE_IMGUI
// パーティクルエディタが保存したエフェクト名の一覧。一覧を開いたときだけ読み直す（毎フレームのファイル読みを避ける）
std::vector<std::string> s_effectFiles;
bool s_effectFilesLoaded = false;

/** @brief 保存済みのエフェクトの一覧を読み直す */
void RefreshEffectFiles()
{
	s_effectFiles.clear();
	std::error_code error;
	for (const auto& entry : std::filesystem::directory_iterator(kParticleEffectDirectory, error))
	{
		if (entry.is_regular_file(error) && entry.path().extension() == kParticleEffectExtension)
		{
			s_effectFiles.push_back(entry.path().stem().string());
		}
	}
	std::sort(s_effectFiles.begin(), s_effectFiles.end());
	s_effectFilesLoaded = true;
}

/** @brief 保存済みのエフェクトの一覧にあるか */
bool IsSavedEffect(const std::string& effect)
{
	return std::find(s_effectFiles.begin(), s_effectFiles.end(), effect) != s_effectFiles.end();
}
#endif

/** @brief キュー1つを JSON にする（時刻は入れない。コピーでも使うため） */
nlohmann::json CueToJson(const ParticleCue& cue)
{
	nlohmann::json json;
	json["effect"] = cue.effect;
	json["position"] = { cue.position.x, cue.position.y, cue.position.z };
	json["role"] = cue.role;
	json["fireOnSkip"] = cue.fireOnSkip;
	return json;
}

/** @brief JSON からキュー1つを読む。読めない項目は既定値のまま */
void CueFromJson(const nlohmann::json& json, ParticleCue& cue)
{
	if (json.contains("effect") && json["effect"].is_string()) { cue.effect = json["effect"].get<std::string>(); }
	if (json.contains("position") && json["position"].is_array() && json["position"].size() == 3)
	{
		const auto& p = json["position"];
		if (p[0].is_number() && p[1].is_number() && p[2].is_number())
		{
			cue.position = { p[0].get<float>(), p[1].get<float>(), p[2].get<float>() };
		}
	}
	if (json.contains("role") && json["role"].is_string()) { cue.role = json["role"].get<std::string>(); }
	if (json.contains("fireOnSkip") && json["fireOnSkip"].is_boolean()) { cue.fireOnSkip = json["fireOnSkip"].get<bool>(); }
}
} // namespace

ParticleTrack::ParticleTrack()
{
	SetName("Particle Track");
	// 役はキューごとに持つので、トラックとしての役は使わない
	SetBindingRole("");
}

float ParticleTrack::GetEndTime() const
{
	return cues_.empty() ? 0.0f : cues_.back().time;
}

size_t ParticleTrack::CueChannel::Insert(const ParticleCue& cue)
{
	const auto it = std::upper_bound(cues_->begin(), cues_->end(), cue.time,
		[](float time, const ParticleCue& c) { return time < c.time; });
	const size_t index = static_cast<size_t>(it - cues_->begin());
	cues_->insert(it, cue);
	return index;
}

size_t ParticleTrack::CueChannel::MoveKey(size_t index, float newTime)
{
	if (index >= cues_->size())
	{
		return index;
	}
	ParticleCue cue = (*cues_)[index];
	cue.time = newTime;
	cues_->erase(cues_->begin() + index);
	return Insert(cue);
}

void ParticleTrack::CueChannel::RemoveKey(size_t index)
{
	if (index < cues_->size())
	{
		cues_->erase(cues_->begin() + index);
	}
}

nlohmann::json ParticleTrack::CueChannel::CopyKey(size_t index) const
{
	return index < cues_->size() ? CueToJson((*cues_)[index]) : nlohmann::json(nullptr);
}

bool ParticleTrack::CueChannel::PasteKey(float time, const nlohmann::json& json)
{
	if (!json.is_object())
	{
		return false;
	}
	ParticleCue cue;
	cue.time = time;
	CueFromJson(json, cue);
	Insert(cue);
	return true;
}

bool ParticleTrack::CueChannel::AddKeyAt(float time)
{
	// 直前のキューの中身を引き継ぐ。同じエフェクトを何回も置くことが多いため
	ParticleCue cue;
	const auto previous = std::upper_bound(cues_->begin(), cues_->end(), time,
		[](float t, const ParticleCue& c) { return t < c.time; });
	if (previous != cues_->begin())
	{
		cue = *(previous - 1);
	}
	cue.time = time;
	Insert(cue);
	return true;
}

#ifdef USE_IMGUI
bool ParticleTrack::CueChannel::DrawKeyValueEditor(size_t index)
{
	ParticleCue& cue = (*cues_)[index];
	bool changed = false;

	// 出せるエフェクトから選ぶ。打ち間違いや、定義の無いエフェクトを選んで何も出ない、を防ぐ
	ParticleManager* particles = ParticleManager::GetInstance();
	if (!s_effectFilesLoaded)
	{
		RefreshEffectFiles();
	}
	const bool playable = !cue.effect.empty() && (particles->HasEffectDefinition(cue.effect) || IsSavedEffect(cue.effect));
	if (ImGui::BeginCombo("エフェクト", cue.effect.empty() ? "(未選択)" : cue.effect.c_str()))
	{
		// 開いたときだけフォルダを読み直す。開いている間に毎フレーム読むと重い
		if (ImGui::IsWindowAppearing())
		{
			RefreshEffectFiles();
		}
		bool listedAny = false;
		const auto choose = [&cue, &changed, &listedAny](const std::string& name)
		{
			listedAny = true;
			ImGui::PushID(name.c_str());
			if (ImGui::Selectable(name.c_str(), name == cue.effect))
			{
				cue.effect = name;
				changed = true;
			}
			ImGui::PopID();
		};
		// パーティクルエディタが保存したもの
		for (const std::string& name : s_effectFiles)
		{
			choose(name);
		}
		// ゲーム側が Load() で読んだもの。定義が無い物（CreateEmpty だけの物）は出せないので並べない。
		// 同じエフェクトを何回も Play() すると同名が並ぶので、先に出てきた1つだけにする
		for (size_t i = 0; i < particles->GetEffectCount(); ++i)
		{
			const ParticleEffect* effect = particles->GetEffect(i);
			if (!effect || !particles->HasEffectDefinition(effect->GetName()) || IsSavedEffect(effect->GetName()))
			{
				continue;
			}
			bool shownBefore = false;
			for (size_t j = 0; j < i && !shownBefore; ++j)
			{
				const ParticleEffect* other = particles->GetEffect(j);
				shownBefore = other && other->GetName() == effect->GetName();
			}
			if (!shownBefore)
			{
				choose(effect->GetName());
			}
		}
		if (!listedAny)
		{
			ImGui::TextDisabled("出せるエフェクトがない。パーティクルエディタで保存してね");
		}
		ImGui::EndCombo();
	}
	if (!cue.effect.empty() && !playable)
	{
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "出せない");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("定義が登録されていなくて、Resources/json/particle にもファイルが無い");
		}
	}

	char nameBuffer[kNameBufferSize];
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", cue.effect.c_str());
	if (ImGui::InputText("エフェクト名", nameBuffer, sizeof(nameBuffer)))
	{
		cue.effect = nameBuffer;
		changed = true;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("一覧に無いときは直接打つ（ゲーム側で後から読み込むエフェクトなど）");
	}

	char roleBuffer[kNameBufferSize];
	std::snprintf(roleBuffer, sizeof(roleBuffer), "%s", cue.role.c_str());
	if (ImGui::InputText("出す場所の役", roleBuffer, sizeof(roleBuffer)))
	{
		cue.role = roleBuffer;
		changed = true;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("空なら下の位置に出す。役を入れると、出した瞬間のその GameObject の位置に出す");
	}

	ImGui::BeginDisabled(!cue.role.empty());
	if (ImGui::DragFloat3("位置", &cue.position.x, kPositionDragSpeed))
	{
		changed = true;
	}
	ImGui::EndDisabled();

	if (ImGui::Checkbox("スキップ時も出す", &cue.fireOnSkip))
	{
		changed = true;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("カットシーンを飛ばしたときにも出すか。見た目だけならオフのままでいい");
	}
	return changed;
}
#endif

bool ParticleTrack::RecordKey(float time, const BindingContext& ctx)
{
	(void)ctx;
	return channel_.AddKeyAt(time);
}

void ParticleTrack::FireInRange(float from, float to, bool skipping, const BindingContext& ctx) const
{
	ParticleManager* particles = ParticleManager::GetInstance();
	for (const ParticleCue& cue : cues_)
	{
		if (cue.time <= from)
		{
			continue;
		}
		if (cue.time > to)
		{
			// 時刻順に並んでいるので、以降はすべて範囲外
			break;
		}
		if ((skipping && !cue.fireOnSkip) || cue.effect.empty())
		{
			continue;
		}

		// 役があればその GameObject の今の位置。居なければ出さない（違う場所に出すよりまし）
		Vector3 position = ctx.ApplyOriginToPoint(cue.position);
		if (!cue.role.empty())
		{
			const GameObject* object = ctx.GetGameObject(cue.role);
			if (!object)
			{
				continue;
			}
			position = object->GetPosition();
		}

		// 定義が無くても、パーティクルエディタが保存したファイルがあれば登録してからもう一度だけ試す
		ParticleEffect* played = particles->Play(cue.effect, position);
		if (!played && EnsureEffectDefinition(*particles, cue.effect))
		{
			played = particles->Play(cue.effect, position);
		}
		if (!played)
		{
			Logger::Log("ParticleTrack: 出せないエフェクトです（定義もファイルも無い、または読み込めない）: " + cue.effect + "\n", Logger::LogLevel::Warning);
		}
	}
}

nlohmann::json ParticleTrack::Serialize() const
{
	nlohmann::json json;
	SerializeCommon(json);

	nlohmann::json cuesJson = nlohmann::json::array();
	for (const ParticleCue& cue : cues_)
	{
		nlohmann::json cueJson = CueToJson(cue);
		cueJson["time"] = cue.time;
		cuesJson.push_back(cueJson);
	}
	json["cues"] = cuesJson;
	return json;
}

bool ParticleTrack::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object())
	{
		return false;
	}

	DeserializeCommon(json);
	cues_.clear();

	if (json.contains("cues") && json["cues"].is_array())
	{
		for (const auto& cueJson : json["cues"])
		{
			if (!cueJson.is_object() || !cueJson.contains("time") || !cueJson["time"].is_number())
			{
				continue;
			}
			ParticleCue cue;
			cue.time = cueJson["time"].get<float>();
			CueFromJson(cueJson, cue);
			channel_.Insert(cue);
		}
	}
	return true;
}
} // namespace KCE
