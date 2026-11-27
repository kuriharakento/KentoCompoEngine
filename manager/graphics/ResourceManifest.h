#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "manager/graphics/ResourceLifetime.h"

namespace KCE
{
class JobSystem;
class DirectXCommon;

/**
 * @brief 起動時に読み込むモデルとテクスチャの表。ファイルに保存し、次の起動で最初に読み込む。
 *
 * - 保存先は Resources/json/resources.json
 * - エンジンが必ず使う物（cube など）は、これまでどおりコードで読む。この表は、それに「足す」物だけを持つ
 * - 「プロジェクト」ウィンドウで、読み込み済みの一覧を確認し、フォルダにあるファイルを後から読み込み、表へ保存できる
 * - 表から外しても、読み込み済みの物はそのまま残る（次の起動から読まれなくなる）
 */
class ResourceManifest
{
public:
	static ResourceManifest* GetInstance();
	static bool HasInstance();

	/** @brief 表のモデル1件 */
	struct ModelEntry
	{
		//! ModelManager の名前（Resources/models の中のフォルダ名）
		std::string name;
		//! ファイル形式（".obj" など）
		std::string type = ".obj";
		ResourceLifetime lifetime = ResourceLifetime::Resident;
	};
	/** @brief 表のテクスチャ1件 */
	struct TextureEntry
	{
		//! 読み込みに使うパス（"./Resources/xxx.png"）
		std::string path;
		ResourceLifetime lifetime = ResourceLifetime::Resident;
		//! リニア（sRGB でない）で読むか。法線マップなど
		bool linear = false;
	};

	/** @brief ファイルから表を読み、エディタの窓を登録する。DebugUIManager の初期化の後に呼ぶ */
	void Initialize();
	void Finalize();

	/** @brief 表の物を読み込む。シーンの初期化より前に、エンジンの既定リソースのあとで呼ぶ */
	void LoadAll(JobSystem* jobSystem);

	/** @brief モデルを今読み込み、表に足す。読めなければ表に足さず false */
	bool AddModel(const ModelEntry& entry);
	/** @brief テクスチャを今読み込み、表に足す。読めなければ表に足さず false */
	bool AddTexture(const TextureEntry& entry);
	/** @brief 表からモデルを外す。読み込み済みの物は残る */
	void RemoveModel(const std::string& name);
	/** @brief 表からテクスチャを外す。読み込み済みの物は残る */
	void RemoveTexture(const std::string& path);

	const std::vector<ModelEntry>& GetModels() const { return models_; }
	const std::vector<TextureEntry>& GetTextures() const { return textures_; }

	/**
	 * @brief 解放を頼む。実行は ProcessPendingReleases（次のフレームの頭）。表からも外す
	 * @param isModel モデルなら真、テクスチャなら偽
	 * @param key モデルは名前、テクスチャは GetLoadedTextures の path
	 */
	void RequestRelease(bool isModel, const std::string& key);
	/**
	 * @brief 頼まれた解放を実行する。フレームの頭（描画命令を積む前）に毎フレーム呼ぶ
	 * @details GPU の完了を待ってから外すので、描いている途中の物は壊れない。何も頼まれていなければ、待たない
	 */
	void ProcessPendingReleases(DirectXCommon* dxCommon);

	/** @brief 読み込み済みのモデルとテクスチャの一覧を、ログへ出す */
	void LogLoadedResources() const;

	/** @brief 表を保存する */
	bool Save();
	/** @return 最後に保存（か読み込み）してから、表が変わっていれば真 */
	bool IsDirty() const { return dirty_; }

	~ResourceManifest() = default;

private:
	static std::unique_ptr<ResourceManifest> instance_;
	friend std::unique_ptr<ResourceManifest> std::make_unique<ResourceManifest>();

	ResourceManifest() = default;
	ResourceManifest(const ResourceManifest&) = delete;
	ResourceManifest& operator=(const ResourceManifest&) = delete;

	static std::string GetFilePath();
	bool Load();

#ifdef USE_IMGUI
	void DrawWindowImGui();
	void DrawModelsTab();
	void DrawTexturesTab();
	/** @brief Resources/models の中の、読み込めるモデル（フォルダ名と形式）を探す */
	static std::vector<ModelEntry> ScanModelFiles();
	/** @brief Resources の中の、読み込めるテクスチャ（読み込みに使うパス）を探す */
	static std::vector<std::string> ScanTextureFiles();

	/** @brief フォルダの上にあるテクスチャのファイル1つ */
	struct AssetFile
	{
		//! 読み込みに使うパス（"./Resources/textures/ui/x.png"）
		std::string path;
		//! textures からの相対（"ui/x.png"）
		std::string relative;
		//! textures からの相対のフォルダ（"ui"）。textures 直下なら空
		std::string folder;
		//! ファイル名（"x.png"）
		std::string name;
		//! 読み込み済みの登録名と比べるための、正規化したパス
		std::string key;
	};
	/** @brief フォルダツリーの1ノード */
	struct FolderNode
	{
		std::string name;
		//! textures からの相対（"ui"）。根は空
		std::string folder;
		std::vector<FolderNode> children;
	};
	/** @brief textures の中のファイルとフォルダを探し直す */
	void RescanAssets();
	/** @brief フォルダツリーを描く。選んだフォルダを selectedFolder_ に入れる */
	void DrawFolderTree(const FolderNode& node);
#endif

	std::vector<ModelEntry> models_;
	std::vector<TextureEntry> textures_;
	// 最後に保存（か読み込み）した時点の表。「次の起動で変わる物」を見つけるのに使う
	std::vector<TextureEntry> savedTextures_;
	std::vector<ModelEntry> savedModels_;
	bool dirty_ = false;
	/** @brief 解放の頼み1件 */
	struct PendingRelease
	{
		bool isModel = false;
		std::string key;
	};
	std::vector<PendingRelease> pendingReleases_;
	// 解放の結果の知らせ。解放の処理は Release でも動くので、USE_IMGUI の外に置く
	std::string statusMessage_;
	// プレビューに出しているテクスチャの登録名。空なら出さない（解放のとき先に外すので、USE_IMGUI の外に置く）
	std::string selectedTexture_;
#ifdef USE_IMGUI
	std::array<char, 128> filter_{};
	// 追加するときの寿命。0 が常駐、1 がシーン
	int addLifetime_ = 0;
	bool addLinear_ = false;
	// 起動後に一度も参照されていない物だけ並べる
	bool unusedOnly_ = false;
	// ---- フォルダ表示 ----
	std::vector<AssetFile> assetFiles_;
	FolderNode folderRoot_;
	bool assetsScanned_ = false;
	// 選んでいるフォルダ（textures からの相対。空なら textures の直下）
	std::string selectedFolder_;
	bool includeSubfolders_ = false;
	float thumbnailSize_ = 88.0f;
	// 選んでいるファイル（AssetFile::path）
	std::string selectedAsset_;
	// 選んでいるモデル（名前）
	std::string selectedModel_;
	// 解放の確認を出している対象。空なら出さない
	bool releaseConfirmIsModel_ = false;
	std::string releaseConfirmKey_;
	bool releasePopupRequested_ = false;
#endif
};
} // namespace KCE
