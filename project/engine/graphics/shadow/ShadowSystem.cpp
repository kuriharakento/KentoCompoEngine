#include "ShadowSystem.h"
#include "base/Logger.h"

void ShadowSystem::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, LightManager* lightManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    lightManager_ = lightManager;

    shadowMapManager_.Initialize(dxCommon, srvManager);
    shadowMapPipeline_.Initialize(dxCommon);

    isInitialized_ = true;
    Logger::Log("ShadowSystem initialized\n");
}

void ShadowSystem::CreateDirectionalLightShadow(uint32_t resolution) {
    if (!isInitialized_) return;
    shadowMapManager_.CreateDirectionalLightShadowMap(resolution);
}

void ShadowSystem::CreateSpotLightShadow(const std::string& name, uint32_t resolution) {
    if (!isInitialized_) return;
    shadowMapManager_.CreateSpotLightShadowMap(name, resolution);
    lightManager_->SetSpotLightShadowEnabled(name, true);
}

void ShadowSystem::CreatePointLightShadow(const std::string& name, uint32_t resolution) {
    if (!isInitialized_) return;
    shadowMapManager_.CreatePointLightShadowMap(name, resolution);
    lightManager_->SetPointLightShadowEnabled(name, true);
}

void ShadowSystem::Update(const Vector3& targetCenter, float shadowMapSize) {
    if (!isInitialized_ || !lightManager_) return;

    // ディレクショナルライトのシャドウ行列更新
    if (shadowMapManager_.HasDirectionalLightShadowMap()) {
        lightManager_->UpdateDirectionalLightShadowMatrix(targetCenter, shadowMapSize, 0.1f, 100.0f);
    }

    // スポットライトのシャドウ行列更新
    for (const auto& [name, light] : lightManager_->GetSpotLights()) {
        if (shadowMapManager_.HasSpotLightShadowMap(name)) {
            lightManager_->UpdateSpotLightShadowMatrix(name, 0.1f, light.gpuData.distance);
        }
    }

    // ポイントライトのシャドウ行列更新
    for (const auto& [name, light] : lightManager_->GetPointLights()) {
        if (shadowMapManager_.HasPointLightShadowMap(name)) {
            lightManager_->UpdatePointLightShadowMatrix(name, 0.1f, light.gpuData.radius);
        }
    }
}

void ShadowSystem::RenderShadowPass(const std::vector<Object3d*>& objects) {
    if (!isInitialized_) return;

    // ディレクショナルライトシャドウパス
    if (shadowMapManager_.HasDirectionalLightShadowMap()) {
        shadowMapManager_.BeginDirectionalLightShadowPass();
        shadowMapPipeline_.SetPipeline();

        for (auto* obj : objects) {
            if (obj) {
                obj->DrawShadow(lightManager_->GetShadowMatrixGPUAddress());
            }
        }

        shadowMapManager_.EndShadowPass();
    }

    // スポットライトシャドウパス
    for (const auto& [name, light] : lightManager_->GetSpotLights()) {
        if (light.shadowEnabled && shadowMapManager_.HasSpotLightShadowMap(name)) {
            shadowMapManager_.BeginSpotLightShadowPass(name);
            shadowMapPipeline_.SetPipeline();

            // スポットライトのビュープロジェクション行列を作成して渡す
            // 注意: 現在の実装では定数バッファを別途作成する必要がある
            // 簡略化のため、ディレクショナルライトと同様の方法を使用

            for (auto* obj : objects) {
                if (obj) {
                    // TODO: スポットライト用の定数バッファを使用
                    // obj->DrawShadow(spotLightMatrixGPUAddress);
                }
            }

            shadowMapManager_.EndShadowPass();
        }
    }

    // ポイントライトシャドウパス（6面レンダリング）
    for (const auto& [name, light] : lightManager_->GetPointLights()) {
        if (light.shadowEnabled && shadowMapManager_.HasPointLightShadowMap(name)) {
            for (uint32_t face = 0; face < 6; ++face) {
                shadowMapManager_.BeginPointLightShadowPass(name, face);
                shadowMapPipeline_.SetPipeline();

                for (auto* obj : objects) {
                    if (obj) {
                        // TODO: ポイントライト用の定数バッファを使用
                        // const Matrix4x4& matrix = lightManager_->GetPointLightShadowMatrix(name, face);
                    }
                }

                // 最後の面以外はパスを終了しない（リソースバリアの効率化）
                if (face == 5) {
                    shadowMapManager_.EndShadowPass();
                }
            }
        }
    }
}

void ShadowSystem::ApplyShadowToObject(Object3d* object) {
    if (!isInitialized_ || !object) return;

    // ディレクショナルライトシャドウマップを適用
    if (shadowMapManager_.HasDirectionalLightShadowMap()) {
        object->SetShadowMap(
            srvManager_,
            shadowMapManager_.GetDirectionalLightShadowMap().srvIndex,
            lightManager_->GetShadowMatrixGPUAddress()
        );
    }
}

void ShadowSystem::Finalize() {
    shadowMapManager_.Clear();
    isInitialized_ = false;
}
