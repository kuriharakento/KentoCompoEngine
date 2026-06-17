#pragma once
/**
 * @file ParticleEffect.h
 * @brief パーティクルエフェクト
 * 
 * 複数のエミッターを1つのエフェクトとしてグループ化。
 * JSONファイルからの読み込み・保存に対応。
 */
#include <memory>
#include <vector>
#include <string>
#include "math/Vector3.h"
#include "time/Timer.h"

class ParticleEmitter;
class CameraManager;
class DirectXCommon;
class SrvManager;
struct Transform;

/**
 * @brief パーティクルエフェクト
 * 
 * 複数のエミッターを1つのエフェクトとしてグループ化し、一括管理する。
 * エディタで作成したJSONファイルから読み込み可能。
 */
class ParticleEffect
{
public:
	ParticleEffect();
	~ParticleEffect();

	// ムーブのみ許可（コピー禁止）
	ParticleEffect(ParticleEffect&&) noexcept;
	ParticleEffect& operator=(ParticleEffect&&) noexcept;
	ParticleEffect(const ParticleEffect&) = delete;
	ParticleEffect& operator=(const ParticleEffect&) = delete;

	/**
	 * @brief JSONファイルからエフェクトを読み込み
	 * @param jsonPath JSONファイルパス
	 * @return 読み込んだエフェクト
	 */
	static std::unique_ptr<ParticleEffect> LoadFromFile(const std::string& jsonPath);

	/**
	 * @brief 初期化
	 * @param name エフェクト名
	 */
	void Initialize(const std::string& name);

	/**
	 * @brief 更新
	 */
	void Update(float deltaTime, CameraManager* camera);

	/**
	 * @brief 描画
	 */
	void Draw(DirectXCommon* dxCommon, SrvManager* srvManager);

	//===== エミッター管理 =====//
	
	/**
	 * @brief エミッターを追加
	 */
	void AddEmitter(std::unique_ptr<ParticleEmitter> emitter);

	/**
	 * @brief エミッターを削除
	 */
	void RemoveEmitter(size_t index);

	/**
	 * @brief 名前でエミッターを取得
	 */
	ParticleEmitter* GetEmitter(const std::string& name);

	/**
	 * @brief インデックスでエミッターを取得
	 */
	ParticleEmitter* GetEmitter(size_t index);
	const ParticleEmitter* GetEmitter(size_t index) const;

	/**
	 * @brief エミッター数を取得
	 */
	size_t GetEmitterCount() const { return emitters_.size(); }

	//===== 一括制御 =====//

	/**
	 * @brief 全エミッターの位置を設定
	 */
	void SetPosition(const Vector3& position);

	/**
	 * @brief 全エミッターの追従ターゲットを設定
	 */
	void SetFollowTarget(Transform* target);

	/**
	 * @brief エフェクトを再生
	 */
	void Play();

	/**
	 * @brief エフェクトを停止
	 */
	void Stop();

	/**
	 * @brief エフェクトをリセット
	 */
	void Reset();

	/**
	 * @brief 再生中か
	 */
	bool IsPlaying() const { return isPlaying_; }

	/**
	 * @brief 終了したか（全エミッターのパーティクルが消滅）
	 */
	bool IsFinished() const;

	/**
	 * @brief 自動削除フラグを設定
	 */
	void SetAutoRemove(bool autoRemove) { isAutoRemove_ = autoRemove; }
	bool IsAutoRemove() const { return isAutoRemove_; }

	/**
	 * @brief タイムスケールを無視して実時間を使うか設定
	 */
	void SetDeltaTimeType(DeltaTimeType type) { deltaTimeType_ = type; }
	DeltaTimeType GetDeltaTimeType() const { return deltaTimeType_; }

	//===== プロパティ =====//

	const std::string& GetName() const { return name_; }
	void SetName(const std::string& name) { name_ = name; }
	const Vector3& GetPosition() const { return position_; }

	/**
	 * @brief JSONファイルに保存
	 */
	void SaveToFile(const std::string& jsonPath);

private:
	std::string name_;
	std::vector<std::unique_ptr<ParticleEmitter>> emitters_;
	Vector3 position_ = {};
	bool isPlaying_ = false;
	bool isAutoRemove_ = true;
	DeltaTimeType deltaTimeType_ = DeltaTimeType::DeltaTime;
};
