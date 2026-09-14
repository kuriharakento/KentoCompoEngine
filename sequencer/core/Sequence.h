#pragma once
#include <memory>
#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "sequencer/core/BindingContext.h"
#include "sequencer/core/ITrack.h"

namespace KCE
{
/**
 * @brief タイムライン上の目印
 * @details サビ・Aメロ・ビートなど。編集時のスナップ先として使う。
 */
struct SequenceMarker
{
	float time = 0.0f;
	std::string name;
};

/**
 * @brief シーケンス全体のメタ情報
 */
struct SequenceMeta
{
	std::string name = "New Sequence";
	//! シーケンスの長さ（秒）。0以下ならトラックの最終キーから求める
	float duration = 0.0f;
	//! 楽曲のBPM。グリッドのスナップに使う。0なら拍のグリッドを出さない
	float bpm = 0.0f;
	//! 楽曲の先頭と1拍目のずれ（秒）
	float offset = 0.0f;
	//! 同期する音声のキー名。空なら音声同期なし（時刻は実時間で進む）
	std::string audioClip;
};

/**
 * @brief 演出データ本体
 *
 * @details 時刻 t を与えると、束ねたトラックを順に Evaluate して絵を作る。
 *          シーケンス自身は再生位置を持たない。位置の管理は SequencePlayer の役割で、
 *          この分離が SEQUENCER_PLAN 3.1 の純関数契約をデータ側で守るための形になっている。
 */
class Sequence
{
public:
	Sequence() = default;

	/**
	 * @brief 指定時刻の状態を全トラックに適用する
	 * @details ミュート中のトラックは飛ばす。トラックの評価順は登録順。
	 * @param time シーケンス先頭からの時刻（秒）
	 * @param ctx 役に実体を割り当てたコンテキスト
	 */
	void Evaluate(float time, const BindingContext& ctx);

	/**
	 * @brief 全トラックの対象について、現在の状態を退避する
	 * @param ctx バインディングコンテキスト
	 */
	void CaptureState(const BindingContext& ctx);

	/**
	 * @brief 退避した状態を全トラックの対象に書き戻す
	 * @param ctx バインディングコンテキスト
	 */
	void RestoreState(const BindingContext& ctx);

	// --- トラック ---

	/**
	 * @brief トラックを追加する
	 * @param track 追加するトラック
	 * @return 追加されたトラックへのポインタ。nullptrを渡した場合はnullptr
	 */
	ITrack* AddTrack(TrackPtr track);

	/**
	 * @brief トラックを取り外す（破棄せずに返す）
	 * @details Undo可能な削除のために、削除したトラックをコマンド側で保持できるようにする。
	 * @param index 取り外すトラックのインデックス
	 * @return 取り外したトラック。範囲外ならnullptr
	 */
	TrackPtr DetachTrack(size_t index);

	/**
	 * @brief 指定位置にトラックを挿入する
	 * @details Undo で削除を取り消すときに、元の位置へ戻すために使う。
	 * @param index 挿入位置。範囲を超える場合は末尾に追加される
	 * @param track 挿入するトラック
	 */
	void InsertTrack(size_t index, TrackPtr track);

	size_t GetTrackCount() const { return tracks_.size(); }

	/**
	 * @brief トラックを取得する
	 * @param index インデックス
	 * @return トラック。範囲外ならnullptr
	 */
	ITrack* GetTrack(size_t index) const;

	const std::vector<TrackPtr>& GetTracks() const { return tracks_; }

	// --- バインディング定義 ---

	const std::vector<BindingDefinition>& GetBindings() const { return bindings_; }
	std::vector<BindingDefinition>& GetBindings() { return bindings_; }

	/**
	 * @brief 役の定義を追加する（同名があれば何もしない）
	 * @param definition 役の定義
	 */
	void AddBinding(const BindingDefinition& definition);

	// --- マーカー ---

	const std::vector<SequenceMarker>& GetMarkers() const { return markers_; }
	std::vector<SequenceMarker>& GetMarkers() { return markers_; }

	// --- メタ情報 ---

	const SequenceMeta& GetMeta() const { return meta_; }
	SequenceMeta& GetMeta() { return meta_; }

	// --- エディタ用のデータ ---

	/**
	 * @brief エディタだけが使うデータ（プレビュー用の割り当てなど）
	 * @details 再生側は見ない。中身を知らなくても、読み込んで保存したときに消えないようそのまま持ち回る。
	 * @return JSON。無ければ null
	 */
	nlohmann::json& GetEditorData() { return editorData_; }
	const nlohmann::json& GetEditorData() const { return editorData_; }

	/**
	 * @brief シーケンスの長さを取得する
	 * @details メタに明示された長さがあればそれを、無ければ全トラックの
	 *          最終キー時刻の最大値を返す。
	 * @return 長さ（秒）
	 */
	float GetDuration() const;

	// --- 保存・読み込み ---

	/**
	 * @brief JSONにシリアライズする
	 * @return JSONオブジェクト
	 */
	nlohmann::json Serialize() const;

	/**
	 * @brief JSONから復元する
	 * @details 未知のトラック種別は読み飛ばす（前方互換）。
	 *          version が現在のスキーマより新しい場合は失敗する。
	 * @param json 入力JSON
	 * @param outError 失敗理由の出力先（任意）
	 * @return 復元に成功したら真
	 */
	bool Deserialize(const nlohmann::json& json, std::string* outError = nullptr);

	/**
	 * @brief ファイルに保存する
	 * @details 書き込み中の異常終了で既存データを失わないよう、
	 *          一時ファイルに書いてからリネームする。
	 * @param path 保存先のパス（Resources/json/sequence からの相対、または絶対）
	 * @return 保存に成功したら真
	 */
	bool SaveToFile(const std::string& path) const;

	/**
	 * @brief ファイルから読み込む
	 * @details 読み込みに失敗しても assert では落とさず、偽を返してログに出す。
	 * @param path 読み込むパス
	 * @param outError 失敗理由の出力先（任意）
	 * @return 読み込みに成功したら真
	 */
	bool LoadFromFile(const std::string& path, std::string* outError = nullptr);

	/**
	 * @brief シーケンスのファイルを置くフォルダ
	 * @details SaveToFile / LoadFromFile が相対パスを解決するときの基準と同じ場所。
	 * @return フォルダのフルパス（Resources/json/sequence）
	 */
	static std::filesystem::path GetSequenceDirectory();

	/**
	 * @brief 全トラック・全マーカーを破棄する
	 */
	void Clear();

private:
	// メタ情報
	SequenceMeta meta_;
	// 役の定義
	std::vector<BindingDefinition> bindings_;
	// トラック（評価順）
	std::vector<TrackPtr> tracks_;
	// タイムライン上の目印
	std::vector<SequenceMarker> markers_;
	// エディタだけが使うデータ。再生側は見ない
	nlohmann::json editorData_;
};
} // namespace KCE
