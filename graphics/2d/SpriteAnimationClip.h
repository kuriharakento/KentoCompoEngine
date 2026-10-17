#pragma once
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace KCE
{
// キーを足すときの表示時間の初期値（秒）
inline constexpr float kSpriteAnimationDefaultKeyDuration = 0.1f;
// 表示時間の下限（秒）。0以下だと見えないコマや総秒数0のループができるので避ける
inline constexpr float kSpriteAnimationMinKeyDuration = 0.01f;

/**
 * @brief アニメーションの1コマ（キー）
 * @details JSON のキー名はメンバ変数名と同じ（NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT が自動で作る）。
 *          duration が無い古い JSON は初期値で読む。
 */
struct SpriteAnimationKey
{
	// このコマに切り替わる時刻（秒）
	float time = 0.0f;
	// 表示するテクスチャのパス。TextureManager で読み込めるもの
	std::string texturePath;
	// このコマを出しておく時間（秒）。次のキーがある間は次のキーの時刻が優先され、
	// 最後のキーだけがこの値でループの長さを決める
	float duration = kSpriteAnimationDefaultKeyDuration;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SpriteAnimationKey, time, texturePath, duration)

/**
 * @brief スプライトを画像ごとの時刻で切り替えるアニメーションのデータ
 *
 * @details 画像1〜最後の画像までを1ループとする。ループ長は「最後のキーの時刻 + その表示時間」なので、
 *          キーを動かしたり表示時間を変えたりすると総秒数も変わる。
 *          キー同士の空白では、直前のキーの画像を出したままにする。
 *          描画物を持たない純粋なデータで、FindKeyIndex は時刻だけで結果が決まる。
 */
class SpriteAnimationClip
{
public:
	/**
	 * @brief キーを追加する（時刻順を保つ）
	 * @param time 切り替わる時刻（秒）。負なら0にする
	 * @param texturePath テクスチャのパス
	 * @param duration 表示時間（秒）。下限未満は下限にする
	 * @return 追加後のインデックス。同時刻のキーがあれば、その後ろに入る
	 */
	size_t AddKey(float time, const std::string& texturePath, float duration = kSpriteAnimationDefaultKeyDuration);

	/**
	 * @brief キーを削除する
	 * @param index 削除するキー。範囲外なら何もしない
	 */
	void RemoveKey(size_t index);

	/**
	 * @brief キーの時刻を変える（時刻順を保つ）
	 * @param index 対象のキー。範囲外なら何もしない
	 * @param newTime 新しい時刻（秒）。負なら0にする
	 * @return 移動後のインデックス。範囲外なら index をそのまま返す
	 */
	size_t MoveKey(size_t index, float newTime);

	/**
	 * @brief キーの画像を差し替える
	 * @param index 対象のキー。範囲外なら何もしない
	 * @param texturePath 新しいテクスチャのパス
	 */
	void SetKeyTexture(size_t index, const std::string& texturePath);

	/**
	 * @brief キーの表示時間を変える
	 * @details 最後のキーなら、その分だけ総秒数が変わる。
	 *          途中のキーなら、次のキーが「このキーの時刻 + 表示時間」に来るよう、
	 *          後ろのキーをまとめてずらす（後ろの間隔はそのまま）。
	 * @param index 対象のキー。範囲外なら何もしない
	 * @param duration 新しい表示時間（秒）。下限未満は下限にする
	 */
	void SetKeyDuration(size_t index, float duration);

	/**
	 * @brief キーが画面に出ている時間を返す
	 * @param index 対象のキー。範囲外なら0
	 * @return 次のキーまでの時間。最後のキーならその表示時間
	 */
	float GetKeyDisplayDuration(size_t index) const;

	/**
	 * @brief 全キーを消す
	 */
	void Clear() { keys_.clear(); }

	/**
	 * @brief 再生時刻から、その時に出すキーを探す（純関数）
	 * @details ループ時は時刻をループ長で折り返す。非ループ時は末尾で止まる。
	 * @param time 再生時刻（秒）
	 * @return キーのインデックス。キーが無ければ -1
	 */
	int FindKeyIndex(float time) const;

	/**
	 * @brief 再生時刻をループ長に収める
	 * @param time 再生時刻（秒）
	 * @return ループなら折り返した時刻、非ループなら 0〜総秒数に丸めた時刻
	 */
	float WrapTime(float time) const;

	/**
	 * @brief JSON に保存する
	 * @return JSON オブジェクト
	 */
	nlohmann::json Serialize() const;

	/**
	 * @brief JSON から復元する
	 * @details 壊れたデータでも落とさず偽を返す。失敗時は自身を変更しない。
	 *          キーの並びは時刻順に直す。
	 * @param json 入力 JSON
	 * @return 復元できたら真
	 */
	bool Deserialize(const nlohmann::json& json);

	/**
	 * @brief JSON ファイルから読み込む
	 * @param path ファイルのパス
	 * @return 読めたら真。失敗時は自身を変更しない
	 */
	bool LoadFromFile(const std::string& path);

	/**
	 * @brief JSON ファイルに保存する。フォルダが無ければ作る
	 * @param path ファイルのパス
	 * @return 書けたら真
	 */
	bool SaveToFile(const std::string& path) const;

	/**
	 * @brief 保存名から JSON ファイルのパスを作る
	 * @param name 拡張子なしの名前
	 * @return アプリのリソース/json/spriteAnimations/<name>.json
	 */
	static std::string BuildFilePath(const std::string& name);

	/**
	 * @brief 総秒数（= 最後のキーの時刻 + その表示時間）
	 * @return 総秒数。キーが無ければ0
	 */
	float GetDuration() const { return keys_.empty() ? 0.0f : keys_.back().time + keys_.back().duration; }

	const std::vector<SpriteAnimationKey>& GetKeys() const { return keys_; }
	bool IsLooping() const { return isLooping_; }
	void SetLooping(bool isLooping) { isLooping_ = isLooping; }

private:
	// 時刻の昇順に保たれたキー
	std::vector<SpriteAnimationKey> keys_;
	// ループするか
	bool isLooping_ = true;
};
} // namespace KCE
