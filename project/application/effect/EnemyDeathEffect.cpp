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
    // エフェクトタイプに応じた適切な演出を選択
    switch (type)
    {
    case EffectType::Normal:
        // 通常死亡: 血飛沫、破片、煙を組み合わせたリアルな演出
        bloodEmitter_->Start(position, 30, 0.2f);
        fragmentEmitter_->Start(position, 15, 0.1f);
        smokeEmitter_->Start(position, 8, 0.3f);
        break;

    case EffectType::Explosive:
        PlayExplosionEffect(position);
        break;

    case EffectType::Electric:
        PlayElectricEffect(position);
        break;

    case EffectType::Dissolve:
        PlayDissolveEffect(position);
        break;
    }
}

void EnemyDeathEffect::PlayExplosionEffect(const Vector3& position, float scale)
{
    // 指定されたスケールで爆発エミッターのサイズを調整
    explosionEmitter_->SetInitialScale(Vector3(scale, scale, scale));

    // 連鎖的な爆発演出の構成
    explosionEmitter_->Start(position, 25, 0.1f);     // メイン爆発
    fragmentEmitter_->Start(position, 40, 0.15f);     // 大量の破片を放出
    smokeEmitter_->Start(position, 15, 1.0f);         // 長時間残る煙

    // 少し上の位置で二次爆発を発生させ、立体的な演出を実現
    Vector3 secondaryPos = position;
    secondaryPos.y += 0.5f;
    explosionEmitter_->Start(secondaryPos, 10, 0.05f, false);
}

void EnemyDeathEffect::PlayElectricEffect(const Vector3& position)
{
    electricEmitter_->Start(position, 35, 0.2f);

    // 電撃ダメージによる副次的な物理効果
    fragmentEmitter_->Start(position, 8, 0.1f);
    smokeEmitter_->Start(position, 5, 0.3f);

    // 複数の放電ポイントから分岐する電撃を表現（稲妻のような見た目）
    Vector3 offset1(1.0f, 0.2f, 0.5f);
    Vector3 offset2(-0.8f, 0.0f, -0.3f);
    Vector3 offset3(0.3f, 0.5f, -0.7f);

    electricEmitter_->Start(position + offset1, 10, 0.1f, false);
    electricEmitter_->Start(position + offset2, 10, 0.1f, false);
    electricEmitter_->Start(position + offset3, 10, 0.1f, false);
}

void EnemyDeathEffect::PlayDissolveEffect(const Vector3& position)
{
    // 長時間かけてゆっくりと消滅する演出
    dissolveEmitter_->Start(position, 60, 1.5f);

    // 上昇しながら消えるエフェクトを補助
    smokeEmitter_->Start(position, 12, 1.0f);
}

void EnemyDeathEffect::InitializeBloodEmitter()
{
    bloodEmitter_ = std::make_unique<ParticleEmitter>();
    bloodEmitter_->Initialize("enemy_blood", bloodTexturePath_);

    // 血飛沫の基本設定（赤色の液体表現）
    bloodEmitter_->SetEmitRate(0.01f);
    bloodEmitter_->SetInitialLifeTime(1.2f);
    bloodEmitter_->SetInitialScale(Vector3(0.2f, 0.2f, 0.2f));
    bloodEmitter_->SetInitialColor(Vector4(0.7f, 0.0f, 0.0f, 0.9f));
    bloodEmitter_->SetEmitRange(Vector3(-0.5f, 0.0f, -0.5f), Vector3(0.5f, 1.0f, 0.5f));
    bloodEmitter_->SetBillborad(true);

    bloodEmitter_->SetRandomVelocity(true);
    bloodEmitter_->SetRandomScale(true);
    bloodEmitter_->SetRandomColor(true);

    // 飛び散る血飛沫の速度範囲（上方向が強め）
    bloodEmitter_->SetRandomVelocityRange(AABB(Vector3(-3.0f, 2.0f, -3.0f), Vector3(3.0f, 5.0f, 3.0f)));
    bloodEmitter_->SetRandomScaleRange(AABB(Vector3(0.15f, 0.15f, 0.15f), Vector3(0.4f, 0.4f, 0.4f)));
    bloodEmitter_->SetRandomColorRange(
        Vector4(0.6f, 0.0f, 0.0f, 0.8f),
        Vector4(0.9f, 0.1f, 0.1f, 1.0f)
    );

    // 物理挙動の追加（重力、空気抵抗、地面バウンド）
    bloodEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    bloodEmitter_->AddComponent(std::make_shared<AccelerationComponent>(Vector3(0.0f, -9.8f, 0.0f)));
    bloodEmitter_->AddComponent(std::make_shared<DragComponent>(0.97f));
    bloodEmitter_->AddComponent(std::make_shared<BounceComponent>(0.0f, 0.3f, 0.1f));
}

void EnemyDeathEffect::InitializeFragmentEmitter()
{
    fragmentEmitter_ = std::make_unique<ParticleEmitter>();
    fragmentEmitter_->Initialize("enemy_fragment", fragmentTexturePath_);

    // 破片の基本設定（固い物質の破片を表現）
    fragmentEmitter_->SetEmitRate(0.01f);
    fragmentEmitter_->SetInitialLifeTime(1.5f);
    fragmentEmitter_->SetInitialScale(Vector3(0.15f, 0.15f, 0.15f));
    fragmentEmitter_->SetInitialColor(Vector4(0.6f, 0.6f, 0.6f, 1.0f));
    fragmentEmitter_->SetEmitRange(Vector3(-0.3f, 0.0f, -0.3f), Vector3(0.3f, 1.5f, 0.3f));
    fragmentEmitter_->SetBillborad(true);

    fragmentEmitter_->SetRandomVelocity(true);
    fragmentEmitter_->SetRandomScale(true);
    fragmentEmitter_->SetRandomRotation(true);

    // 破片の放出速度（上方向が強く、回転も激しい）
    fragmentEmitter_->SetRandomVelocityRange(AABB(Vector3(-4.0f, 3.0f, -4.0f), Vector3(4.0f, 6.0f, 4.0f)));
    fragmentEmitter_->SetRandomScaleRange(AABB(Vector3(0.1f, 0.1f, 0.1f), Vector3(0.25f, 0.25f, 0.25f)));
    fragmentEmitter_->SetRandomRotationRange(AABB(Vector3(-3.14f, -3.14f, -3.14f), Vector3(3.14f, 3.14f, 3.14f)));

    // 物理挙動（血飛沫より強い重力、高い反発力）
    fragmentEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    fragmentEmitter_->AddComponent(std::make_shared<AccelerationComponent>(Vector3(0.0f, -12.0f, 0.0f)));
    fragmentEmitter_->AddComponent(std::make_shared<DragComponent>(0.98f));
    fragmentEmitter_->AddComponent(std::make_shared<BounceComponent>(0.0f, 0.4f, 0.2f));
}

void EnemyDeathEffect::InitializeExplosionEmitter()
{
    explosionEmitter_ = std::make_unique<ParticleEmitter>();
    explosionEmitter_->Initialize("explosion", explosionTexturePath_);

    // 爆発の基本設定（オレンジ色の炎を表現）
    explosionEmitter_->SetEmitRate(0.005f);
    explosionEmitter_->SetInitialLifeTime(0.8f);
    explosionEmitter_->SetInitialScale(Vector3(0.5f, 0.5f, 0.5f));
    explosionEmitter_->SetInitialColor(Vector4(1.0f, 0.7f, 0.2f, 0.9f));
    explosionEmitter_->SetEmitRange(Vector3(-0.2f, 0.0f, -0.2f), Vector3(0.2f, 0.4f, 0.2f));
    explosionEmitter_->SetBillborad(true);

    explosionEmitter_->SetRandomVelocity(true);
    explosionEmitter_->SetRandomScale(true);
    explosionEmitter_->SetRandomColor(true);

    // 爆発の放射状の広がり
    explosionEmitter_->SetRandomVelocityRange(AABB(Vector3(-2.0f, 0.5f, -2.0f), Vector3(2.0f, 4.0f, 2.0f)));
    explosionEmitter_->SetRandomScaleRange(AABB(Vector3(0.3f, 0.3f, 0.3f), Vector3(1.0f, 1.0f, 1.0f)));
    explosionEmitter_->SetRandomColorRange(
        Vector4(0.9f, 0.4f, 0.0f, 0.8f),
        Vector4(1.0f, 0.8f, 0.3f, 1.0f)
    );

    // 炎の減衰（空気抵抗のみ、重力なし）
    explosionEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    explosionEmitter_->AddComponent(std::make_shared<DragComponent>(0.9f));
}

void EnemyDeathEffect::InitializeElectricEmitter()
{
    electricEmitter_ = std::make_unique<ParticleEmitter>();
    electricEmitter_->Initialize("electric", electricTexturePath_);

    // 電撃の基本設定（青白い放電を表現）
    electricEmitter_->SetEmitRate(0.01f);
    electricEmitter_->SetInitialLifeTime(0.4f);  // 短命で激しい点滅を表現
    electricEmitter_->SetInitialScale(Vector3(0.25f, 0.25f, 0.25f));
    electricEmitter_->SetInitialColor(Vector4(0.3f, 0.6f, 1.0f, 0.9f));
    electricEmitter_->SetEmitRange(Vector3(-0.4f, -0.2f, -0.4f), Vector3(0.4f, 0.8f, 0.4f));
    electricEmitter_->SetBillborad(true);

    electricEmitter_->SetRandomVelocity(true);
    electricEmitter_->SetRandomScale(true);
    electricEmitter_->SetRandomColor(true);

    // 電撃の激しく不規則な動き
    electricEmitter_->SetRandomVelocityRange(AABB(Vector3(-3.0f, -1.0f, -3.0f), Vector3(3.0f, 3.0f, 3.0f)));
    electricEmitter_->SetRandomScaleRange(AABB(Vector3(0.1f, 0.1f, 0.1f), Vector3(0.4f, 0.4f, 0.4f)));
    electricEmitter_->SetRandomColorRange(
        Vector4(0.2f, 0.5f, 1.0f, 0.7f),
        Vector4(0.5f, 0.8f, 1.0f, 1.0f)
    );

    // 電撃特有の急速な減衰
    electricEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    electricEmitter_->AddComponent(std::make_shared<DragComponent>(0.85f));
}

void EnemyDeathEffect::InitializeDissolveEmitter()
{
    dissolveEmitter_ = std::make_unique<ParticleEmitter>();
    dissolveEmitter_->Initialize("dissolve", dissolveTexturePath_);

    // 消滅の基本設定（紫色の神秘的な粒子を表現）
    dissolveEmitter_->SetEmitRate(0.01f);
    dissolveEmitter_->SetInitialLifeTime(2.0f);  // 長時間かけてゆっくり消える
    dissolveEmitter_->SetInitialScale(Vector3(0.2f, 0.2f, 0.2f));
    dissolveEmitter_->SetInitialColor(Vector4(0.4f, 0.2f, 0.5f, 0.8f));
    dissolveEmitter_->SetEmitRange(Vector3(-0.8f, -0.5f, -0.8f), Vector3(0.8f, 1.5f, 0.8f));
    dissolveEmitter_->SetBillborad(true);

    dissolveEmitter_->SetRandomVelocity(true);
    dissolveEmitter_->SetRandomScale(true);
    dissolveEmitter_->SetRandomColor(true);
    dissolveEmitter_->SetRandomRotation(true);

    // ゆっくりと上昇しながら消える動き
    dissolveEmitter_->SetRandomVelocityRange(AABB(Vector3(-0.3f, 0.5f, -0.3f), Vector3(0.3f, 2.0f, 0.3f)));
    dissolveEmitter_->SetRandomScaleRange(AABB(Vector3(0.1f, 0.1f, 0.1f), Vector3(0.3f, 0.3f, 0.3f)));
    dissolveEmitter_->SetRandomColorRange(
        Vector4(0.3f, 0.1f, 0.4f, 0.6f),
        Vector4(0.6f, 0.3f, 0.7f, 0.9f)
    );
    dissolveEmitter_->SetRandomRotationRange(AABB(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 6.28f, 0.0f)));

    // 緩やかな減速と上昇
    dissolveEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    dissolveEmitter_->AddComponent(std::make_shared<DragComponent>(0.99f));
    dissolveEmitter_->AddComponent(std::make_shared<AccelerationComponent>(Vector3(0.0f, 0.2f, 0.0f)));
}

void EnemyDeathEffect::InitializeSmokeEmitter()
{
    smokeEmitter_ = std::make_unique<ParticleEmitter>();
    smokeEmitter_->Initialize("smoke", smokeTexturePath_);

    // 煙の基本設定（灰色の煙を表現）
    smokeEmitter_->SetEmitRate(0.02f);
    smokeEmitter_->SetInitialLifeTime(1.8f);
    smokeEmitter_->SetInitialScale(Vector3(0.4f, 0.4f, 0.4f));
    smokeEmitter_->SetInitialColor(Vector4(0.3f, 0.3f, 0.3f, 0.6f));
    smokeEmitter_->SetEmitRange(Vector3(-0.5f, 0.0f, -0.5f), Vector3(0.5f, 0.8f, 0.5f));
    smokeEmitter_->SetBillborad(true);

    smokeEmitter_->SetRandomVelocity(true);
    smokeEmitter_->SetRandomScale(true);
    smokeEmitter_->SetRandomRotation(true);

    // ゆっくりと上昇する煙の動き
    smokeEmitter_->SetRandomVelocityRange(AABB(Vector3(-0.5f, 0.5f, -0.5f), Vector3(0.5f, 2.0f, 0.5f)));
    smokeEmitter_->SetRandomScaleRange(AABB(Vector3(0.3f, 0.3f, 0.3f), Vector3(0.8f, 0.8f, 0.8f)));
    smokeEmitter_->SetRandomRotationRange(AABB(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 6.28f, 0.0f)));

    // 煙特有のゆっくりした動き（上昇しながら徐々に消える）
    smokeEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    smokeEmitter_->AddComponent(std::make_shared<AccelerationComponent>(Vector3(0.0f, 0.1f, 0.0f)));
    smokeEmitter_->AddComponent(std::make_shared<DragComponent>(0.98f));
}