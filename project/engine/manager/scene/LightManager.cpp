#include "LightManager.h"

#include <numbers>
#include "DirectXTex/d3dx12.h"
// system
#include "base/Logger.h"
// math
#include "math/Easing.h"
// editor
#include "externals/imgui/imgui.h"

LightManager::LightManager()
{
	//ライトの数を初期化
	lightCount_.pointLightCount = 0;
	lightCount_.spotLightCount = 0;
}

LightManager::~LightManager()
{
	if (lightCountResource_)
	{
		lightCountResource_->Unmap(0, nullptr);
	}
	if (pointLightResource_)
	{
		pointLightResource_->Unmap(0, nullptr);
	}
	if (spotLightResource_)
	{
		spotLightResource_->Unmap(0, nullptr);
	}
}

void LightManager::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
	//定数バッファの作成
	CreateConstantBuffer();

	//イージング関数の設定
	pEasingFunc_ = EaseInSine<float>;

	//ポイントライトの追加
	AddPointLight("pointLight" + std::to_string(pointLights_.size()));

	//スポットライトの追加
	AddSpotLight("spotLight" + std::to_string(spotLights_.size()));

	//グラデーションしてみる
	StartGradient("pointLight0", startPointLightColor_, endPointLightColor_, duration_, pEasingFunc_);
	StartGradient("spotLight0", startSpotLightColor_, endSpotLightColor_, duration_, pEasingFunc_);

}

void LightManager::Update()
{
	//ImGuiの表示
	ImGuiUpdate();

	// フレーム間の経過時間を取得（例として固定値を使用）
	float deltaTime = 1.0f / 60.0f;  // 60FPSの場合

	// ポイントライトの更新
	for (auto& [name, light] : pointLights_) {
		if (light.isGradientActive) {
			// 経過時間を更新
			light.elapsedTime += deltaTime;
			if (light.elapsedTime > light.duration) {
				light.elapsedTime = 0.0f;
				light.isReversing = !light.isReversing;  // 補間の方向を反転
			}
			// 補間係数の計算
			float t = light.elapsedTime / light.duration;
			t = light.easingFunction(t);  // イージング関数を適用
			// 色の補間
			if (light.isReversing) {
				light.gpuData.color = Vector4::Lerp(light.endColor, light.startColor, t);
			} else {
				light.gpuData.color = Vector4::Lerp(light.startColor, light.endColor, t);
			}
		}
	}

	// スポットライトの更新
	for (auto& [name, light] : spotLights_) {
		if (light.isGradientActive) {
			// 経過時間を更新
			light.elapsedTime += deltaTime;
			if (light.elapsedTime > light.duration) {
				light.elapsedTime = 0.0f;
				light.isReversing = !light.isReversing;  // 補間の方向を反転
			}
			// 補間係数の計算
			float t = light.elapsedTime / light.duration;
			t = light.easingFunction(t);  // イージング関数を適用
			// 色の補間
			if (light.isReversing) {
				light.gpuData.color = Vector4::Lerp(light.endColor, light.startColor, t);
			} else {
				light.gpuData.color = Vector4::Lerp(light.startColor, light.endColor, t);
			}
		}
	}

	// GPUに送るデータを更新
	uint32_t pointLightIndex = 0;
	for (const auto& [name, light] : pointLights_) {
		pointLightData_[pointLightIndex++] = light.gpuData;
	}

	uint32_t spotLightIndex = 0;
	for (const auto& [name, light] : spotLights_) {
		spotLightData_[spotLightIndex++] = light.gpuData;
	}

	lightCount_.pointLightCount = pointLightIndex;
	lightCount_.spotLightCount = spotLightIndex;

	// 定数バッファにライトの数を更新
	lightCountData_->pointLightCount = lightCount_.pointLightCount;
	lightCountData_->spotLightCount = lightCount_.spotLightCount;

}

void LightManager::Draw()
{
	//ポイントライトのCBVを設定
	dxCommon_->GetCommandList()->SetGraphicsRootShaderResourceView(5, pointLightResource_->GetGPUVirtualAddress());
	//スポットライトのCBVを設定
	dxCommon_->GetCommandList()->SetGraphicsRootShaderResourceView(6, spotLightResource_->GetGPUVirtualAddress());
	//ライトの数のCBVを設定
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(7, lightCountResource_->GetGPUVirtualAddress());
}

void LightManager::AddPointLight(const std::string& name)
{
	//最大個数に達している場合は追加しない
	if (pointLights_.size() >= LightMaxCount::kMaxPointLightCount)
	{
		Logger::Log("ポイントライトの最大数に達しているため追加できません\n");
		return;
	}

	//ポイントライトを作成と初期化
	GPUPointLight pointLight;
	pointLight.color = { 1.0f,1.0f,1.0f,1.0f };
	pointLight.position = { 0.0f,2.0f,0.0f };
	pointLight.intensity = 1.0f;
	pointLight.radius = 3.0f;
	pointLight.decay = 1.0f;
	pointLights_.emplace(name, pointLight);
	//名前を保存
	pointLightNames_.push_back(name);
	//ライトの数をインクリメント
	++lightCount_.pointLightCount;
}

void LightManager::AddSpotLight(const std::string& name)
{
	//最大個数に達している場合は追加しない
	if (spotLights_.size() >= LightMaxCount::kMaxSpotLightCount)
	{
		Logger::Log("スポットライトの最大数に達しているため追加できません\n");
		return;
	}

	// スポットライトを作成と初期化
    CPUSpotLight spotLight;
    spotLight.gpuData.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    spotLight.gpuData.position = { 0.0f, 1.0f, 0.0f };
    spotLight.gpuData.distance = 7.0f;
    spotLight.gpuData.intensity = 4.0f;
    spotLight.gpuData.direction = Vector3::Normalize({ 0.0f, -1.0f, 1.0f });
    spotLight.gpuData.cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);
    spotLight.gpuData.decay = 2.0f;
    spotLight.gpuData.cosFalloffStart = 1.0f;
	spotLight.isGradientActive = false;

    // シャドウマップ用のリソースを初期化
    spotLight.InitializeShadowMap(dxCommon_->GetDevice());
	//リストに追加
	spotLights_.emplace(name, spotLight);
	//名前を保存
	spotLightNames_.push_back(name);
	//ライトの数をインクリメント
	++lightCount_.spotLightCount;
}

void LightManager::Clear()
{
	pointLights_.clear();
	spotLights_.clear();
	pointLightNames_.clear();
	spotLightNames_.clear();
	lightCount_.pointLightCount = 0;
	lightCount_.spotLightCount = 0;
}

void LightManager::StartGradient(const std::string& name, const Vector4& startColor, const Vector4& endColor, float duration, std::function<float(float)> easingFunction)
{
	if (pointLights_.find(name) != pointLights_.end()) {
		auto& light = pointLights_.at(name);
		light.startColor = startColor;
		light.endColor = endColor;
		light.duration = duration;
		light.elapsedTime = 0.0f;
		light.isReversing = false;
		light.isGradientActive = true;
		light.easingFunction = easingFunction;
	} else if (spotLights_.find(name) != spotLights_.end()) {
		auto& light = spotLights_.at(name);
		light.startColor = startColor;
		light.endColor = endColor;
		light.duration = duration;
		light.elapsedTime = 0.0f;
		light.isReversing = false;
		light.isGradientActive = true;
		light.easingFunction = easingFunction;
	} else {
		Logger::Log("ライトが見つかりません: " + name);
	}
}

void LightManager::CreateConstantBuffer()
{
	/*--------------[ ライトの数リソースを作る ]-----------------*/

	lightCountResource_ = dxCommon_->CreateBufferResource(sizeof(LightCount));

	/*--------------[ ライトの数リソースにデータを書き込むためのアドレスを取得してlightCountDataに割り当てる ]-----------------*/

	lightCountResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&lightCountData_)
	);

	//デフォルト値は以下のようにしておく
	lightCountData_->pointLightCount = 0;
	lightCountData_->spotLightCount = 0;

	/*--------------[ ポイントライトリソースを作る ]-----------------*/

	pointLightResource_ = dxCommon_->CreateBufferResource(sizeof(GPUPointLight) * LightMaxCount::kMaxPointLightCount);

	/*--------------[ ポイントライトリソースにデータを書き込むためのアドレスを取得してpointLightDataに割り当てる ]-----------------*/

	pointLightResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&pointLightData_)
	);

	//デフォルト値は以下のようにしておく
	pointLightData_->color = { 1.0f,1.0f,1.0f,1.0f };
	pointLightData_->position = { 0.0f,0.0f,0.0f };
	pointLightData_->intensity = 1.0f;
	pointLightData_->radius = 3.0f;

	/*--------------[ スポットライトリソースを作る ]-----------------*/

	spotLightResource_ = dxCommon_->CreateBufferResource(sizeof(GPUSpotLight) * LightMaxCount::kMaxSpotLightCount);

	/*--------------[ スポットライトリソースにデータを書き込むためのアドレスを取得してspotLightDataに割り当てる ]-----------------*/

	spotLightResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&spotLightData_)
	);

	//デフォルト値は以下のようにしておく
	spotLightData_->color = { 1.0f,1.0f,1.0f,1.0f };
	spotLightData_->position = { 0.0f,0.0f,0.0f };
	spotLightData_->intensity = 1.0f;
	spotLightData_->distance = 3.0f;
	spotLightData_->decay = 1.0f;
	spotLightData_->direction = { 0.0f,-1.0f,0.0f };
	spotLightData_->cosAngle = 0.0f;
	spotLightData_->cosFalloffStart = 0.0f;
}

void LightManager::ImGuiUpdate()
{
#ifdef _DEBUG
	ImGui::Begin("LightManager");

	if (ImGui::BeginTabBar("LightTabs"))
	{
		if (ImGui::BeginTabItem("Light Options"))
		{
			// イージング関数の選択肢
			const char* easingOptions[] = {
				"EaseInSine",
				"EaseOutSine",
				"EaseInOutSine",
				"EaseInQuint",
				"EaseOutQuint",
				"EaseInOutQuint",
				"EaseInCirc",
				"EaseOutCirc",
				"EaseInOutCirc",
				"EaseInElastic",
				"EaseOutElastic",
				"EaseInOutElastic",
				"EaseInExpo",
				"EaseOutQuad",
				"EaseInOutQuart",
				"EaseInBack",
				"EaseOutBack",
				"EaseInOutBack",
				"EaseInBounce",
				"EaseOutBounce",
				"EaseInOutBounce"
			};
			static int currentEasingIndex = 0;
			//イージングの時間
			ImGui::DragFloat("Easing Time", &duration_, 0.1f, 0.0f, 10.0f);
			// イージング関数の選択
			if (ImGui::Combo("Easing Function", &currentEasingIndex, easingOptions, IM_ARRAYSIZE(easingOptions))) {
				switch (currentEasingIndex) {
				case 0: pEasingFunc_ = EaseInSine<float>; break;
				case 1: pEasingFunc_ = EaseOutSine<float>; break;
				case 2: pEasingFunc_ = EaseInOutSine<float>; break;
				case 3: pEasingFunc_ = EaseInQuint<float>; break;
				case 4: pEasingFunc_ = EaseOutQuint<float>; break;
				case 5: pEasingFunc_ = EaseInOutQuint<float>; break;
				case 6: pEasingFunc_ = EaseInCirc<float>; break;
				case 7: pEasingFunc_ = EaseOutCirc<float>; break;
				case 8: pEasingFunc_ = EaseInOutCirc<float>; break;
				case 9: pEasingFunc_ = EaseInElastic<float>; break;
				case 10: pEasingFunc_ = EaseOutElastic<float>; break;
				case 11: pEasingFunc_ = EaseInOutElastic<float>; break;
				case 12: pEasingFunc_ = EaseInExpo<float>; break;
				case 13: pEasingFunc_ = EaseOutQuad<float>; break;
				case 14: pEasingFunc_ = EaseInOutQuart<float>; break;
				case 15: pEasingFunc_ = EaseInBack<float>; break;
				case 16: pEasingFunc_ = EaseOutBack<float>; break;
				case 17: pEasingFunc_ = EaseInOutBack<float>; break;
				case 18: pEasingFunc_ = EaseInBounce<float>; break;
				case 19: pEasingFunc_ = EaseOutBounce<float>; break;
				case 20: pEasingFunc_ = EaseInOutBounce<float>; break;
				}
			}
			ImGui::SeparatorText("List Clear");
			if (ImGui::Button("clear"))
			{
				Clear();
			}
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Point Lights"))
		{
			ImGui::SeparatorText("List Options");
			ImGui::Text("PointLight Count : %d", lightCount_.pointLightCount);
			if (ImGui::Button("Add PointLight"))
			{
				AddPointLight("PointLight" + std::to_string(pointLights_.size()));
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear PointLights"))
			{
				pointLights_.clear();
				pointLightNames_.clear();
				lightCount_.pointLightCount = 0;
			}

			ImGui::SeparatorText("Gradient");
			if (ImGui::Button("Start Gradient"))
			{
				//すべてのポイントライトにグラデーションを適用
				for (const auto& name : pointLightNames_)
				{
					StartGradient(name, startPointLightColor_, endPointLightColor_, duration_, pEasingFunc_);
				}
			}
			//開始色
			ImGui::ColorEdit4("Start Color", &startPointLightColor_.x);
			//終了色
			ImGui::ColorEdit4("End Color", &endPointLightColor_.x);

			ImGui::SeparatorText("List");
			// ポイントライトの設定
			for (const auto& name : pointLightNames_)
			{
				ImGui::PushID(name.c_str());
				if (ImGui::CollapsingHeader(name.c_str()))
				{
					ImGui::ColorEdit4("PointLight Color", &pointLights_.at(name).gpuData.color.x);
					ImGui::DragFloat3("PointLight Position", &pointLights_.at(name).gpuData.position.x, 0.1f);
					ImGui::DragFloat("PointLight Intensity", &pointLights_.at(name).gpuData.intensity, 0.1f, 0.0f,100.0f);
					ImGui::DragFloat("PointLight Radius", &pointLights_.at(name).gpuData.radius, 0.1f, 0.0f,1000.0f);
					ImGui::DragFloat("PointLight Decay", &pointLights_.at(name).gpuData.decay, 0.1f, 0.0f,10.0f);
				}
				ImGui::PopID();
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Spot Lights"))
		{
			ImGui::SeparatorText("List Options");
			ImGui::Text("SpotLight Count : %d", lightCount_.spotLightCount);
			if (ImGui::Button("Add GPUSpotLight"))
			{
				AddSpotLight("SpotLight" + std::to_string(spotLights_.size()));
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear SpotLights"))
			{
				spotLights_.clear();
				spotLightNames_.clear();
				lightCount_.spotLightCount = 0;
			}

			ImGui::SeparatorText("Gradient");

			if (ImGui::Button("Start Gradient"))
			{
				//すべてのスポットライトにグラデーションを適用
				for (const auto& name : spotLightNames_)
				{
					StartGradient(name, startSpotLightColor_, endSpotLightColor_, duration_, pEasingFunc_);
				}
			}
			//開始色
			ImGui::ColorEdit4("Start Color", &startSpotLightColor_.x);
			//終了色
			ImGui::ColorEdit4("End Color", &endSpotLightColor_.x);

			ImGui::SeparatorText("List");
			// スポットライトの設定
			for (const auto& name : spotLightNames_)
			{
				ImGui::PushID(name.c_str());
				if (ImGui::CollapsingHeader(name.c_str()))
				{
					ImGui::ColorEdit4("SpotLight Color", &spotLights_.at(name).gpuData.color.x);
					ImGui::DragFloat3("SpotLight Position", &spotLights_.at(name).gpuData.position.x, 0.1f);
					ImGui::DragFloat3("SpotLight Direction", &spotLights_.at(name).gpuData.direction.x, 0.01f, -1.0f, 1.0f);
					ImGui::DragFloat("SpotLight Intensity", &spotLights_.at(name).gpuData.intensity, 0.1f, 0.0f,100.0f);
					ImGui::DragFloat("SpotLight Distance", &spotLights_.at(name).gpuData.distance, 0.1f, 0.0f,1000.0f);
					ImGui::DragFloat("SpotLight CosAngle", &spotLights_.at(name).gpuData.cosAngle, 0.01f, -3.14f, 3.14f);
					ImGui::DragFloat("SpotLight Decay", &spotLights_.at(name).gpuData.decay, 0.1f, 0.0f,10.0f);
					ImGui::DragFloat("SpotLight CosFalloffStart", &spotLights_.at(name).gpuData.cosFalloffStart, 0.01f, -3.14f, 3.14f);
				}
				ImGui::PopID();
			}

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
#endif

}

#pragma region Accessor
void LightManager::SetPointLightColor(const std::string& name, const Vector4& color)
{
	if (pointLights_.find(name) != pointLights_.end()) {
		pointLights_.at(name).gpuData.color = color;
	} else {
		Logger::Log("ポイントライトが見つかりません: " + name);
	}
}

void LightManager::SetPointLightPosition(const std::string& name, const Vector3& position)
{
	if (pointLights_.find(name) != pointLights_.end()) {
		pointLights_.at(name).gpuData.position = position;
	} else {
		Logger::Log("ポイントライトが見つかりません: " + name);
	}
}

void LightManager::SetPointLightIntensity(const std::string& name, float intensity)
{
	if (pointLights_.find(name) != pointLights_.end()) {
		pointLights_.at(name).gpuData.intensity = intensity;
	} else {
		Logger::Log("ポイントライトが見つかりません: " + name);
	}
}

void LightManager::SetPointLightRadius(const std::string& name, float radius)
{
	if (pointLights_.find(name) != pointLights_.end()) {
		pointLights_.at(name).gpuData.radius = radius;
	} else {
		Logger::Log("ポイントライトが見つかりません: " + name);
	}
}

void LightManager::SetPointLightDecay(const std::string& name, float decay)
{
	if (pointLights_.find(name) != pointLights_.end()) {
		pointLights_.at(name).gpuData.decay = decay;
	} else {
		Logger::Log("ポイントライトが見つかりません: " + name);
	}
}

void LightManager::SetSpotLightColor(const std::string& name, const Vector4& color)
{
	if (spotLights_.find(name) != spotLights_.end()) {
		spotLights_.at(name).gpuData.color = color;
	} else {
		Logger::Log("スポットライトが見つかりません: " + name);
	}
}

void LightManager::SetSpotLightPosition(const std::string& name, const Vector3& position)
{
	if (spotLights_.find(name) != spotLights_.end()) {
		spotLights_.at(name).gpuData.position = position;
	} else {
		Logger::Log("スポットライトが見つかりません: " + name);
	}
}

void LightManager::SetSpotLightIntensity(const std::string& name, float intensity)
{
	if (spotLights_.find(name) != spotLights_.end()) {
		spotLights_.at(name).gpuData.intensity = intensity;
	} else {
		Logger::Log("スポットライトが見つかりません: " + name);
	}
}

void LightManager::SetSpotLightDirection(const std::string& name, const Vector3& direction)
{
	if (spotLights_.find(name) != spotLights_.end()) {
		spotLights_.at(name).gpuData.direction = direction;
	} else {
		Logger::Log("スポットライトが見つかりません: " + name);
	}
}

void LightManager::SetSpotLightDistance(const std::string& name, float distance)
{
	if (spotLights_.find(name) != spotLights_.end()) {
		spotLights_.at(name).gpuData.distance = distance;
	} else {
		Logger::Log("スポットライトが見つかりません: " + name);
	}
}

void LightManager::SetSpotLightDecay(const std::string& name, float decay)
{
	if (spotLights_.find(name) != spotLights_.end()) {
		spotLights_.at(name).gpuData.decay = decay;
	} else {
		Logger::Log("スポットライトが見つかりません: " + name);
	}
}

void LightManager::SetSpotLightCosAngle(const std::string& name, float cosAngle)
{
	if (spotLights_.find(name) != spotLights_.end()) {
		spotLights_.at(name).gpuData.cosAngle = cosAngle;
	} else {
		Logger::Log("スポットライトが見つかりません: " + name);
	}
}

void LightManager::SetSpotLightCosFalloffStart(const std::string& name, float cosFalloffStart)
{
	if (spotLights_.find(name) != spotLights_.end()) {
		spotLights_.at(name).gpuData.cosFalloffStart = cosFalloffStart;
	} else {
		Logger::Log("スポットライトが見つかりません: " + name);
	}
}

const uint32_t& LightManager::GetPointLightCount() const
{
	return lightCount_.pointLightCount;
}

const uint32_t& LightManager::GetSpotLightCount() const
{
	return lightCount_.spotLightCount;
}

const GPUPointLight& LightManager::GetPointLight(const std::string& name) const
{
	if (pointLights_.find(name) != pointLights_.end()) {
		return pointLights_.at(name).gpuData;
	} else {
		Logger::Log("ポイントライトが見つかりません: " + name);
		return pointLights_.begin()->second.gpuData;
	}
}

const GPUSpotLight& LightManager::GetSpotLight(const std::string& name) const
{
	if (spotLights_.find(name) != spotLights_.end()) {
		return spotLights_.at(name).gpuData;
	} else {
		Logger::Log("スポットライトが見つかりません: " + name);
		return spotLights_.begin()->second.gpuData;
	}
}

const Vector4& LightManager::GetPointLightColor(const std::string& name) const
{
	if (pointLights_.find(name) != pointLights_.end()) {
		return pointLights_.at(name).gpuData.color;
	} else {
		Logger::Log("ポイントライトが見つかりません: " + name);
		return pointLights_.begin()->second.gpuData.color;
	}
}

const Vector3& LightManager::GetPointLightPosition(const std::string& name) const
{
	if (pointLights_.find(name) != pointLights_.end()) {
		return pointLights_.at(name).gpuData.position;
	} else {
		Logger::Log("ポイントライトが見つかりません: " + name);
		return pointLights_.begin()->second.gpuData.position;
	}
}

float LightManager::GetPointLightIntensity(const std::string& name) const
{
	if (pointLights_.find(name) != pointLights_.end()) {
		return pointLights_.at(name).gpuData.intensity;
	} else {
		Logger::Log("ポイントライトが見つかりません: " + name);
		return pointLights_.begin()->second.gpuData.intensity;
	}
}

float LightManager::GetPointLightRadius(const std::string& name) const
{
	if (pointLights_.find(name) != pointLights_.end()) {
		return pointLights_.at(name).gpuData.radius;
	} else {
		Logger::Log("ポイントライトが見つかりません: " + name);
		return pointLights_.begin()->second.gpuData.radius;
	}
}

float LightManager::GetPointLightDecay(const std::string& name) const
{
	if (pointLights_.find(name) != pointLights_.end()) {
		return pointLights_.at(name).gpuData.decay;
	} else {
		Logger::Log("ポイントライトが見つかりません: " + name);
		return pointLights_.begin()->second.gpuData.decay;
	}
}

const Vector4& LightManager::GetSpotLightColor(const std::string& name) const
{
	if (spotLights_.find(name) != spotLights_.end()) {
		return spotLights_.at(name).gpuData.color;
	} else {
		Logger::Log("スポットライトが見つかりません: " + name);
		return spotLights_.begin()->second.gpuData.color;
	}
}

const Vector3& LightManager::GetSpotLightPosition(const std::string& name) const
{
	if (spotLights_.find(name) != spotLights_.end()) {
		return spotLights_.at(name).gpuData.position;
	} else {
		Logger::Log("スポットライトが見つかりません: " + name);
		return spotLights_.begin()->second.gpuData.position;
	}
}

float LightManager::GetSpotLightIntensity(const std::string& name) const
{
	if (spotLights_.find(name) != spotLights_.end()) {
		return spotLights_.at(name).gpuData.intensity;
	} else {
		Logger::Log("スポットライトが見つかりません: " + name);
		return spotLights_.begin()->second.gpuData.intensity;
	}
}

const Vector3& LightManager::GetSpotLightDirection(const std::string& name) const
{
	if (spotLights_.find(name) != spotLights_.end()) {
		return spotLights_.at(name).gpuData.direction;
	} else {
		Logger::Log("スポットライトが見つかりません: " + name);
		return spotLights_.begin()->second.gpuData.direction;
	}
}

float LightManager::GetSpotLightDistance(const std::string& name) const
{
	if (spotLights_.find(name) != spotLights_.end()) {
		return spotLights_.at(name).gpuData.distance;
	} else {
		Logger::Log("スポットライトが見つかりません: " + name);
		return spotLights_.begin()->second.gpuData.distance;
	}
}

float LightManager::GetSpotLightDecay(const std::string& name) const
{
	if (spotLights_.find(name) != spotLights_.end()) {
		return spotLights_.at(name).gpuData.decay;
	} else {
		Logger::Log("スポットライトが見つかりません: " + name);
		return spotLights_.begin()->second.gpuData.decay;
	}
}

float LightManager::GetSpotLightCosAngle(const std::string& name) const
{
	if (spotLights_.find(name) != spotLights_.end()) {
		return spotLights_.at(name).gpuData.cosAngle;
	} else {
		Logger::Log("スポットライトが見つかりません: " + name);
		return spotLights_.begin()->second.gpuData.cosAngle;
	}
}

float LightManager::GetSpotLightCosFalloffStart(const std::string& name) const
{
	if (spotLights_.find(name) != spotLights_.end()) {
		return spotLights_.at(name).gpuData.cosFalloffStart;
	} else {
		Logger::Log("スポットライトが見つかりません: " + name);
		return spotLights_.begin()->second.gpuData.cosFalloffStart;
	}
}

#pragma endregion