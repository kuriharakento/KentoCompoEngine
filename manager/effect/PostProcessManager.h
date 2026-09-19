#pragma once
#include <memory>
#include <wrl.h>
#include <d3d12.h>
#include <string>

// effects
#include "effects/postprocess/BloomEffect.h"
#include "effects/postprocess/CRTEffect.h"
#include "effects/postprocess/ColorGradingEffect.h"
#include "effects/postprocess/DiffusionEffect.h"
#include "effects/postprocess/GaussianBlurEffect.h"
#include "effects/postprocess/GrayscaleEffect.h"
#include "effects/postprocess/NoiseEffect.h"
#include "effects/postprocess/RadialBlurEffect.h"
#include "effects/postprocess/TonemapEffect.h"
#include "effects/postprocess/VignetteEffect.h"

namespace KCE
{
class DirectXCommon;
class SrvManager;
class RenderTexture;

/**
 * @brief ポストプロセスマネージャークラス
 * @details 各種ポストプロセスエフェクトの管理と描画を行う
 *          Bloomエフェクトのマルチパス処理（明るい部分の抽出→ブラー→合成）に対応
 *          Grayscale、Vignette、Noise、CRTエフェクトもサポート
 */
class PostProcessManager
{
public:
    /**
     * @brief コンストラクタ
     */
    PostProcessManager();

    /**
     * @brief デストラクタ
     */
    ~PostProcessManager();

    /**
     * @brief 初期化処理
     * @param dxCommon DirectXCommonへのポインタ
     * @param srvManager SRVマネージャーへのポインタ
     * @param vsPath 頂点シェーダーのパス
     * @param psPath ピクセルシェーダーのパス
     */
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::wstring& vsPath, const std::wstring& psPath, uint32_t width, uint32_t height);

    /**
     * @brief 描画処理
     * @param inputTexture 入力テクスチャ
     * @details ブルームが有効な場合はマルチパス処理、それ以外はシングルパス処理を行う
     */
    void Draw(RenderTexture* inputTexture, RenderTexture* outputRT = nullptr, RenderTexture* selectiveBloomSource = nullptr);

    /**
     * @brief ブルームが有効か
     * @return 有効なら true
     */
    bool IsBloomEnabled() const { return bloomEffect_ && bloomEffect_->IsEnabled(); }

    /**
     * @brief 選択的ブルームが有効か
     * @return 有効なら true
     * @details 明るさで拾うのではなく、発光用のRTに書かれたものだけをぼかす。
     *          ブルーム自体が切れていれば false。
     */
    bool IsSelectiveBloomEnabled() const { return IsBloomEnabled() && selectiveBloomModeEnabled_; }

    /**
     * @brief 選択的ブルームの有効無効を切り替える
     * @param enabled 有効にするなら true
     */
    void SetSelectiveBloomModeEnabled(bool enabled) { selectiveBloomModeEnabled_ = enabled; }

    /**
     * @brief ブライトパスレンダリング
     * @param inputTexture 入力テクスチャ
     * @param outputRT 出力レンダーターゲット
     * @details 明るい部分（閾値以上）を抽出する
     */
    void RenderBrightPass(RenderTexture* inputTexture, RenderTexture* outputRT);

    /**
     * @brief ブラーパスレンダリング
     * @param inputTexture 入力テクスチャ
     * @param outputRT 出力レンダーターゲット
     * @param horizontal 水平方向ブラーの場合true、垂直方向の場合false
     */
    void RenderBlurPass(RenderTexture* inputTexture, RenderTexture* outputRT, bool horizontal);

    /**
     * @brief 最終合成レンダリング
     * @param sceneTexture シーンテクスチャ
     * @param bloomTexture ブルームテクスチャ
     * @details シーンとブルームを合成して最終出力を生成する
     */
    void RenderFinalComposite(RenderTexture* sceneTexture, RenderTexture* bloomTexture, RenderTexture* outputRT = nullptr);

    /**
     * @brief ブルーム用レンダーターゲットの設定
     * @param brightPassRT ブライトパス用レンダーターゲット
     * @param blurRT0 ブラー用レンダーターゲット0
     * @param blurRT1 ブラー用レンダーターゲット1
     */
    void SetBloomRenderTargets(RenderTexture* brightPassRT, RenderTexture* blurRT0, RenderTexture* blurRT1);

    /**
     * @brief ポストプロセスをリサイズする。
     * @param width 新しい幅
     * @param height 新しい高さ
     */
    void Resize(uint32_t width, uint32_t height);

    /**
     * @brief シェーダーを再コンパイルしてパイプラインを作り直す
     *
     * @details ホットリロードから呼ばれる。コンパイルに失敗した場合は
     *          既存のパイプラインを一切触らずに偽を返す。
     *          作りかけの状態で差し替えると絵が消えてしまう。
     *
     * @param outError 失敗理由の出力先
     * @return 全てのシェーダーのコンパイルに成功したら真
     */
    bool ReloadShaders(std::string& outError);

#ifdef USE_IMGUI
    /**
     * @brief デバッグUIを登録する
     * @details トーンマップの露出やブルームの閾値は、実際の絵を見ながらでないと
     *          決められないため、調整用のUIを用意する。
     */
    void RegisterDebugUI();

    /**
     * @brief デバッグUIを描画する
     */
    void DrawImGui();
#endif

    std::unique_ptr<GrayscaleEffect> grayscaleEffect_; // グレースケールエフェクト
    std::unique_ptr<VignetteEffect> vignetteEffect_;   // ビネットエフェクト
    std::unique_ptr<NoiseEffect> noiseEffect_;         // ノイズエフェクト
    std::unique_ptr<CRTEffect> crtEffect_;             // CRTエフェクト
    std::unique_ptr<BloomEffect> bloomEffect_;         // ブルームエフェクト
    std::unique_ptr<TonemapEffect> tonemapEffect_;     // トーンマップ（HDR→LDR）
	std::unique_ptr<GaussianBlurEffect> gaussianBlurEffect_;
	std::unique_ptr<DiffusionEffect> diffusionEffect_;
	std::unique_ptr<RadialBlurEffect> radialBlurEffect_;
	std::unique_ptr<ColorGradingEffect> colorGradingEffect_;

    /**
     * @brief ブライトパスパラメータ
     */
	struct BrightPassParams
    {
        float threshold = 0.8f;   // 明るさの閾値
        float intensity = 1.5f;   // 強度
        float knee = 0.5f;        // ソフトな閾値の範囲
        float padding = 0.0f;     // パディング
    } brightPassParams_;

    /**
     * @brief ブラーパラメータ
     */
    struct BlurParams
    {
        Vector2 texelSize;           // テクセルサイズ
        Vector2 blurDirection;       // ブラー方向
        float radius = 8.0f;         // ブラー半径
        float padding[3] = {};       // パディング
    } blurParams_;

private:
    /**
     * @brief 定数バッファの作成
     */
    void CreateConstantBuffer();

    /**
     * @brief 定数バッファの更新
     */
    void UpdateConstantBuffer();

    /**
     * @brief パイプラインのセットアップ
     * @param vsPath 頂点シェーダーのパス
     * @param psPath ピクセルシェーダーのパス
     */
    void SetupPipeline(const std::wstring& vsPath, const std::wstring& psPath);

    /**
     * @brief ブルーム用パイプラインの作成
     */
    void CreateBloomPipelines();

    /**
     * @brief シングルパスレンダリング
     * @param inputTexture 入力テクスチャ
     */
    void RenderSinglePass(RenderTexture* inputTexture, RenderTexture* outputRT = nullptr);

    /**
     * @brief ブルーム付きレンダリング
     * @param inputTexture 入力テクスチャ
     */
    void RenderWithBloom(RenderTexture* inputTexture, RenderTexture* outputRT = nullptr, RenderTexture* selectiveBloomSource = nullptr);

    /**
     * @brief ブルーム用レンダーターゲットが設定されているかチェック
     * @return 設定されている場合true
     */
    bool HasBloomRenderTargets() const;

private:
    DirectXCommon* dxCommon_ = nullptr;  // DirectXCommonへのポインタ
    SrvManager* srvManager_ = nullptr;   // SRVマネージャーへのポインタ

    // ビューポートとシザー矩形の設定
    D3D12_VIEWPORT viewport_ = {};
	D3D12_RECT scissorRect_ = {};

    // パイプライン関連
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;      // ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;      // パイプラインステート
    Microsoft::WRL::ComPtr<ID3D12RootSignature> singlePassRootSignature_; // シングルパス用ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> singlePassPSO_;      // シングルパス用パイプラインステート
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;          // 定数バッファ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> brightPassPSO_;      // ブライトパス用パイプラインステート
	Microsoft::WRL::ComPtr<ID3D12RootSignature> bloomRootSignature_; // ブルーム用ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> blurPSO_;            // ブラー用パイプラインステート

    std::wstring vsPath_;  // 最終合成の頂点シェーダーのパス（ホットリロード用）
    std::wstring psPath_;  // 最終合成のピクセルシェーダーのパス（ホットリロード用）
    Microsoft::WRL::ComPtr<ID3D12Resource> brightPassConstantBuffer_; // ブライトパス用定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> blurConstantBuffer_;      // ブラー用定数バッファ

    RenderTexture* brightPassRT_ = nullptr;      // ブライトパス用レンダーターゲット
    RenderTexture* blurRT_[2] = { nullptr, nullptr }; // ブラー用レンダーターゲット
    // 選択的ブルームを使うか。既定はOFF。ONにすると明るさで拾う通常のブルームは
    // 効かなくなり、発光を有効にしたものだけが光る
    bool selectiveBloomModeEnabled_ = false;

    PostEffectParams params_;    // 現在のエフェクトパラメータ
    PostEffectParams preParams_; // 前フレームのエフェクトパラメータ

    
};
} // namespace KCE
