#pragma once
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace KCE
{
class DirectXCommon;

/**
 * @brief シェーダーを再コンパイルしてPSOを作り直すコールバック
 * @details 成功したら真、コンパイルに失敗したら偽を返す。
 *          偽を返した場合、既存のPSOはそのまま使われ続けなければならない。
 *          作りかけのPSOで差し替えてしまうと絵が消える。
 */
using ShaderReloadCallback = std::function<bool(std::string& outError)>;

/**
 * @brief ホットリロードの対象
 */
struct ShaderReloadTarget
{
	void* owner = nullptr;
	std::string name;
	ShaderReloadCallback rebuild;
	//! 直近のリロード結果
	bool lastSucceeded = true;
	//! 直近の失敗理由
	std::string lastError;
};

/**
 * @brief シェーダーのホットリロードを管理するクラス
 *
 * @details SEQUENCER_PLAN 5.2。これから描画パスを大量に足す作業で、
 *          シェーダー1行のためにアプリを再起動するのは効率が悪すぎる。
 *          DirectXCommon が IDxcCompiler3 を実行時に保持しているため、
 *          再コンパイルは実行中でも行える。
 *
 *          監視は「シェーダーディレクトリ配下の最終更新時刻」で行う。
 *          .hlsli のインクルード関係を追跡するより単純で、
 *          取りこぼしが無い。1つでも変わったら全対象を作り直す。
 */
class ShaderHotReload
{
public:
	static ShaderHotReload* GetInstance();
	static bool HasInstance();

	/**
	 * @brief 初期化する
	 * @param dxCommon PSOの再生成に使うDirectXCommon
	 */
	void Initialize(DirectXCommon* dxCommon);

	/**
	 * @brief 終了処理
	 */
	void Finalize();

	/**
	 * @brief リロード対象を登録する
	 * @param owner 登録元。解除に使う
	 * @param name エディタに表示する名前
	 * @param rebuild シェーダーを再コンパイルしてPSOを作り直すコールバック
	 */
	void Register(void* owner, const std::string& name, ShaderReloadCallback rebuild);

	/**
	 * @brief リロード対象を解除する
	 * @param owner 登録時に渡した所有者
	 */
	void Unregister(void* owner);

	/**
	 * @brief 毎フレームの更新。監視が有効なら更新を検出してリロードする
	 * @details ファイルの更新時刻を毎フレーム見るとディスクアクセスが無駄なので、
	 *          一定間隔でのみ確認する。
	 */
	void Update();

	/**
	 * @brief 全対象を今すぐリロードする
	 *
	 * @details PSOを差し替える前にGPUの完了を待つ。待たずに差し替えると、
	 *          実行中のコマンドリストが参照しているPSOを解放してしまう。
	 *
	 * @return 全て成功したら真
	 */
	bool ReloadAll();

	/**
	 * @brief 自動監視の有効・無効を設定する
	 * @param enabled 有効にするなら真
	 */
	void SetAutoReloadEnabled(bool enabled) { autoReloadEnabled_ = enabled; }
	bool IsAutoReloadEnabled() const { return autoReloadEnabled_; }

	/**
	 * @brief 監視するディレクトリを追加する
	 * @param directory シェーダーが置かれているディレクトリ
	 */
	void AddWatchDirectory(const std::filesystem::path& directory);

	const std::vector<ShaderReloadTarget>& GetTargets() const { return targets_; }

#ifdef USE_IMGUI
	/**
	 * @brief デバッグUIを登録する
	 */
	void RegisterDebugUI();

	/**
	 * @brief デバッグUIを描画する
	 */
	void DrawImGui();
#endif

public:
	~ShaderHotReload() = default;

private:
	static std::unique_ptr<ShaderHotReload> instance_;
	friend std::unique_ptr<ShaderHotReload> std::make_unique<ShaderHotReload>();

	ShaderHotReload() = default;
	ShaderHotReload(const ShaderHotReload&) = delete;
	ShaderHotReload& operator=(const ShaderHotReload&) = delete;

	/**
	 * @brief 監視対象の中で最も新しい更新時刻を求める
	 * @return 最終更新時刻
	 */
	std::filesystem::file_time_type GetLatestWriteTime() const;

	DirectXCommon* dxCommon_ = nullptr;
	std::vector<ShaderReloadTarget> targets_;
	std::vector<std::filesystem::path> watchDirectories_;

	// 前回確認したときの最終更新時刻
	std::filesystem::file_time_type lastWriteTime_{};
	// 更新時刻を取得済みかどうか
	bool hasWriteTime_ = false;
	// 自動監視が有効かどうか
	bool autoReloadEnabled_ = true;
	// 前回確認してからの経過時間（秒）
	float checkTimer_ = 0.0f;
	// 直近のリロードの概要
	std::string statusMessage_;
};
} // namespace KCE
