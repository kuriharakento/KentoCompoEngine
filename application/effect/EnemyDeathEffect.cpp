#include "EnemyDeathEffect.h"
// component
#include "effects/particle/component/single/AccelerationComponent.h"
#include "effects/particle/component/single/BounceComponent.h"
#include "effects/particle/component/single/ColorFadeOutComponent.h"
#include "effects/particle/component/single/DragComponent.h"

EnemyDeathEffect::EnemyDeathEffect()
{
}

EnemyDeathEffect::~EnemyDeathEffect()
{
}

void EnemyDeathEffect::Initialize()
{
    InitializeBloodEmitter();
    InitializeFragmentEmitter();
    InitializeExplosionEmitter();
    InitializeElectricEmitter();
    InitializeDissolveEmitter();
    InitializeSmokeEmitter();
}

void EnemyDeathEffect::PlayDeathEffect(const Vector3& position, EffectType type)
{
    switch (type)
    {
    case EffectType::Normal:
        // 通常の死亡エフェクト（血飛沫 + 破片）
        bloodEmitter_->Start(position, 30, 0.2f);
        fragmentEmitter_->Start(position, 15, 0.1f);
        smokeEmitter_->Start(position, 8, 0.3f);
        break;

    case EffectType::Explosive:
        // 爆発的な死亡エフェクト
        PlayExplosionEffect(position);
        break;

    case EffectType::Electric:
        // 電撃系の死亡エフェクト
        PlayElectricEffect(position);
        break;

    case EffectType::Dissolve:
        // 消滅系の死亡エフェクト
        PlayDissolveEffect(position);
        break;
    }
}

void EnemyDeathEffect::PlayExplosionEffect(const Vector3& position, float scale)
{
    // 爆発エミッターの設定を調整
    explosionEmitter_->SetInitialScale(Vector3(scale, scale, scale));

    // 連鎖的なエフェクト
    explosionEmitter_->Start(position, 25, 0.1f);                // メイン爆発
    fragmentEmitter_->Start(position, 40, 0.15f);                // 破片（多め）
    smokeEmitter_->Start(position, 15, 1.0f);                    // 煙（長持ち）

    // 位置をずらした二次爆発
    Vector3 secondaryPos = position;
    secondaryPos.y += 0.5f;
    explosionEmitter_->Start(secondaryPos, 10, 0.05f, false);    // 二次爆発（少なめ）
}

void EnemyDeathEffect::PlayElectricEffect(const Vector3& position)
{
    // 電撃エミッターの設定
    electricEmitter_->Start(position, 35, 0.2f);

    // 電撃のダメージで少量の破片と煙も発生
    fragmentEmitter_->Start(position, 8, 0.1f);
    smokeEmitter_->Start(position, 5, 0.3f);

    // 複数の電撃ポイント（分岐する感じ）
    Vector3 offset1(1.0f, 0.2f, 0.5f);
    Vector3 offset2(-0.8f, 0.0f, -0.3f);
    Vector3 offset3(0.3f, 0.5f, -0.7f);

    electricEmitter_->Start(position + offset1, 10, 0.1f, false);
    electricEmitter_->Start(position + offset2, 10, 0.1f, false);
    electricEmitter_->Start(position + offset3, 10, 0.1f, false);
}

void EnemyDeathEffect::PlayDissolveEffect(const Vector3& position)
{
    // 消滅エミッターの設定
    dissolveEmitter_->Start(position, 60, 1.5f);  // 長い消滅時間

    // 消滅しながら上昇するエフェクト
    smokeEmitter_->Start(position, 12, 1.0f);
}

void EnemyDeathEffect::InitializeBloodEmitter()
{
    // 血飛沫エミッターの初期化
    bloodEmitter_ = std::make_unique<ParticleEmitter>();
    bloodEmitter_->Initialize("enemy_blood", bloodTexturePath_);

    // 基本パラメーター設定
    bloodEmitter_->SetEmitRate(0.01f);
    bloodEmitter_->SetInitialLifeTime(1.2f);
    bloodEmitter_->SetInitialScale(Vector3(0.2f, 0.2f, 0.2f));
    bloodEmitter_->SetInitialColor(Vector4(0.7f, 0.0f, 0.0f, 0.9f));
    bloodEmitter_->SetEmitRange(Vector3(-0.5f, 0.0f, -0.5f), Vector3(0.5f, 1.0f, 0.5f));
    bloodEmitter_->SetBillborad(true);

    // ランダム設定
    bloodEmitter_->SetRandomVelocity(true);
    bloodEmitter_->SetRandomScale(true);
    bloodEmitter_->SetRandomColor(true);

    // 範囲設定
    bloodEmitter_->SetRandomVelocityRange(AABB(Vector3(-3.0f, 2.0f, -3.0f), Vector3(3.0f, 5.0f, 3.0f)));
    bloodEmitter_->SetRandomScaleRange(AABB(Vector3(0.15f, 0.15f, 0.15f), Vector3(0.4f, 0.4f, 0.4f)));
    bloodEmitter_->SetRandomColorRange(
        Vector4(0.6f, 0.0f, 0.0f, 0.8f),
        Vector4(0.9f, 0.1f, 0.1f, 1.0f)
    );

    // コンポーネント設定
    bloodEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    bloodEmitter_->AddComponent(std::make_shared<AccelerationComponent>(Vector3(0.0f, -9.8f, 0.0f))); // 重力
    bloodEmitter_->AddComponent(std::make_shared<DragComponent>(0.97f)); // 空気抵抗
    bloodEmitter_->AddComponent(std::make_shared<BounceComponent>(0.0f, 0.3f, 0.1f)); // 地面でのバウンド
}

void EnemyDeathEffect::InitializeFragmentEmitter()
{
    // 破片エミッターの初期化
    fragmentEmitter_ = std::make_unique<ParticleEmitter>();
    fragmentEmitter_->Initialize("enemy_fragment", fragmentTexturePath_);

    // 基本パラメーター設定
    fragmentEmitter_->SetEmitRate(0.01f);
    fragmentEmitter_->SetInitialLifeTime(1.5f);
    fragmentEmitter_->SetInitialScale(Vector3(0.15f, 0.15f, 0.15f));
    fragmentEmitter_->SetInitialColor(Vector4(0.6f, 0.6f, 0.6f, 1.0f));
    fragmentEmitter_->SetEmitRange(Vector3(-0.3f, 0.0f, -0.3f), Vector3(0.3f, 1.5f, 0.3f));
    fragmentEmitter_->SetBillborad(true);

    // ランダム設定
    fragmentEmitter_->SetRandomVelocity(true);
    fragmentEmitter_->SetRandomScale(true);
    fragmentEmitter_->SetRandomRotation(true);

    // 範囲設定
    fragmentEmitter_->SetRandomVelocityRange(AABB(Vector3(-4.0f, 3.0f, -4.0f), Vector3(4.0f, 6.0f, 4.0f)));
    fragmentEmitter_->SetRandomScaleRange(AABB(Vector3(0.1f, 0.1f, 0.1f), Vector3(0.25f, 0.25f, 0.25f)));
    fragmentEmitter_->SetRandomRotationRange(AABB(Vector3(-3.14f, -3.14f, -3.14f), Vector3(3.14f, 3.14f, 3.14f)));

    // コンポーネント設定
    fragmentEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    fragmentEmitter_->AddComponent(std::make_shared<AccelerationComponent>(Vector3(0.0f, -12.0f, 0.0f))); // 重力
    fragmentEmitter_->AddComponent(std::make_shared<DragComponent>(0.98f)); // 空気抵抗
    fragmentEmitter_->AddComponent(std::make_shared<BounceComponent>(0.0f, 0.4f, 0.2f)); // 地面でのバウンド
}

void EnemyDeathEffect::InitializeExplosionEmitter()
{
    // 爆発エミッターの初期化
    explosionEmitter_ = std::make_unique<ParticleEmitter>();
    explosionEmitter_->Initialize("explosion", explosionTexturePath_);

    // 基本パラメーター設定
    explosionEmitter_->SetEmitRate(0.005f);
    explosionEmitter_->SetInitialLifeTime(0.8f);
    explosionEmitter_->SetInitialScale(Vector3(0.5f, 0.5f, 0.5f));
    explosionEmitter_->SetInitialColor(Vector4(1.0f, 0.7f, 0.2f, 0.9f));
    explosionEmitter_->SetEmitRange(Vector3(-0.2f, 0.0f, -0.2f), Vector3(0.2f, 0.4f, 0.2f));
    explosionEmitter_->SetBillborad(true);

    // ランダム設定
    explosionEmitter_->SetRandomVelocity(true);
    explosionEmitter_->SetRandomScale(true);
    explosionEmitter_->SetRandomColor(true);

    // 範囲設定
    explosionEmitter_->SetRandomVelocityRange(AABB(Vector3(-2.0f, 0.5f, -2.0f), Vector3(2.0f, 4.0f, 2.0f)));
    explosionEmitter_->SetRandomScaleRange(AABB(Vector3(0.3f, 0.3f, 0.3f), Vector3(1.0f, 1.0f, 1.0f)));
    explosionEmitter_->SetRandomColorRange(
        Vector4(0.9f, 0.4f, 0.0f, 0.8f),
        Vector4(1.0f, 0.8f, 0.3f, 1.0f)
    );

    // コンポーネント設定
    explosionEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    explosionEmitter_->AddComponent(std::make_shared<DragComponent>(0.9f)); // 減速効果
}

void EnemyDeathEffect::InitializeElectricEmitter()
{
    // 電撃エミッターの初期化
    electricEmitter_ = std::make_unique<ParticleEmitter>();
    electricEmitter_->Initialize("electric", electricTexturePath_);

    // 基本パラメーター設定
    electricEmitter_->SetEmitRate(0.01f);
    electricEmitter_->SetInitialLifeTime(0.4f);
    electricEmitter_->SetInitialScale(Vector3(0.25f, 0.25f, 0.25f));
    electricEmitter_->SetInitialColor(Vector4(0.3f, 0.6f, 1.0f, 0.9f));
    electricEmitter_->SetEmitRange(Vector3(-0.4f, -0.2f, -0.4f), Vector3(0.4f, 0.8f, 0.4f));
    electricEmitter_->SetBillborad(true);

    // ランダム設定
    electricEmitter_->SetRandomVelocity(true);
    electricEmitter_->SetRandomScale(true);
    electricEmitter_->SetRandomColor(true);

    // 範囲設定
    electricEmitter_->SetRandomVelocityRange(AABB(Vector3(-3.0f, -1.0f, -3.0f), Vector3(3.0f, 3.0f, 3.0f)));
    electricEmitter_->SetRandomScaleRange(AABB(Vector3(0.1f, 0.1f, 0.1f), Vector3(0.4f, 0.4f, 0.4f)));
    electricEmitter_->SetRandomColorRange(
        Vector4(0.2f, 0.5f, 1.0f, 0.7f),
        Vector4(0.5f, 0.8f, 1.0f, 1.0f)
    );

    // コンポーネント設定
    electricEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    electricEmitter_->AddComponent(std::make_shared<DragComponent>(0.85f)); // 電撃特有の減速
}

void EnemyDeathEffect::InitializeDissolveEmitter()
{
    // 消滅エミッターの初期化
    dissolveEmitter_ = std::make_unique<ParticleEmitter>();
    dissolveEmitter_->Initialize("dissolve", dissolveTexturePath_);

    // 基本パラメーター設定
    dissolveEmitter_->SetEmitRate(0.01f);
    dissolveEmitter_->SetInitialLifeTime(2.0f);
    dissolveEmitter_->SetInitialScale(Vector3(0.2f, 0.2f, 0.2f));
    dissolveEmitter_->SetInitialColor(Vector4(0.4f, 0.2f, 0.5f, 0.8f));
    dissolveEmitter_->SetEmitRange(Vector3(-0.8f, -0.5f, -0.8f), Vector3(0.8f, 1.5f, 0.8f));
    dissolveEmitter_->SetBillborad(true);

    // ランダム設定
    dissolveEmitter_->SetRandomVelocity(true);
    dissolveEmitter_->SetRandomScale(true);
    dissolveEmitter_->SetRandomColor(true);
    dissolveEmitter_->SetRandomRotation(true);

    // 範囲設定
    dissolveEmitter_->SetRandomVelocityRange(AABB(Vector3(-0.3f, 0.5f, -0.3f), Vector3(0.3f, 2.0f, 0.3f)));
    dissolveEmitter_->SetRandomScaleRange(AABB(Vector3(0.1f, 0.1f, 0.1f), Vector3(0.3f, 0.3f, 0.3f)));
    dissolveEmitter_->SetRandomColorRange(
        Vector4(0.3f, 0.1f, 0.4f, 0.6f),
        Vector4(0.6f, 0.3f, 0.7f, 0.9f)
    );
    dissolveEmitter_->SetRandomRotationRange(AABB(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 6.28f, 0.0f)));

    // コンポーネント設定
    dissolveEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    dissolveEmitter_->AddComponent(std::make_shared<DragComponent>(0.99f)); // ゆっくりした減速
    dissolveEmitter_->AddComponent(std::make_shared<AccelerationComponent>(Vector3(0.0f, 0.2f, 0.0f))); // 上昇
}

void EnemyDeathEffect::InitializeSmokeEmitter()
{
    // 煙エミッターの初期化
    smokeEmitter_ = std::make_unique<ParticleEmitter>();
    smokeEmitter_->Initialize("smoke", smokeTexturePath_);

    // 基本パラメーター設定
    smokeEmitter_->SetEmitRate(0.02f);
    smokeEmitter_->SetInitialLifeTime(1.8f);
    smokeEmitter_->SetInitialScale(Vector3(0.4f, 0.4f, 0.4f));
    smokeEmitter_->SetInitialColor(Vector4(0.3f, 0.3f, 0.3f, 0.6f));
    smokeEmitter_->SetEmitRange(Vector3(-0.5f, 0.0f, -0.5f), Vector3(0.5f, 0.8f, 0.5f));
    smokeEmitter_->SetBillborad(true);

    // ランダム設定
    smokeEmitter_->SetRandomVelocity(true);
    smokeEmitter_->SetRandomScale(true);
    smokeEmitter_->SetRandomRotation(true);

    // 範囲設定
    smokeEmitter_->SetRandomVelocityRange(AABB(Vector3(-0.5f, 0.5f, -0.5f), Vector3(0.5f, 2.0f, 0.5f)));
    smokeEmitter_->SetRandomScaleRange(AABB(Vector3(0.3f, 0.3f, 0.3f), Vector3(0.8f, 0.8f, 0.8f)));
    smokeEmitter_->SetRandomRotationRange(AABB(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 6.28f, 0.0f)));

    // コンポーネント設定
    smokeEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    smokeEmitter_->AddComponent(std::make_shared<AccelerationComponent>(Vector3(0.0f, 0.1f, 0.0f))); // ゆっくり上昇
    smokeEmitter_->AddComponent(std::make_shared<DragComponent>(0.98f)); // ゆっくりした減速
}