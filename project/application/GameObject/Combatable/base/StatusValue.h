#pragma once

#include <vector>
#include <algorithm>

struct BuffValue
{
    float value;      // 加算量、または割合
    bool isPercent;   // trueなら割合バフ、falseなら加算バフ
    float duration;   // 残り効果時間（秒、永続なら-1.0fなどで表現）

    BuffValue(float v, bool percent, float dur = -1.0f)
        : value(v), isPercent(percent), duration(dur)
    {
    }
};

struct StatusValue
{
    float base = 0.0f;
    std::vector<BuffValue> buffs;

    // 最新の値を返す
    float Calc() const
    {
        float add = 0.0f;
        float percent = 1.0f;
        for (const auto& b : buffs)
        {
            if (b.isPercent) percent += b.value;
            else add += b.value;
        }
        return (base + add) * percent;
    }

    // バフ管理: 経過時間（秒）を与えて更新＆切れたバフを削除
    void Update(float deltaTime)
    {
        for (auto& b : buffs)
        {
            if (b.duration > 0.0f)
            {
                b.duration -= deltaTime;
            }
        }
        // durationが0以下のものを削除
        buffs.erase(
            std::remove_if(buffs.begin(), buffs.end(),
                           [](const BuffValue& b) { return b.duration >= 0.0f && b.duration <= 0.0f; }),
            buffs.end()
        );
    }
};