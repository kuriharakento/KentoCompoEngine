#pragma once
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "sequencer/core/BindingContext.h"
#include "sequencer/core/CurveChannel.h"

namespace KCE
{
/**
 * @brief トラックの種別
 * @details JSONには文字列で保存する。列挙値の順序変更でデータが壊れないようにするため。
 */
enum class TrackType
{
	Camera,		 //!< カメラワーク
	Transform,	 //!< 位置・回転・スケール
	Light,		 //!< ライトのパラメータ
	PostProcess, //!< ポストプロセスのパラメータ
	Event,		 //!< ゲーム側へのコールバック
	Screen,		 //!< ステージの画面（モニター）
	Text,		 //!< 歌詞テロップ・会話
	Text3D,		 //!< 3D 空間の文字
	Component,	 //!< GameObject コンポーネントの項目
	Particle,	 //!< パーティクルのエフェクトを出す
	Unknown,	 //!< 未知（前方互換のためのプレースホルダ）
};

/**
 * @brief シーケンスの1トラックが満たすべきインターフェース
 *
 * @section contract Evaluate(t) の純関数契約
 *
 * SEQUENCER_PLAN 3.1。全てのトラックは以下を厳守すること。
 *
 * - Evaluate(t) は、**それまでにどの時刻を評価したかに依存してはならない。**
 *   同じ t と同じ ctx を渡せば、常に同じ結果になること。
 * - 「毎フレーム加算して進める」実装は**禁止**。
 *   `position += velocity * dt` のような書き方をした時点で契約は破れる。
 * - 時間差分（deltaTime）を引数に取らないのは意図的なもの。
 *
 * この契約を守ると、以下が実装なしで同時に成立する。
 *
 * - エディタのスクラブ（タイムラインを掴んで前後に動かす）
 * - カットシーンのスキップ（t = end を一発適用するだけで最終状態になる）
 * - 早送り・巻き戻し・途中再生
 *
 * 逆に副作用の積み上げで実装すると、**スキップが原理的に実装できなくなる。**
 *
 * パーティクルのように本質的に状態を持つものだけが例外で、
 * その場合も「クリップ先頭から上限付きで早送りシミュレート」＋「乱数シード固定」で
 * 近似し、完全な逆再生は仕様として諦める。
 */
class ITrack
{
public:
	virtual ~ITrack() = default;

	/**
	 * @brief トラック種別を返す
	 * @return 種別
	 */
	virtual TrackType GetType() const = 0;

	/**
	 * @brief JSONに保存する種別名を返す
	 * @return 種別名（例: "Camera"）
	 */
	virtual const char* GetTypeName() const = 0;

	/**
	 * @brief 指定時刻の状態を対象に適用する（純関数）
	 *
	 * @details 上記の契約を厳守すること。ctx から引いた実体が nullptr の場合は
	 *          何もせずに返る（エディタでは対象が未割り当てのことが日常的にある）。
	 *
	 * @param time シーケンス先頭からの時刻（秒）
	 * @param ctx 役に実体を割り当てたコンテキスト
	 */
	virtual void Evaluate(float time, const BindingContext& ctx) = 0;

	/**
	 * @brief このトラックが値を持つ最後の時刻
	 * @return 終了時刻（秒）。キーが無ければ0
	 */
	virtual float GetEndTime() const = 0;

	/**
	 * @brief 対象の現在の状態を退避する
	 * @details SEQUENCER_PLAN 3.5。シーケンス開始時とエディタのスクラブ開始時に呼び、
	 *          演出が対象を書き換える前の状態を保存する。
	 * @param ctx バインディングコンテキスト
	 */
	virtual void CaptureState(const BindingContext& ctx) = 0;

	/**
	 * @brief 退避した状態を対象に書き戻す
	 * @details CaptureState() を呼んでいない場合は何もしない。
	 * @param ctx バインディングコンテキスト
	 */
	virtual void RestoreState(const BindingContext& ctx) = 0;

	/**
	 * @brief トラックをJSONにシリアライズする
	 * @return JSONオブジェクト
	 */
	virtual nlohmann::json Serialize() const = 0;

	/**
	 * @brief JSONからトラックを復元する
	 * @details 壊れたJSONでも assert で落とさず、偽を返して ConsoleLog に出すこと。
	 *          エディタでは壊れたデータを開くことが日常的に起きる。
	 * @param json 入力JSON
	 * @return 復元に成功したら真
	 */
	virtual bool Deserialize(const nlohmann::json& json) = 0;

	// --- カーブの共通窓口（エディタ用） ---

	/**
	 * @brief このトラックが持つカーブの本数
	 * @details エディタはトラックの種類を問わず、この窓口だけでキーを編集する。
	 * @return チャンネル数
	 */
	virtual size_t GetChannelCount() const { return 0; }

	/**
	 * @brief カーブを1本取得する
	 * @param index チャンネル番号
	 * @return チャンネル。範囲外なら nullptr
	 */
	virtual ICurveChannel* GetChannel(size_t index) { (void)index; return nullptr; }

	/**
	 * @brief 対象の現在の状態を、指定時刻のキーとして全チャンネルに打つ
	 * @details エディタの「キーを打つ」操作の実体。対象が見つからなければ何もしない。
	 * @param time キーを打つ時刻（秒）
	 * @param ctx バインディングコンテキスト
	 * @return キーを打てたら真
	 */
	virtual bool RecordKey(float time, const BindingContext& ctx) { (void)time; (void)ctx; return false; }

	/**
	 * @brief このトラックが値を持つ最後の時刻（全チャンネルの最大）
	 * @return 終了時刻（秒）
	 */
	float GetChannelsEndTime();

	/**
	 * @brief 指定時刻付近のキーを全チャンネルから削除する
	 * @return 1つでも削除したら真
	 */
	bool RemoveKeysAt(float time, float tolerance);

#ifdef USE_IMGUI
	/**
	 * @brief インスペクタにこのトラックのプロパティを描画する
	 * @details 編集はすべて CommandHistory 経由で行うこと。
	 * @return プロパティが変わったら真（呼び出し側がコマンドとして履歴に積む）
	 */
	virtual bool DrawInspector() { return false; }
#endif

	// --- 共通プロパティ ---

	const std::string& GetName() const { return name_; }
	void SetName(const std::string& name) { name_ = name; }

	/**
	 * @brief このトラックが対象とする役の名前
	 * @return 役の名前
	 */
	const std::string& GetBindingRole() const { return bindingRole_; }
	void SetBindingRole(const std::string& role) { bindingRole_ = role; }

	/**
	 * @brief ミュート中かどうか
	 * @details ミュート中のトラックは Sequence::Evaluate() から呼ばれない。
	 * @return ミュート中なら真
	 */
	bool IsMuted() const { return muted_; }
	void SetMuted(bool muted) { muted_ = muted; }

protected:
	/**
	 * @brief 共通プロパティをJSONに書き出す
	 * @param json 出力先
	 */
	void SerializeCommon(nlohmann::json& json) const;

	/**
	 * @brief 共通プロパティをJSONから読み込む
	 * @param json 入力
	 */
	void DeserializeCommon(const nlohmann::json& json);

private:
	// エディタに表示するトラック名
	std::string name_ = "Track";
	// 対象とする役の名前
	std::string bindingRole_;
	// ミュートフラグ
	bool muted_ = false;
};

using TrackPtr = std::unique_ptr<ITrack>;
} // namespace KCE
