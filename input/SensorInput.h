#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace KCE
{
/**
 * @brief マイコンからシリアルで届くセンサーの値を受け取る（シングルトン）。
 *
 * マイコンは 1 行に「名前:値」をカンマ区切りで並べて送る（例: "A0:512,D2:1\n"）。
 * ゲーム側は名前で値を読む（GetValue("A0")）。並びの順番は関係ない。
 * 名前はピン名でも意味の名前（"SLAP" など）でもよい。大文字と小文字は区別する。
 *
 * - 受信は別スレッドで行う。ゲーム側は Update の後に Get〜 で読むだけ
 * - 1 フレームの間に何行も届くので、最新の値に加えて、そのフレーム中の最大・最小も持つ。
 *   一瞬だけ立つデジタル入力（スイッチを素早く叩いたなど）を取りこぼさないため
 * - デジタル入力は 0 / 1 を送る。IsOn はフレーム中の最大がしきい値以上かで見る
 * - 繋がっていないときは全部 0 になる。ゲームは落とさない
 * - 繋がっているのに届いていない名前を読んだら、ログに 1 回だけ警告を出す（名前の打ち間違い・送り忘れ用）
 * - 一定時間データが来なければ切断扱いにして、開き直しを試み続ける
 */
class SensorInput
{
public:
	// 受け取れる名前の数の上限。超えた分は捨てる
	static constexpr size_t kMaxChannels = 16;
	// 名前の長さの上限
	static constexpr size_t kMaxNameLength = 15;

	/**
	 * @brief インスタンスを取得する。
	 * @return SensorInput のポインタ（所有しない）
	 */
	static SensorInput* GetInstance();

	/**
	 * @brief デバッグ表示を登録する。まだ接続はしない。
	 */
	void Initialize();

	/**
	 * @brief 切断して、デバッグ表示の登録を外す。
	 */
	void Finalize();

	/**
	 * @brief 受け取った値を 1 フレーム分確定させる。毎フレーム 1 回呼ぶ。
	 */
	void Update();

	/**
	 * @brief 指定のポートで受信を始める。開けなくても、開けるまで裏で試し続ける。
	 * @param portName "COM3" のようなポート名
	 * @param baudRate 通信速度。マイコン側の Serial.begin と合わせる
	 */
	void Connect(const std::string& portName, uint32_t baudRate = 115200);

	/**
	 * @brief 受信をやめてポートを閉じる。
	 */
	void Disconnect();

	/**
	 * @brief データが届いているか。ポートが開いていても、しばらく何も来なければ false。
	 * @return 届いていれば true
	 */
	bool IsConnected() const { return current_.connected; }

	/**
	 * @brief その名前の値が最後の行に入っていたか。
	 * @param name 名前
	 * @return 入っていれば true
	 */
	bool Has(std::string_view name) const;

	/**
	 * @brief 最新の値。
	 * @param name 名前
	 * @return 値。届いていない名前や未接続なら 0
	 */
	float GetValue(std::string_view name) const;

	/**
	 * @brief 前のフレームの値。変化量を見たいとき用。
	 * @param name 名前
	 * @return 値。届いていない名前なら 0
	 */
	float GetPrevValue(std::string_view name) const;

	/**
	 * @brief このフレームの間に届いた値の最大。一瞬だけの山を拾いたいとき用。
	 * @param name 名前
	 * @return 値。届いていない名前や未接続なら 0
	 */
	float GetMax(std::string_view name) const;

	/**
	 * @brief このフレームの間に届いた値の最小。
	 * @param name 名前
	 * @return 値。届いていない名前や未接続なら 0
	 */
	float GetMin(std::string_view name) const;

	/**
	 * @brief デジタル入力としてオンか。フレーム中に一度でもしきい値以上ならオン。
	 * @param name 名前
	 * @param threshold オンとみなす値。0 / 1 を送るなら既定のままでよい
	 * @return オンなら true
	 */
	bool IsOn(std::string_view name, float threshold = 0.5f) const;

	/**
	 * @brief デジタル入力としてオンになった瞬間か。
	 * @param name 名前
	 * @param threshold オンとみなす値
	 * @return オンになった瞬間なら true
	 */
	bool IsTriggered(std::string_view name, float threshold = 0.5f) const;

	/**
	 * @brief デジタル入力としてオフになった瞬間か。
	 * @param name 名前
	 * @param threshold オンとみなす値
	 * @return オフになった瞬間なら true
	 */
	bool IsReleased(std::string_view name, float threshold = 0.5f) const;

	/**
	 * @brief マイコンから 1 行届いたことにする。実機なしで確かめるとき用（テストや ImGui の疑似入力）。
	 *
	 * 受信スレッドが読んだ行と同じ扱いになり、ポートが開いていなくても接続中として扱う。
	 * @param line "A0:512,D2:1" のような 1 行（改行なし）
	 */
	void InjectLine(const std::string& line);

	/**
	 * @brief Settings の「システム > センサー入力」に出す表示。
	 */
	void DrawImGui();

private:
	friend std::unique_ptr<SensorInput> std::make_unique<SensorInput>();
	SensorInput() = default;

	SensorInput(const SensorInput&) = delete;
	SensorInput& operator=(const SensorInput&) = delete;

public:
	~SensorInput();

private:
	// 名前ごとの値
	struct Channel
	{
		std::array<char, kMaxNameLength + 1> name{};
		float value = 0.0f;
		float max = 0.0f;
		float min = 0.0f;
	};

	// 1 フレーム分の確定した状態
	struct Frame
	{
		std::array<Channel, kMaxChannels> channels{};
		size_t channelCount = 0;
		bool connected = false;
	};

	/** @brief 名前の場所を探す。なければ nullptr */
	static const Channel* Find(const Frame& frame, std::string_view name);

	/** @brief 今のフレームから名前で探す。繋がっているのに無ければ 1 回だけ警告する */
	const Channel* FindCurrent(std::string_view name) const;

	/** @brief 受信スレッドの中身 */
	void ReceiveLoop(std::string portName, uint32_t baudRate);

	/** @brief 1 行を名前と値に分けて pending 側に書き込む */
	void ParseLine(const std::string& line);

	static std::unique_ptr<SensorInput> instance_;

	// メインスレッドだけが触る
	Frame current_{};
	Frame previous_{};
	std::string portName_ = "COM3";
	int baudRate_ = 115200;
	// 警告済みの名前。同じ警告を毎フレーム出さないため
	mutable std::vector<std::string> warnedNames_;

	// 受信スレッド
	std::thread thread_;
	std::atomic<bool> running_ = false;
	std::mutex stopMutex_;
	std::condition_variable stopCv_;

	// 受信スレッドが書き、Update で読む。pendingMutex_ で守る
	std::mutex pendingMutex_;
	Frame pending_{};
	// このフレームに届いた行の数。0 なら max / min は value と同じにする
	int pendingLinesThisFrame_ = 0;
	std::chrono::steady_clock::time_point lastReceiveTime_{};
	bool portOpen_ = false;
	// InjectLine で行が入れられたか。true ならポートが閉じていても接続中として扱う
	bool injected_ = false;
	std::string lastLine_;
	uint64_t totalLines_ = 0;
	// 読めなかった行の数と、最後に読めなかった行。形式の間違いに気付くため
	uint64_t rejectedLines_ = 0;
	std::string lastRejectedLine_;

	// 接続が変わったときのログ用
	bool loggedConnected_ = false;

	// デバッグ表示用。1 秒に何行届いているか
	uint64_t rateStartLines_ = 0;
	std::chrono::steady_clock::time_point rateStartTime_{};
	float linesPerSecond_ = 0.0f;
};
} // namespace KCE
