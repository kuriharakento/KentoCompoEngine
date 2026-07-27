#pragma once
#include <string>
#include <functional>

namespace KCE
{
/**
 * @brief デルタタイム取得タイプ
 * 
 * タイマーの時間経過にどの時間を使用するかを指定する。
 */
enum class DeltaTimeType
{
	DeltaTime,      // タイムスケール適用済み（ゲーム時間）
	RealDeltaTime,  // タイムスケール未適用（実時間）
};

/**
 * @brief タイマークラス
 * 
 * 指定した時間が経過するとコールバックを呼び出すタイマー機能を提供する。
 * 開始時、毎フレーム、終了時のコールバックを設定可能。
 * タイムスケールの影響を受けるかどうかを選択できる。
 */
class Timer
{
public:
    /**
     * @brief デフォルトコンストラクタ
     */
    Timer()
        : name_(""), duration_(0.0f), elapsed_(0.0f), running_(false), finished_(false), deltaTimeType_(DeltaTimeType::DeltaTime)
    {
        onStart = []() {};
        onTick_ = [](float) {};
        onFinish_ = []() {};
    }

    /**
     * @brief コンストラクタ
     * @param name タイマーの識別名
     * @param duration タイマーの継続時間（秒）
     * @param deltaType 使用するデルタタイムのタイプ
     */
    Timer(const std::string& name, float duration, DeltaTimeType deltaType = DeltaTimeType::DeltaTime)
		: name_(name), duration_(duration), elapsed_(0.0f), running_(false), finished_(false), deltaTimeType_(deltaType)
	{
    	onStart = []() {};
		onTick_ = [](float) {};
    	onFinish_ = []() {};
	}

    // --- タイマー操作 ---

    /**
     * @brief タイマーを開始
     */
    void Start();

    /**
     * @brief タイマーをリセット（停止して経過時間を0に戻す）
     */
    void Reset();

    /**
     * @brief タイマーを停止（経過時間は保持）
     */
    void Stop();

    /**
     * @brief 毎フレーム呼び出す更新関数
     * @param deltaTime 経過時間
     */
    void Update(float deltaTime);

    // --- コールバック設定 ---

    /**
     * @brief 開始時コールバックを設定
     * @param callback タイマー開始時に呼び出される関数
     */
    void SetOnStart(std::function<void()> callback);

    /**
     * @brief 毎フレームコールバックを設定
     * @param callback 毎フレーム呼び出される関数（残り時間を引数に受け取る）
     */
    void SetOnTick(std::function<void(float)> callback);

    /**
     * @brief 終了時コールバックを設定
     * @param callback タイマー終了時に呼び出される関数
     */
    void SetOnFinish(std::function<void()> callback);

	// --- パラメータ設定 ---

	/**
	 * @brief 継続時間を設定
	 * @param duration 継続時間（秒）
	 */
	void SetDuration(float duration) { duration_ = duration; }

    // --- 状態取得 ---

    /**
     * @brief タイマーが動作中かどうかを取得
     * @return 動作中ならtrue
     */
    bool IsRunning() const;

    /**
     * @brief タイマーが終了したかどうかを取得
     * @return 終了済みならtrue
     */
    bool IsFinished() const;

    /**
     * @brief 残り時間を取得
     * @return 残り時間（秒）
     */
    float GetRemainingTime() const;

    /**
     * @brief 継続時間を取得
     * @return 設定された継続時間
     */
    float GetDuration() const;

	/**
	 * @brief 進行具合を取得（0.0〜1.0）
	 * @return 進行具合
	 */
	float GetProgress() const;

    /**
     * @brief タイマー名を取得
     * @return タイマーの識別名
     */
    std::string GetName() const;

	/**
	 * @brief デルタタイムタイプを取得
	 * @return デルタタイムのタイプ
	 */
	DeltaTimeType GetDeltaTimeType() const { return deltaTimeType_; }

private:
    std::string name_;      // タイマー名
    float duration_;        // 継続時間
    float elapsed_;         // 経過時間
    bool running_;          // 動作中フラグ
    bool finished_;         // 終了フラグ

	DeltaTimeType deltaTimeType_ = DeltaTimeType::DeltaTime; // 時間経過のタイプ

    std::function<void()> onStart;      // 開始時コールバック
    std::function<void(float)> onTick_; // 毎フレームコールバック
    std::function<void()> onFinish_;    // 終了時コールバック
};
} // namespace KCE
