#pragma once

#include <vector>
#include <optional>
#include <functional>
#include <string>
#include <memory>


 /**
  * @enum BuffType
  * @brief バフの適用方法を定義
  */
enum class BuffType
{
    Additive,        // 加算バフ
    Multiplicative,  // 乗算バフ
    Percentage,      // 割合バフ
    Override         // 上書きバフ
};

/**
 * @enum BuffPriority
 * @brief バフの計算優先度
 */
enum class BuffPriority
{
    Low = 0,      // 低優先度: デバフや弱体化効果
    Normal = 1,   // 通常優先度: 装備品や基本的なバフ
    High = 2      // 高優先度: 一時的な強化スキルやアイテム効果
};

/**
 * @struct BuffConfig
 * @brief バフの設定情報を定義する構造体
 */
struct BuffConfig
{
    std::string id;                    // バフの一意識別子（例: "fire_buff", "speed_boost"）
    float value;                       // バフの効果値（種類により意味が異なる）
    BuffType type;                     // バフの種類（加算、乗算、割合、上書き）
    std::optional<float> duration;     // 持続時間（秒）。nulloptの場合は永続バフ
    BuffPriority priority;             // 計算優先度（デフォルト: Normal）
    int maxStacks;                     // 最大スタック数（0=無制限、デフォルト: 1）
    bool refreshable;                  // 同じバフを再度付与した時に持続時間をリセットするか
    std::vector<std::string> tags;     // バフのタグ（カテゴリ分類用）

    /**
     * @brief BuffConfigのコンストラクタ
     *
     * @param id バフの一意識別子
     * @param val バフの効果値
     * @param t バフの種類
     * @param dur 持続時間（秒）。nulloptで永続バフ
     */
    BuffConfig(std::string id, float val, BuffType t, std::optional<float> dur = std::nullopt);

    /**
     * @brief タグを追加
     * @param tag 追加するタグ
     * @return 自身の参照
     */
    BuffConfig& AddTag(const std::string& tag);

    /**
     * @brief 特定のタグを持っているか確認
     * @param tag 確認するタグ
     * @return タグを持っていればtrue
     */
    bool HasTag(const std::string& tag) const;
};
/**
 * @class BuffInstance
 * @brief 実際にステータスに適用されているバフのインスタンス
 */
class BuffInstance
{
public:
    /**
     * @brief BuffInstanceのコンストラクタ
     * @param config バフの設定情報
     */
    explicit BuffInstance(const BuffConfig& config);

    // ===============================================
    // Getter メソッド群
    // ===============================================

    /**
     * @brief バフのIDを取得
     * @return バフの一意識別子
     */
    const std::string& GetId() const;

    /**
     * @brief バフの現在の効果値を取得（スタック数を考慮）
     * @return 効果値 × スタック数
     * @note スタック数が3、効果値が5なら、15を返します
     */
    float GetValue() const;

    /**
     * @brief バフの種類を取得
     * @return BuffType (Additive, Multiplicative, Percentage, Override)
     */
    BuffType GetType() const;

    /**
     * @brief バフの優先度を取得
     * @return BuffPriority (Low, Normal, High)
     */
    BuffPriority GetPriority() const;

    /**
     * @brief 現在のスタック数を取得
     * @return スタック数（1以上）
     */
    int GetStackCount() const;

    /**
     * @brief バフが期限切れかどうかを判定
     * @return 期限切れならtrue、まだ有効ならfalse
     * @note 永続バフは常にfalseを返します
     */
    bool IsExpired() const;

    /**
     * @brief 永続バフかどうかを判定
     * @return 永続バフならtrue、時限バフならfalse
     */
    bool IsPermanent() const;

    /**
     * @brief 残り時間を取得
     * @return 残り時間（秒）。永続バフの場合は-1.0f
     */
    float GetRemainingTime() const;

    /**
     * @brief 特定のタグを持っているか確認
     * @param tag 確認するタグ
     * @return タグを持っていればtrue
     */
    bool HasTag(const std::string& tag) const;

    /**
     * @brief 全てのタグを取得
     * @return タグのvector
     */
    const std::vector<std::string>& GetTags() const;

    /**
     * @brief バフの残り時間を更新
     * @param deltaTime 経過時間（秒）
     * @note 永続バフの場合は何もしません
     */
    void Update(float deltaTime);

    /**
     * @brief スタック数を1増やす
     * @return 成功したらtrue、スタック上限に達していたらfalse
     * @note maxStacks=0の場合は無制限にスタック可能
     */
    bool TryAddStack();

    /**
     * @brief バフの持続時間をリセット
     * @note refreshable=trueの時限バフの場合のみ有効
     */
    void RefreshDuration();

private:
    BuffConfig config_;                      ///< バフの設定情報
    std::optional<float> remainingTime_;     ///< 残り時間（永続バフの場合はnullopt）
    int stackCount_;                         ///< 現在のスタック数
};

/**
 * @struct StatusChangeEvent
 * @brief ステータス値が変更された時のイベント情報
 */
struct StatusChangeEvent
{
    float oldValue;          // 変更前の値
    float newValue;          // 変更後の値
    std::string causeBuffId; // 変更の原因となったバフID（複数の場合は空文字列）
};

/**
 * @class StatusValue
 * @brief 単一のステータス値とそれに適用されるバフを管理するクラス
 */
class StatusValue
{
public:
    /// @brief ステータス変更時のコールバック関数型
    using ChangeCallback = std::function<void(const StatusChangeEvent&)>;

    /**
     * @brief StatusValueのコンストラクタ
     * @param baseValue 基礎値（デフォルト: 0.0f）
     */
    explicit StatusValue(float baseValue = 0.0f);

    // ===============================================
    // 基礎値の取得・設定
    // ===============================================

	// 基礎値を取得
    float GetBase() const;

	// 基礎値を設定
    void SetBase(float value);

	// 最終的なステータス値を取得
    float GetValue() const;

    // ===============================================
    // バフの管理
    // ===============================================

	// 新しいバフを追加
    bool AddBuff(const BuffConfig& config);

	// 特定のバフを削除
    bool RemoveBuff(const std::string& buffId);

	// 全てのバフを削除
    void ClearAllBuffs();

	// 特定の種類のバフを全て削除
    int ClearBuffsByType(BuffType type);

	// 条件に合致するバフを削除
    int RemoveBuffIf(std::function<bool(const BuffInstance*)> predicate);

	// 特定のタグを持つバフを全て削除
    int RemoveBuffsByTag(const std::string& tag);

	/// 特定の優先度のバフを全て削除
    int RemoveBuffsByPriority(BuffPriority priority);

	// 更新
    void Update(float deltaTime);

	// バフの存在確認
	bool HasBuff(const std::string& buffId) const;

	// 特定のバフの残り時間を取得
	std::optional<float> GetBuffRemainingTime(const std::string& buffId) const;

	// 全てのバフIDを取得
    std::vector<std::string> GetBuffIds() const;

	// 適用中のバフ数を取得
    size_t GetBuffCount() const;

	// ステータス変更時のコールバックを設定
    void SetChangeCallback(ChangeCallback callback);

private: // メンバ関数
	// キャッシュを無効化
    void MarkDirty();

	// キャッシュが無効な場合に値を再計算
    float CalculateValue() const;

private: // メンバ変数
    float base_;                                         // 基礎値
    mutable float cachedValue_;                          // キャッシュされた計算結果（const関数内で更新するためmutable）
    mutable bool isDirty_;                               // キャッシュが無効かどうか（const関数内で更新するためmutable）
    std::vector<std::unique_ptr<BuffInstance>> buffs_;   // 適用中のバフリスト
    ChangeCallback changeCallback_;                      // 値変更時のコールバック
};