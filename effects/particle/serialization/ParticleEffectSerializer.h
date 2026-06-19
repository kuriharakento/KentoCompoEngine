#pragma once
/**
 * @file ParticleEffectSerializer.h
 * @brief パーティクルエフェクトシリアライザ
 * 
 * JSON形式でエフェクトの保存/読み込みを行う。
 */
#include <string>
#include <memory>

class ParticleEffect;
class ParticleEmitter;

/**
 * @brief パーティクルエフェクトシリアライザ
 * 
 * JSON形式でパーティクルエフェクトを保存/読み込みする。
 */
class ParticleEffectSerializer
{
public:
	/**
	 * @brief JSONファイルからエフェクトを読み込み
	 * @param path JSONファイルパス
	 * @return 読み込んだエフェクト
	 */
	static std::unique_ptr<ParticleEffect> Load(const std::string& path);

	/**
	 * @brief エフェクトをJSONファイルに保存
	 * @param effect 保存するエフェクト
	 * @param path JSONファイルパス
	 * @return 成功したらtrue
	 */
	static bool Save(const ParticleEffect& effect, const std::string& path);
};
