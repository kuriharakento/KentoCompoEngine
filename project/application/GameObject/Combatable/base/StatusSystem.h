#pragma once

#include <vector>
#include <optional>
#include <functional>
#include <string>
#include <memory>


 // ===============================================
 // バフの種類を定義する列挙型
 // ===============================================

 /**
  * @enum BuffType
  * @brief バフの適用方法を定義
  *
  * @details
  * 各バフタイプの計算方法:
  * - Additive: 基礎値に直接加算 (base + value)
  * - Multiplicative: 基礎値に乗算 (base * value)
  * - Percentage: パーセンテージで増減 (base * (1 + value))
  * - Override: 他のバフを無視して値を上書き
  *
  * 計算順序: (base + Additive) * (1 + Percentage) * Multiplicative
  * ただし、Overrideが存在する場合は最大値のOverride値を使用
  */
enum class BuffType
{
    Additive,        ///< 加算バフ: HP+50、攻撃力+10 など
    Multiplicative,  ///< 乗算バフ: HP×2.0、攻撃力×1.5 など
    Percentage,      ///< 割合バフ: HP+50%、攻撃力+30% など
    Override         ///< 上書きバフ: 特殊な状態異常などで使用
};

/**
 * @enum BuffPriority
 * @brief バフの計算優先度
 *
 * @details
 * 同じBuffTypeのバフが複数ある場合、優先度の低い順に計算されます。
 * 装備品は通常Normalで、一時的な強化バフはHighに設定することで、
 * より予測可能な計算結果を得ることができます。
 */
enum class BuffPriority
{
    Low = 0,      ///< 低優先度: デバフや弱体化効果
    Normal = 1,   ///< 通常優先度: 装備品や基本的なバフ
    High = 2      ///< 高優先度: 一時的な強化スキルやアイテム効果
};

// ===============================================
// BuffConfig - バフの設定情報を保持する構造体
// ===============================================

/**
 * @struct BuffConfig
 * @brief バフの設定情報を定義する構造体
 *
 * @details
 * この構造体はバフの「設計図」として機能します。
 * 同じBuffConfigから複数のBuffInstanceを生成することが可能です。
 *
 * 使用例:
 * @code
 * // 10秒間攻撃力を50%増加させるバフ
 * BuffConfig powerUp("power_boost", 0.5f, BuffType::Percentage, 10.0f);
 *
 * // 永続的な装備ボーナス（+20 HP）
 * BuffConfig equipmentBonus("helmet_bonus", 20.0f, BuffType::Additive);
 *
 * // 最大3スタックまで重複可能な毒デバフ（タグ付き）
 * BuffConfig poison("poison", -2.0f, BuffType::Additive, 5.0f);
 * poison.maxStacks = 3;
 * poison.AddTag("debuff").AddTag("status_ailment").AddTag("poison");
 * @endcode
 */
struct BuffConfig
{
    std::string id;                    ///< バフの一意識別子（例: "fire_buff", "speed_boost"）
    float value;                       ///< バフの効果値（種類により意味が異なる）
    BuffType type;                     ///< バフの種類（加算、乗算、割合、上書き）
    std::optional<float> duration;     ///< 持続時間（秒）。nulloptの場合は永続バフ
    BuffPriority priority;             ///< 計算優先度（デフォルト: Normal）
    int maxStacks;                     ///< 最大スタック数（0=無制限、デフォルト: 1）
    bool refreshable;                  ///< 同じバフを再度付与した時に持続時間をリセットするか
    std::vector<std::string> tags;     ///< バフのタグ（カテゴリ分類用）

    /**
     * @brief BuffConfigのコンストラクタ
     *
     * @param id バフの一意識別子
     * @param val バフの効果値
     * @param t バフの種類
     * @param dur 持続時間（秒）。nulloptで永続バフ
     *
     * @note priority, maxStacks, refreshableはデフォルト値が設定されます
     */
    BuffConfig(std::string id, float val, BuffType t, std::optional<float> dur = std::nullopt);

    /**
     * @brief タグを追加（メソッドチェーン対応）
     * @param tag 追加するタグ
     * @return 自身の参照（メソッドチェーン用）
     */
    BuffConfig& AddTag(const std::string& tag);

    /**
     * @brief 特定のタグを持っているか確認
     * @param tag 確認するタグ
     * @return タグを持っていればtrue
     */
    bool HasTag(const std::string& tag) const;
};

// ===============================================
// BuffInstance - 実際に適用されているバフのインスタンス
// ===============================================

/**
 * @class BuffInstance
 * @brief 実際にステータスに適用されているバフのインスタンス
 *
 * @details
 * BuffConfigから生成され、残り時間やスタック数などの動的な状態を管理します。
 * BuffConfigが「設計図」なら、BuffInstanceは「実体」に相当します。
 *
 * 内部状態:
 * - 残り時間（時限バフの場合）
 * - 現在のスタック数
 * - バフの設定情報（BuffConfig）
 *
 * @note このクラスは通常、StatusValueクラスによって内部的に管理されます
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

    // ===============================================
    // 状態更新メソッド群
    // ===============================================

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

// ===============================================
// StatusChangeEvent - ステータス変更イベント情報
// ===============================================

/**
 * @struct StatusChangeEvent
 * @brief ステータス値が変更された時のイベント情報
 *
 * @details
 * SetChangeCallback()で登録したコールバック関数に渡されます。
 * UI更新やログ記録、実績解除などに利用できます。
 *
 * 使用例:
 * @code
 * statusValue.SetChangeCallback([](const StatusChangeEvent& event) {
 *     std::cout << "HP changed from " << event.oldValue
 *               << " to " << event.newValue << std::endl;
 *     if (event.causeBuffId != "") {
 *         std::cout << "Cause: " << event.causeBuffId << std::endl;
 *     }
 * });
 * @endcode
 */
struct StatusChangeEvent
{
    float oldValue;          ///< 変更前の値
    float newValue;          ///< 変更後の値
    std::string causeBuffId; ///< 変更の原因となったバフID（複数の場合は空文字列）
};

// ===============================================
// StatusValue - ステータス値を管理するクラス
// ===============================================

/**
 * @class StatusValue
 * @brief 単一のステータス値とそれに適用されるバフを管理するクラス
 *
 * @details
 * HP、攻撃力、移動速度など、あらゆる数値ステータスに使用できます。
 * 基礎値と複数のバフから最終的な値を計算し、キャッシュ機構により
 * 高速に値を取得できます。
 *
 * 主な機能:
 * - 基礎値の管理
 * - 複数のバフの適用と管理
 * - 時限バフの自動削除
 * - バフのスタック管理
 * - 値変更時のコールバック通知
 * - 計算結果のキャッシュ
 * - 条件付きバフ解除
 *
 * 使用例:
 * @code
 * // HP管理の例
 * StatusValue hp(100.0f);  // 基礎HP: 100
 *
 * // 装備による永続ボーナス（+20）
 * hp.AddBuff(BuffConfig("armor_bonus", 20.0f, BuffType::Additive));
 *
 * // スキルによる一時強化（10秒間50%増加）
 * hp.AddBuff(BuffConfig("fortify", 0.5f, BuffType::Percentage, 10.0f));
 *
 * std::cout << hp.GetValue() << std::endl;  // (100 + 20) * 1.5 = 180
 *
 * // デバフだけを解除
 * hp.RemoveBuffIf([](const BuffInstance* buff) {
 *     return buff->HasTag("debuff");
 * });
 *
 * // 時間経過
 * hp.Update(deltaTime);
 * @endcode
 *
 * @note スレッドセーフではありません。マルチスレッド環境では外部で同期が必要です
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

    /**
     * @brief 基礎値を取得
     * @return 基礎値（バフの影響を受けない元の値）
     */
    float GetBase() const;

    /**
     * @brief 基礎値を設定
     * @param value 新しい基礎値
     * @note 値が変更された場合、キャッシュが無効化され、コールバックが呼ばれます
     */
    void SetBase(float value);

    // ===============================================
    // 最終値の取得
    // ===============================================

    /**
     * @brief 最終的なステータス値を取得（バフの影響を含む）
     * @return 計算済みの最終値
     *
     * @details
     * この関数は内部でキャッシュ機構を使用しており、バフが変更されていない限り
     * 高速に値を返します。バフの変更があった場合のみ再計算されます。
     *
     * 計算式:
     * 1. 全バフを優先度順にソート
     * 2. 加算バフを合計: additiveSum
     * 3. 割合バフを合計: percentageMultiplier (1 + sum)
     * 4. 乗算バフを適用: multiplicativeValue (product)
     * 5. 最終値 = (base + additiveSum) * percentageMultiplier * multiplicativeValue
     * 6. Overrideバフがある場合は、その最大値を使用
     *
     * @note この関数はconstですが、内部でキャッシュを更新します（mutable）
     */
    float GetValue() const;

    // ===============================================
    // バフの管理
    // ===============================================

    /**
     * @brief バフを追加
     * @param config バフの設定情報
     * @return 成功したらtrue、失敗したらfalse
     *
     * @details
     * 同じIDのバフが既に存在する場合:
     * - スタック可能なら、スタック数を増やして持続時間をリフレッシュ
     * - スタック上限に達していたら、falseを返す
     *
     * 新規バフの場合:
     * - バフリストに追加し、キャッシュを無効化
     *
     * @note バフ追加後、値が変更された場合はコールバックが呼ばれます
     */
    bool AddBuff(const BuffConfig& config);

    /**
     * @brief 特定のバフを削除
     * @param buffId 削除するバフのID
     * @return 成功したらtrue、該当するバフが見つからなかったらfalse
     * @note バフ削除後、値が変更された場合はコールバックが呼ばれます
     */
    bool RemoveBuff(const std::string& buffId);

    /**
     * @brief 全てのバフをクリア
     * @note バフがクリアされた場合、コールバックが呼ばれます
     */
    void ClearAllBuffs();

    /**
     * @brief 特定の種類のバフをクリア
     * @param type クリアするバフの種類
     * @return 削除されたバフの数
     *
     * @details
     * 例: デバフだけを解除したい場合などに使用
     * @code
     * statusValue.ClearBuffsByType(BuffType::Additive);  // 加算バフのみクリア
     * @endcode
     */
    int ClearBuffsByType(BuffType type);

    /**
     * @brief 条件に一致するバフを全て削除
     * @param predicate 削除条件を判定する関数（trueを返すバフが削除される）
     * @return 削除されたバフの数
     *
     * @details
     * 使用例:
     * @code
     * // デバフ（負の効果）だけを削除
     * int removed = statusValue.RemoveBuffIf([](const BuffInstance* buff) {
     *     return buff->GetValue() < 0.0f;
     * });
     *
     * // 残り時間が3秒以下のバフを削除
     * statusValue.RemoveBuffIf([](const BuffInstance* buff) {
     *     return !buff->IsPermanent() && buff->GetRemainingTime() < 3.0f;
     * });
     *
     * // 優先度がLowのバフを削除
     * statusValue.RemoveBuffIf([](const BuffInstance* buff) {
     *     return buff->GetPriority() == BuffPriority::Low;
     * });
     *
     * // 「debuff」タグを持つバフを削除
     * statusValue.RemoveBuffIf([](const BuffInstance* buff) {
     *     return buff->HasTag("debuff");
     * });
     * @endcode
     */
    int RemoveBuffIf(std::function<bool(const BuffInstance*)> predicate);

    /**
     * @brief タグに一致するバフを全て削除
     * @param tag 削除するバフのタグ
     * @return 削除されたバフの数
     *
     * @details
     * 使用例:
     * @code
     * // 「debuff」タグを持つバフを全て解除
     * hp.RemoveBuffsByTag("debuff");
     *
     * // 状態異常を全て解除
     * hp.RemoveBuffsByTag("status_ailment");
     * @endcode
     */
    int RemoveBuffsByTag(const std::string& tag);

    /**
     * @brief 優先度に一致するバフを全て削除
     * @param priority 削除するバフの優先度
     * @return 削除されたバフの数
     *
     * @details
     * 使用例:
     * @code
     * // スキル「浄化」で低優先度のデバフを解除
     * hp.RemoveBuffsByPriority(BuffPriority::Low);
     * @endcode
     */
    int RemoveBuffsByPriority(BuffPriority priority);

    // ===============================================
    // 更新処理
    // ===============================================

    /**
     * @brief バフの時間を更新し、期限切れのバフを削除
     * @param deltaTime 経過時間（秒）
     *
     * @details
     * この関数は毎フレーム呼び出す必要があります。
     * - 全てのバフの残り時間を減らす
     * - 期限切れになったバフを削除
     * - バフが削除された場合、キャッシュを無効化しコールバックを呼ぶ
     *
     * @note ゲームループ内で定期的に呼び出してください
     */
    void Update(float deltaTime);

    // ===============================================
    // バフの確認
    // ===============================================

    /**
     * @brief 特定のバフが適用されているか確認
     * @param buffId 確認するバフのID
     * @return 適用されていればtrue、されていなければfalse
     */
    bool HasBuff(const std::string& buffId) const;

    /**
     * @brief 特定のバフの残り時間を取得
     * @param buffId 確認するバフのID
     * @return 残り時間（秒）。永続バフの場合はnullopt。バフが存在しない場合もnullopt
     */
    std::optional<float> GetBuffRemainingTime(const std::string& buffId) const;

    /**
     * @brief 適用されている全てのバフのIDリストを取得
     * @return バフIDのvector
     * @note デバッグやUI表示に使用できます
     */
    std::vector<std::string> GetBuffIds() const;

    /**
     * @brief 適用されているバフの数を取得
     * @return バフの数
     */
    size_t GetBuffCount() const;

    // ===============================================
    // コールバック設定
    // ===============================================

    /**
     * @brief ステータス値変更時のコールバックを設定
     * @param callback 値が変更された時に呼ばれる関数
     */
    void SetChangeCallback(ChangeCallback callback);

private:
    /**
     * @brief キャッシュを無効化（内部使用）
     * @details バフや基礎値が変更された時に呼ばれます
     */
    void MarkDirty();

    /**
     * @brief 最終値を計算（内部使用）
     * @return 計算された最終値
     * @details キャッシュが無効な時のみ呼ばる
     */
    float CalculateValue() const;

    // ===============================================
    // メンバ変数
    // ===============================================

    float base_;                                         // 基礎値
    mutable float cachedValue_;                          // キャッシュされた計算結果（const関数内で更新するためmutable）
    mutable bool isDirty_;                               // キャッシュが無効かどうか（const関数内で更新するためmutable）
    std::vector<std::unique_ptr<BuffInstance>> buffs_;   // 適用中のバフリスト
    ChangeCallback changeCallback_;                      // 値変更時のコールバック
};