#include "StatusSystem.h"
#include <algorithm>

 // ===============================================
 // BuffConfig
 // ===============================================

BuffConfig::BuffConfig(std::string id, float val, BuffType t, std::optional<float> dur)
    : id(std::move(id))
    , value(val)
    , type(t)
    , duration(dur)
    , priority(BuffPriority::Normal)  // デフォルトは通常優先度
    , maxStacks(1)                     // デフォルトはスタック不可
    , refreshable(true)                // デフォルトは持続時間リフレッシュ可能
{
}

BuffConfig& BuffConfig::AddTag(const std::string& tag)
{
    tags.push_back(tag);
    return *this;  // メソッドチェーン用に自身の参照を返す
}

bool BuffConfig::HasTag(const std::string& tag) const
{
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

// ===============================================
// BuffInstance
// ===============================================

BuffInstance::BuffInstance(const BuffConfig& config)
    : config_(config)
    , remainingTime_(config.duration)  // durationがnulloptなら永続バフ
    , stackCount_(1)                    // 初期スタック数は1
{
}

const std::string& BuffInstance::GetId() const
{
    return config_.id;
}

float BuffInstance::GetValue() const
{
    // スタック数を考慮した効果値を返す
    return config_.value * stackCount_;
}

BuffType BuffInstance::GetType() const
{
    return config_.type;
}

BuffPriority BuffInstance::GetPriority() const
{
    return config_.priority;
}

int BuffInstance::GetStackCount() const
{
    return stackCount_;
}

bool BuffInstance::IsExpired() const
{
    // 時限バフで、かつ残り時間が0以下なら期限切れ
    return remainingTime_.has_value() && *remainingTime_ <= 0.0f;
}

bool BuffInstance::IsPermanent() const
{
    // durationがnulloptなら永続バフ
    return !remainingTime_.has_value();
}

float BuffInstance::GetRemainingTime() const
{
    // 永続バフの場合は-1.0fを返す
    return remainingTime_.value_or(-1.0f);
}

bool BuffInstance::HasTag(const std::string& tag) const
{
    return config_.HasTag(tag);
}

const std::vector<std::string>& BuffInstance::GetTags() const
{
    return config_.tags;
}

void BuffInstance::Update(float deltaTime)
{
    // 時限バフの場合のみ残り時間を減らす
    if (remainingTime_.has_value())
    {
        *remainingTime_ -= deltaTime;
    }
}

bool BuffInstance::TryAddStack()
{
    // maxStacks == 0 は無制限スタック可能を意味する
    if (config_.maxStacks == 0 || stackCount_ < config_.maxStacks)
    {
        stackCount_++;
        return true;  // スタック追加成功
    }
    return false;  // スタック上限に達している
}

void BuffInstance::RefreshDuration()
{
    // リフレッシュ可能な時限バフの場合のみ、持続時間を初期値に戻す
    if (config_.refreshable && config_.duration.has_value())
    {
        remainingTime_ = config_.duration;
    }
}

// ===============================================
// StatusValue 
// ===============================================

StatusValue::StatusValue(float baseValue)
    : base_(baseValue)
    , cachedValue_(baseValue)  // 初期状態ではキャッシュ = 基礎値
    , isDirty_(false)          // 初期状態はクリーン
    , changeCallback_(nullptr) // コールバックは未設定
{
}

float StatusValue::GetBase() const
{
    return base_;
}

void StatusValue::SetBase(float value)
{
    // 値が実際に変更された場合のみ処理
    if (base_ != value)
    {
        float oldValue = GetValue();  // 変更前の最終値を記録
        base_ = value;
        MarkDirty();  // キャッシュを無効化

        // コールバックが設定されていれば呼び出す
        if (changeCallback_)
        {
            StatusChangeEvent event{ oldValue, GetValue(), "" };
            changeCallback_(event);
        }
    }
}

float StatusValue::GetValue() const
{
    // キャッシュが無効な場合のみ再計算
    if (isDirty_)
    {
        cachedValue_ = CalculateValue();
        isDirty_ = false;
    }
    return cachedValue_;
}

bool StatusValue::AddBuff(const BuffConfig& config)
{
    // 既存のバフを検索（同じIDのバフが既に存在するか？）
    auto it = std::find_if(buffs_.begin(), buffs_.end(),
                           [&config](const auto& buff) { return buff->GetId() == config.id; });

    if (it != buffs_.end())
    {
        // 同じIDのバフが既に存在する場合
        // スタック追加を試みる
        if ((*it)->TryAddStack())
        {
            // スタック追加成功
            (*it)->RefreshDuration();  // 持続時間をリフレッシュ
            MarkDirty();               // キャッシュを無効化

            // コールバック通知
            if (changeCallback_)
            {
                float oldValue = cachedValue_;
                StatusChangeEvent event{ oldValue, GetValue(), config.id };
                changeCallback_(event);
            }
            return true;
        }
        // スタック上限に達している場合はfalseを返す
        return false;
    }

    // 新規バフの追加
    buffs_.push_back(std::make_unique<BuffInstance>(config));
    MarkDirty();  // キャッシュを無効化

    // コールバック通知
    if (changeCallback_)
    {
        float oldValue = cachedValue_;
        StatusChangeEvent event{ oldValue, GetValue(), config.id };
        changeCallback_(event);
    }

    return true;
}

bool StatusValue::RemoveBuff(const std::string& buffId)
{
    // 指定されたIDのバフを検索
    auto it = std::find_if(buffs_.begin(), buffs_.end(),
                           [&buffId](const auto& buff) { return buff->GetId() == buffId; });

    if (it != buffs_.end())
    {
        // バフが見つかった場合
        float oldValue = GetValue();  // 削除前の値を記録
        buffs_.erase(it);            // バフを削除
        MarkDirty();                  // キャッシュを無効化

        // コールバック通知
        if (changeCallback_)
        {
            StatusChangeEvent event{ oldValue, GetValue(), buffId };
            changeCallback_(event);
        }
        return true;
    }

    // バフが見つからなかった
    return false;
}

void StatusValue::ClearAllBuffs()
{
    // バフが1つでも存在する場合のみ処理
    if (!buffs_.empty())
    {
        float oldValue = GetValue();  // クリア前の値を記録
        buffs_.clear();              // 全バフを削除
        MarkDirty();                  // キャッシュを無効化

        // コールバック通知
        if (changeCallback_)
        {
            // 原因バフIDは空文字列（複数のバフが削除されたため）
            StatusChangeEvent event{ oldValue, GetValue(), "" };
            changeCallback_(event);
        }
    }
}

int StatusValue::ClearBuffsByType(BuffType type)
{
    auto oldSize = buffs_.size();
    float oldValue = GetValue();  // クリア前の値を記録

    // 指定された種類のバフを削除
    buffs_.erase(
        std::remove_if(buffs_.begin(), buffs_.end(),
                       [type](const auto& buff) { return buff->GetType() == type; }),
        buffs_.end()
    );

    int removedCount = static_cast<int>(oldSize - buffs_.size());

    // 実際にバフが削除された場合のみ処理
    if (removedCount > 0)
    {
        MarkDirty();  // キャッシュを無効化

        // コールバック通知
        if (changeCallback_)
        {
            StatusChangeEvent event{ oldValue, GetValue(), "" };
            changeCallback_(event);
        }
    }

    return removedCount;
}

int StatusValue::RemoveBuffIf(std::function<bool(const BuffInstance*)> predicate)
{
    if (!predicate)
    {
        return 0; // predicateがnullptrの場合は何もしない
    }

    float oldValue = GetValue();
    auto oldSize = buffs_.size();

    // 条件に一致するバフを削除
    buffs_.erase(
        std::remove_if(buffs_.begin(), buffs_.end(),
                       [&predicate](const auto& buff) { return predicate(buff.get()); }),
        buffs_.end()
    );

    int removedCount = static_cast<int>(oldSize - buffs_.size());

    // バフが削除された場合のみ処理
    if (removedCount > 0)
    {
        MarkDirty();

        if (changeCallback_)
        {
            StatusChangeEvent event{ oldValue, GetValue(), "" };
            changeCallback_(event);
        }
    }

    return removedCount;
}

int StatusValue::RemoveBuffsByTag(const std::string& tag)
{
    return RemoveBuffIf([&tag](const BuffInstance* buff) {
        return buff->HasTag(tag);
                        });
}

int StatusValue::RemoveBuffsByPriority(BuffPriority priority)
{
    return RemoveBuffIf([priority](const BuffInstance* buff) {
        return buff->GetPriority() == priority;
                        });
}

void StatusValue::Update(float deltaTime)
{
    // まず全てのバフの時間を更新
    bool hasExpired = false;

    for (auto& buff : buffs_)
    {
        buff->Update(deltaTime);
        if (buff->IsExpired())
        {
            hasExpired = true;
        }
    }

    // 期限切れのバフがない場合は早期リターン（最適化）
    if (!hasExpired)
    {
        return;
    }

    // 期限切れバフの削除処理
    float oldValue = GetValue();
    auto oldSize = buffs_.size();

    // 期限切れのバフを削除
    buffs_.erase(
        std::remove_if(buffs_.begin(), buffs_.end(),
                       [](const auto& buff) { return buff->IsExpired(); }),
        buffs_.end()
    );

    // 実際にバフが削除された場合のみ処理
    if (buffs_.size() != oldSize)
    {
        MarkDirty();  // キャッシュを無効化

        // コールバック通知
        if (changeCallback_)
        {
            StatusChangeEvent event{ oldValue, GetValue(), "" };
            changeCallback_(event);
        }
    }
}

bool StatusValue::HasBuff(const std::string& buffId) const
{
    // 指定されたIDのバフが存在するか確認
    return std::any_of(buffs_.begin(), buffs_.end(),
                       [&buffId](const auto& buff) { return buff->GetId() == buffId; });
}

std::optional<float> StatusValue::GetBuffRemainingTime(const std::string& buffId) const
{
    // 指定されたIDのバフを検索
    auto it = std::find_if(buffs_.begin(), buffs_.end(),
                           [&buffId](const auto& buff) { return buff->GetId() == buffId; });

    if (it != buffs_.end())
    {
        float time = (*it)->GetRemainingTime();
        // -1.0f（永続バフ）の場合はnulloptを返す
        return time < 0.0f ? std::nullopt : std::optional<float>(time);
    }

    // バフが見つからなかった場合もnullopt
    return std::nullopt;
}

std::vector<std::string> StatusValue::GetBuffIds() const
{
    // 全てのバフのIDをvectorで返す（デバッグやUI表示用）
    std::vector<std::string> ids;
    ids.reserve(buffs_.size());  // メモリ確保を最適化

    for (const auto& buff : buffs_)
    {
        ids.push_back(buff->GetId());
    }

    return ids;
}

size_t StatusValue::GetBuffCount() const
{
    return buffs_.size();
}

void StatusValue::SetChangeCallback(ChangeCallback callback)
{
    changeCallback_ = std::move(callback);
}

void StatusValue::MarkDirty()
{
    // 次回GetValue()が呼ばれた時に再計算される
    isDirty_ = true;
}

float StatusValue::CalculateValue() const
{
     // バフを優先度順にソートするための準備
    std::vector<BuffInstance*> sortedBuffs;
    sortedBuffs.reserve(buffs_.size());
    for (const auto& buff : buffs_)
    {
        sortedBuffs.push_back(buff.get());
    }

    // 優先度の低い順にソート（Low → Normal → High）
    std::sort(sortedBuffs.begin(), sortedBuffs.end(),
              [](const BuffInstance* a, const BuffInstance* b) {
                  return static_cast<int>(a->GetPriority()) < static_cast<int>(b->GetPriority());
              });

    // 各バフタイプの値を集計するための変数
    float result = base_;                // 基礎値から開始
    float additiveSum = 0.0f;            // 加算バフの合計
    float percentageMultiplier = 1.0f;   // 割合バフの乗数（1.0 + パーセンテージの合計）
    float multiplicativeValue = 1.0f;    // 乗算バフの積
    float maxOverride = result;          // 上書きバフの最大値
    bool hasOverride = false;            // 上書きバフが存在するか

    // バフを種類ごとに処理
    for (const auto* buff : sortedBuffs)
    {
        switch (buff->GetType())
        {
        case BuffType::Additive:
            // 加算バフ: 値をそのまま加算
            // 例: +10, +20 → additiveSum = 30
            additiveSum += buff->GetValue();
            break;

        case BuffType::Multiplicative:
            // 乗算バフ: 値を乗算
            // 例: ×1.5, ×2.0 → multiplicativeValue = 3.0
            multiplicativeValue *= buff->GetValue();
            break;

        case BuffType::Percentage:
            // 割合バフ: パーセンテージを加算
            // 例: +50% (+0.5), +30% (+0.3) → percentageMultiplier = 1.8
            percentageMultiplier += buff->GetValue();
            break;

        case BuffType::Override:
            // 上書きバフ: 最大値を記録
            // 複数のOverrideバフがある場合、最も大きい値が使われる
            hasOverride = true;
            maxOverride = std::max(maxOverride, buff->GetValue());
            break;
        }
    }

    // 最終計算
    if (!hasOverride)
    {
        // 通常の計算: (基礎値 + 加算) × 割合 × 乗算
        result = (result + additiveSum) * percentageMultiplier * multiplicativeValue;
    }
    else
    {
        // 上書きバフがある場合: 他のバフを無視して上書き値を使用
        // 例: 石化状態で移動速度を強制的に0にする場合など
        result = maxOverride;
    }

    return result;
}