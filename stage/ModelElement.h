#pragma once

#include <memory>
#include <string>

#include "math/Vector3.h"
#include "stage/IStageElement.h"

namespace KCE
{
class GameObject;

/**
 * @brief ステージに置くモデル1つ。GameObject を1つ受け持つ。
 *
 * JSON の例（"models" 配列の1件）:
 *   {
 *     "name": "mirror_ball",
 *     "prefab": "mirror_ball.json",      // 最初に使ったプレハブ。表示用で、読み込みには使わない
 *     "object": { ... }                  // GameObject::SerializePrefab の形（モデル、置き場所、コンポーネントの値）
 *   }
 *
 * - GameObject の所有は GameObjectManager。外すときは Unregister するだけで、破棄は次のフレームの頭になる
 * - 再生中だけ GameObject の更新を有効にし、停止で再生前の置き場所へ戻す（根の GameObject だけ。子の動きは戻さない）
 * - 保存は常に再生前の置き場所を書く。再生中に動いた位置が保存されない
 */
class ModelElement final : public IStageElement
{
public:
	/**
	 * @param name ステージ内で一意な名前。GameObject の名前にも使う
	 * @param prefabName 最初に使ったプレハブのファイル名。無ければ空
	 * @param object GameObjectManager に登録済みの GameObject。所有はmanager
	 */
	ModelElement(std::string name, std::string prefabName, GameObject* object);
	~ModelElement() override;

	/** @brief StageManager に登録する、"models" の種類 */
	static StageElementType MakeType();
	/**
	 * @brief プレハブのファイルから要素を作る
	 * @param name 要素の名前
	 * @param prefabFile Resources/json/prefab の中のファイル名
	 * @return 作れなければ nullptr
	 */
	static std::unique_ptr<ModelElement> CreateFromPrefab(const std::string& name, const std::string& prefabFile);

	const char* GetTypeKey() const override { return "models"; }
	void SetPlaying(bool playing) override;
	bool OnRename(const std::string& newName) override;
	nlohmann::ordered_json Serialize() const override;

	/** @return ゲームから動かしたいときの窓口。所有権は持たない */
	GameObject* GetGameObject() const { return object_; }

#ifdef USE_IMGUI
	void DrawHierarchyRow() override;
	void DrawInspector(const SelectionItem& selected) override;
#endif

private:
	/** @brief 再生を始める時点の根の置き場所 */
	struct Pose
	{
		Vector3 position{};
		Vector3 rotation{};
		Vector3 scale{ 1.0f, 1.0f, 1.0f };
	};
	void CapturePose();
	void RestorePose();

	// 所有は GameObjectManager
	GameObject* object_ = nullptr;
	std::string prefabName_;
	Pose startPose_;
	// 作った時点は更新が有効（ゲームの中ではそのまま動く）。エディタが停止を指示すると止まる
	bool playing_ = true;
};
} // namespace KCE
