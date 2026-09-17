#include "Framework.h"

#include <algorithm>

#include "audio/Audio.h"
#include "base/Logger.h"
#include "input/Input.h"

// system
#include "graphics/2d/SpriteCommon.h"
#include "graphics/3d/Object3dCommon.h"
// manager
#include "manager/editor/JsonEditor.h"
#include "manager/editor/GameObjectEditor.h"
#include "manager/graphics/TextureManager.h"
#include "effects/particle/ParticleManager.h"
#include "manager/graphics/ModelManager.h"
#include "manager/graphics/LineManager.h"
#include "manager/graphics/SkinnedModelManager.h"
#include "time/TimeManager.h"
#include "time/TimerManager.h"
#include "manager/graphics/InstancedModelPipelineManager.h"
#include "manager/graphics/SkinningPipelineManager.h"
#include "manager/editor/DebugUIManager.h"
#include "manager/editor/ConsoleLog.h"
#include "graphics/RenderFormats.h"
#include "graphics/shader/ShaderHotReload.h"
#include "graphics/pipeline/StandardRenderPasses.h"
#include "graphics/view/RenderView.h"
#include "gameobject/manager/GameObjectManager.h"
#include "gameobject/component/collision/GameObjectCollisionManager.h"
// editor
#include "editor/EditorContext.h"
#include "editor/SceneGizmo.h"
#include "editor/SceneViewContext.h"
#include "editor/SelectionContext.h"
#include "editor/command/CommandHistory.h"
// sequencer
#include "sequencer/editor/SequencerEditor.h"
#include "sequencer/runtime/CutsceneManager.h"

#ifdef USE_IMGUI
#include "ImGui/imgui_internal.h"

#endif

namespace KCE
{

// ブラーレンダーターゲットの数（ピンポンバッファ用）
constexpr int kBlurRenderTargetCount = 2;
// レンダーテクスチャのクリアカラー
constexpr float kClearColorR = 0.1f;
constexpr float kClearColorG = 0.1f;
constexpr float kClearColorB = 0.1f;
constexpr float kClearColorA = 1.0f;
// デフォルトのカメラ位置
constexpr float kDefaultCameraY = 1.0f;
constexpr float kDefaultCameraZ = -10.0f;
// 2048回の描画でWVPとカメラ定数をそれぞれ確保できる容量。
constexpr size_t kFrameConstantBufferCapacity = 2048 * 2 * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;

void Framework::Initialize()
{
	/*----- システムの初期化（順序重要） -----*/

	// 1. ウィンドウアプリケーションの初期化
	winApp_ = std::make_unique<WinApp>();
	winApp_->Initialize();

	// 2. DirectXCommonの初期化
	dxCommon_ = std::make_unique<DirectXCommon>();
	dxCommon_->Initialize(winApp_.get());

	// 3. SRVマネージャーの初期化
	srvManager_ = std::make_unique<SrvManager>();
	srvManager_->Initialize(dxCommon_.get());

	// 4. ImGuiの初期化
	imguiManager_ = std::make_unique<ImGuiManager>();
	imguiManager_->Initialize(winApp_.get(), dxCommon_.get(), srvManager_.get());

	// DebugUIManager と ConsoleLog の初期化
	DebugUIManager::GetInstance()->Initialize();
	ConsoleLog::GetInstance()->Initialize();
	SceneGizmo::GetInstance()->Initialize();

	// 読み込みを並べて回すワーカー。起動時の素材読み込みから使う
	jobSystem_ = std::make_unique<JobSystem>();
	jobSystem_->Initialize();


	/*----- テクスチャ・グラフィックスの初期化 -----*/

	// テクスチャマネージャーの初期化
	TextureManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());

	// スプライト共通部の初期化
	spriteCommon_ = std::make_unique<SpriteCommon>();
	spriteCommon_->Initialize(dxCommon_.get());

	// ゲーム内の日本語の文字（DirectWrite）。先に描く分の転送を今のコマンドに積むので、GPU の完了待ちより前に用意する
	constexpr uint32_t kGlyphAtlasFontSize = 48;
	glyphAtlas_ = std::make_unique<GlyphAtlas>();
	glyphAtlas_->Build(dxCommon_.get(), kGlyphAtlasFontSize);

	// 3Dオブジェクト共通部の初期化
	objectCommon_ = std::make_unique<Object3dCommon>();
	objectCommon_->Initialize(dxCommon_.get(), srvManager_.get());
	frameConstantAllocator_ = std::make_unique<FrameConstantAllocator>();
	frameConstantAllocator_->Initialize(dxCommon_.get(), kFrameConstantBufferCapacity);
	objectCommon_->SetFrameConstantAllocator(frameConstantAllocator_.get());

	// 2D の文字は文字列ごとに1回で描く。板のデータはフレームの置き場に書くので、置き場を作った後で用意する
	textSpritePipeline_ = std::make_unique<TextSpritePipeline>();
	textSpritePipeline_->Initialize(dxCommon_.get(), frameConstantAllocator_.get());
	textOverlay_ = std::make_unique<TextOverlay>();
	textOverlay_->Initialize(spriteCommon_.get(), glyphAtlas_.get(), textSpritePipeline_.get());

	// 3Dモデルマネージャーの初期化
	ModelManager::GetInstance()->Initialize(dxCommon_.get());

	// スキニングモデルマネージャーの初期化
	SkinnedModelManager::GetInstance()->Initialize(dxCommon_.get());

	// パーティクルマネージャーの初期化
	ParticleManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());

	// インスタンス描画パイプラインマネージャーの初期化
	InstancedModelPipelineManager::GetInstance()->Initialize(dxCommon_.get());

	// スキニングパイプラインマネージャーの初期化
	SkinningPipelineManager::GetInstance()->Initialize(dxCommon_.get());

	/*----- 入力・オーディオ・時間管理の初期化 -----*/

	// 入力の初期化
	Input::GetInstance()->Initialize(winApp_.get());

	// オーディオの初期化
	Audio::GetInstance()->Initialize();
#ifdef USE_IMGUI
	Audio::GetInstance()->SetDebugWindowVisible(true);
#endif // USE_IMGUI

	// 時間管理クラスの初期化。Settings のページは DebugUIManager の初期化の後に登録する
	TimeManager::GetInstance().RegisterDebugUI();

	// タイマーマネージャーの初期化
	TimerManager::GetInstance();

	/*----- カメラ・シーン管理の初期化 -----*/

	// カメラマネージャーの初期化
	cameraManager_ = std::make_unique<CameraManager>();
	cameraManager_->Initialize(dxCommon_.get());
	cameraManager_->AddCamera("main");
	cameraManager_->SetActiveCamera("main");
	cameraManager_->GetActiveCamera()->SetTranslate({ 0.0f, kDefaultCameraY, kDefaultCameraZ });
	cameraManager_->GetActiveCamera()->SetRotate({ 0.0f, 0.0f, 0.0f });
#ifdef USE_IMGUI
	// エディタで視点を動かす口はここだけにする（シーンごとやシーケンサに持たせると、同じ右ドラッグで二重に動く）
	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize(cameraManager_->GetCamera("main"));
	debugCamera_->Start(cameraManager_->GetActiveCamera()->GetTranslate(), cameraManager_->GetActiveCamera()->GetRotate());
#endif

	// 3Dオブジェクト共通部に初期カメラをセット
	objectCommon_->SetDefaultCamera(cameraManager_->GetActiveCamera());
	objectCommon_->SetCameraManager(cameraManager_.get());

	// シーンファクトリーの初期化
	sceneFactory_ = std::make_unique<SceneFactory>();

	// シーンマネージャーの初期化
	sceneManager_ = std::make_unique<SceneManager>(sceneFactory_.get());

	/*----- ライト・ライン管理の初期化 -----*/

	// ライトマネージャーの初期化
	lightManager_ = std::make_unique<LightManager>();
	lightManager_->Initialize(dxCommon_.get());

	// ラインマネージャーの初期化
	LineManager::GetInstance()->Initialize(dxCommon_.get(), cameraManager_.get());
	// ラインもビューごとに頂点と行列を分けて書く（モニターや床の反射のカメラで上書きされないように）
	LineManager::GetInstance()->SetFrameConstantAllocator(frameConstantAllocator_.get());

	/*----- レンダーテクスチャ・ポストプロセスの初期化 -----*/

	Vector4 clearColor = { kClearColorR, kClearColorG, kClearColorB, kClearColorA };

	// ブライトパス用レンダーターゲットの初期化
	brightPassRT_ = std::make_unique<RenderTexture>();
	// DXGI_FORMAT_R16G16B16A16_FLOAT: HDR処理用の浮動小数点フォーマット
	brightPassRT_->Initialize(
		dxCommon_.get(),
		srvManager_.get(),
		winApp_->GetClientWidth(),
		winApp_->GetClientHeight(),
		kBloomBufferFormat,
		clearColor
	);

	// ブラー用レンダーターゲットの初期化（ピンポンバッファとして使用）
	for (int i = 0; i < kBlurRenderTargetCount; i++)
	{
		blurRT_[i] = std::make_unique<RenderTexture>();
		blurRT_[i]->Initialize(
			dxCommon_.get(),
			srvManager_.get(),
			winApp_->GetClientWidth(),
			winApp_->GetClientHeight(),
			kBloomBufferFormat,
			clearColor
		);
	}

	// ポストプロセスマネージャーの初期化
	postProcessManager_ = std::make_unique<PostProcessManager>();
	postProcessManager_->Initialize(
		dxCommon_.get(),
		srvManager_.get(),
		L"Resources/shaders/PostEffect.VS.hlsl",
		L"Resources/shaders/PostEffect.PS.hlsl",
		winApp_->GetClientWidth(),
		winApp_->GetClientHeight()
	);
	postProcessManager_->SetBloomRenderTargets(
		brightPassRT_.get(),
		blurRT_[0].get(),
		blurRT_[1].get()
	);
#ifdef USE_IMGUI
	postProcessManager_->RegisterDebugUI();
#endif

	// シェーダーのホットリロード。以降パスを足していく作業で、
	// シェーダー1行のために再起動しなくて済むようにする。
	ShaderHotReload* shaderHotReload = ShaderHotReload::GetInstance();
	shaderHotReload->Initialize(dxCommon_.get());
	shaderHotReload->Register(postProcessManager_.get(), "PostProcess",
		[this](std::string& outError) { return postProcessManager_->ReloadShaders(outError); });
#ifdef USE_IMGUI
	shaderHotReload->RegisterDebugUI();
#endif

	/*----- その他の初期化 -----*/

	// JSONエディターの初期化
	JsonEditor::GetInstance()->Initialize();

	// GameObjectエディターの初期化
	GameObjectEditor::GetInstance()->Initialize();

	// 演出シーケンサの初期化。編集用カメラをここで追加するため、
	// カメラマネージャーの初期化より後に行う。
	SequencerEditor::GetInstance()->Initialize(cameraManager_.get(), lightManager_.get(), postProcessManager_.get());

	// シャドウマップマネージャーの初期化
	shadowMapManager_ = std::make_unique<ShadowMapManager>();
	shadowMapManager_->Initialize(dxCommon_.get(), srvManager_.get());
	// カスケードシャドウマップを作成（4カスケード、各2048x2048）
	shadowMapManager_->CreateCascadeShadowMaps(2048);

	// シャドウマップ描画パイプラインの初期化
	shadowMapPipeline_ = std::make_unique<ShadowMapPipeline>();
	shadowMapPipeline_->Initialize(dxCommon_.get());

	// ディファードレンダラーの初期化。
	// G-Buffer は持たず、パイプラインステートだけを全ビューで共有する。
	deferredRenderer_ = std::make_unique<DeferredRenderer>();
	deferredRenderer_->Initialize(dxCommon_.get(), srvManager_.get());
#ifdef USE_IMGUI
	deferredRenderer_->RegisterDebugUI();
#endif

	// 本編を描くビューの初期化。
	// G-Bufferとシーンカラー（HDR）をまとめて持つ。
	mainView_ = std::make_unique<RenderView>();
	mainView_->Initialize(
		dxCommon_.get(),
		srvManager_.get(),
		"Main",
		winApp_->GetClientWidth(),
		winApp_->GetClientHeight());
	// 本編のビューはカメラを固定しない。
	// アクティブカメラは実行中に切り替わる（デバッグカメラ、シーケンサの編集用カメラなど）ため、
	// 固定するとその切り替えに追従できなくなる。
	// カメラ未設定のビューは、描画時にアクティブカメラを使う。
	mainView_->SetCamera(nullptr);

	// 大気（フォグとビーム）。ビームは空気中の粒子が前提なので、両方そろえて入れる
	fogRenderer_ = std::make_unique<FogRenderer>();
	fogRenderer_->Initialize(dxCommon_.get(), srvManager_.get());
	beamRenderer_ = std::make_unique<BeamRenderer>();
	beamRenderer_->Initialize(dxCommon_.get(), srvManager_.get());
	shaderHotReload->Register(fogRenderer_.get(), "Fog",
		[this](std::string& outError) { return fogRenderer_->ReloadShaders(outError); });
	shaderHotReload->Register(beamRenderer_.get(), "Beam",
		[this](std::string& outError) { return beamRenderer_->ReloadShaders(outError); });
#ifdef USE_IMGUI
	fogRenderer_->RegisterDebugUI();
	beamRenderer_->RegisterDebugUI();
#endif

	// 画の質（アンチエイリアス・被写界深度・ボリュメトリック・床反射）
	{
		const uint32_t width = winApp_->GetClientWidth();
		const uint32_t height = winApp_->GetClientHeight();
		fxaaRenderer_ = std::make_unique<FxaaRenderer>();
		fxaaRenderer_->Initialize(dxCommon_.get(), srvManager_.get(), width, height);
		depthOfFieldRenderer_ = std::make_unique<DepthOfFieldRenderer>();
		depthOfFieldRenderer_->Initialize(dxCommon_.get(), srvManager_.get(), width, height);
		volumetricLightRenderer_ = std::make_unique<VolumetricLightRenderer>();
		volumetricLightRenderer_->Initialize(dxCommon_.get(), srvManager_.get(), width, height);
		planarReflection_ = std::make_unique<PlanarReflection>();
		planarReflection_->Initialize(dxCommon_.get(), srvManager_.get(), this, width, height);

		shaderHotReload->Register(fxaaRenderer_.get(), "FXAA",
			[this](std::string& outError) { return fxaaRenderer_->ReloadShaders(outError); });
		shaderHotReload->Register(depthOfFieldRenderer_.get(), "DepthOfField",
			[this](std::string& outError) { return depthOfFieldRenderer_->ReloadShaders(outError); });
		shaderHotReload->Register(volumetricLightRenderer_.get(), "VolumetricLight",
			[this](std::string& outError) { return volumetricLightRenderer_->ReloadShaders(outError); });
		shaderHotReload->Register(planarReflection_.get(), "Reflection",
			[this](std::string& outError) { return planarReflection_->ReloadShaders(outError); });
#ifdef USE_IMGUI
		fxaaRenderer_->RegisterDebugUI();
		depthOfFieldRenderer_->RegisterDebugUI();
		volumetricLightRenderer_->RegisterDebugUI();
		planarReflection_->RegisterDebugUI();
#endif
	}

	// 3D 空間の文字（ライブ映像の歌詞の演出など）
	text3DRenderer_ = std::make_unique<Text3DRenderer>();
	text3DRenderer_->Initialize(dxCommon_.get(), glyphAtlas_.get());
	shaderHotReload->Register(text3DRenderer_.get(), "Text3D",
		[this](std::string& outError) { return text3DRenderer_->ReloadShaders(outError); });
#ifdef USE_IMGUI
	text3DRenderer_->RegisterDebugUI();
#endif

	// アウトライン（輪郭線）
	outlineRenderer_ = std::make_unique<OutlineRenderer>();
	outlineRenderer_->Initialize(dxCommon_.get(), srvManager_.get());
	shaderHotReload->Register(outlineRenderer_.get(), "Outline",
		[this](std::string& outError) { return outlineRenderer_->ReloadShaders(outError); });
#ifdef USE_IMGUI
	outlineRenderer_->RegisterDebugUI();
#endif

	// シーケンサのトラックからフォグの濃さとビームの明るさを動かせるようにする。
	// シーケンサの初期化はこれらより前なので、ここで後から渡す
	SequencerEditor::GetInstance()->SetAtmosphere(fogRenderer_.get(), beamRenderer_.get());
	SequencerEditor::GetInstance()->SetTextOverlay(textOverlay_.get());
	SequencerEditor::GetInstance()->GetPlayer().GetBindingContext().SetText3DRenderer(text3DRenderer_.get());
	SequencerEditor::GetInstance()->GetPlayer().GetBindingContext().SetDepthOfFieldRenderer(depthOfFieldRenderer_.get());

	// ゲームからカットシーンを再生する窓口
	CutsceneManager* cutscene = CutsceneManager::GetInstance();
	cutscene->Initialize(cameraManager_.get(), lightManager_.get(), postProcessManager_.get());
	cutscene->SetAtmosphere(fogRenderer_.get(), beamRenderer_.get());
	cutscene->SetTextOverlay(textOverlay_.get());
	cutscene->SetText3DRenderer(text3DRenderer_.get());
	cutscene->SetDepthOfField(depthOfFieldRenderer_.get());
#ifdef USE_IMGUI
	cutscene->RegisterDebugUI();
#endif
	// Skyboxの初期化
	skybox_ = std::make_unique<Skybox>();

	// 描画パイプラインの構築。
	// 描画順はここに集約されており、アプリ側の Draw() には展開しない。
	renderPipeline_ = std::make_unique<RenderPipeline>();
	BuildStandardRenderPipeline(*renderPipeline_);
#ifdef USE_IMGUI
	renderPipeline_->RegisterDebugUI();
#endif

	// パスごとの GPU 時間とドローコール数の計測（本編もサブビューも、パイプラインを回すたびに積む）
	renderProfiler_ = std::make_unique<RenderProfiler>();
	renderProfiler_->Initialize(dxCommon_.get());
#ifdef USE_IMGUI
	renderProfiler_->RegisterDebugUI();
#endif

	// サブビュー（中継映像、カメラプレビュー、反射）用のパイプライン。
	// シャドウマップは本編と共有するため含めない。
	subViewPipeline_ = std::make_unique<RenderPipeline>();
	BuildSceneOnlyRenderPipeline(*subViewPipeline_);

	// 本編のパイプラインからサブビューの描画を呼べるようにする
	if (auto* subViewPass = dynamic_cast<SubViewRenderPass*>(renderPipeline_->FindPass("SubViews")))
	{
		subViewPass->SetCallback([this]()
		{
			// 反射用カメラは本編のカメラを鏡映したものなので、描く直前に合わせる
			if (planarReflection_)
			{
				planarReflection_->SyncCamera(cameraManager_->GetActiveCamera());
			}
			for (RenderView* view : subViews_)
			{
				RenderSubView(view);
			}
		});
	}

	// ウィンドウのリサイズコールバックを登録する
	winApp_->SetResizeCallback([this](uint32_t width, uint32_t height) {
		// GPUのコマンド完了を待機する
		dxCommon_->ExecuteAndWait();

		// DirectXCommon をリサイズする
		dxCommon_->Resize(width, height);

		// 本編のビューをリサイズする（G-Bufferとシーンカラーがまとめて作り直される）
		mainView_->Resize(width, height);

		// 各種レンダーターゲットをリサイズする
		brightPassRT_->Resize(width, height);
		for (int i = 0; i < kBlurRenderTargetCount; i++)
		{
			blurRT_[i]->Resize(width, height);
		}

		// ポストプロセスマネージャーをリサイズする
		postProcessManager_->Resize(width, height);

		// 画面の大きさで作っているターゲットを持つもの
		fxaaRenderer_->Resize(width, height);
		depthOfFieldRenderer_->Resize(width, height);
		volumetricLightRenderer_->Resize(width, height);
		planarReflection_->Resize(width, height);

		// 派生クラス用のリサイズ通知
		OnResize(width, height);
	});
}


void Framework::Finalize()
{
	/*----- 終了処理（初期化の逆順で解放） -----*/

	sceneManager_.reset();

	// 仕事がマネージャーを触ってるかもしれないので、何より先にワーカーを止める
	jobSystem_.reset();

	// GameObject の管理を明示的に閉じる。閉じないとマネージャーはプロセス終了時の静的変数の片付けまで残り、
	// そこで壊れる GameObject（static に置いた物や、マネージャー自身が持つ物）が壊れかけのマネージャーから Unregister して落ちる。
	// 持っている GameObject は GPU のリソースを持つので、デバイスより先に、エディタ（GameObjectEditor）への通知が届くうちに閉じる
	if (GameObjectManager::HasInstance())
	{
		GameObjectManager::GetInstance()->Finalize();
	}
	// 当たり判定の管理も同じ理由で閉じる。GameObject（のコライダー）を片付けた後に閉じる
	if (GameObjectCollisionManager::HasInstance())
	{
		GameObjectCollisionManager::GetInstance()->Finalize();
	}

	// エディタ層は、登録先の DebugUIManager より先に片付ける
#ifdef USE_IMGUI
	debugCamera_.reset();
#endif
	ShaderHotReload::GetInstance()->Finalize();
	CutsceneManager::GetInstance()->Finalize();
	SequencerEditor::GetInstance()->Finalize();
	SceneGizmo::GetInstance()->Finalize();
	TimeManager::GetInstance().UnregisterDebugUI();
	SceneViewContext::GetInstance()->Finalize();
	SelectionContext::GetInstance()->Finalize();
	CommandHistory::GetInstance()->Finalize();
	EditorContext::GetInstance()->Finalize();

	Audio::GetInstance()->Finalize();
	winApp_->Finalize();
	winApp_.reset();
	DebugUIManager::GetInstance()->Finalize();
	ConsoleLog::GetInstance()->Finalize();

	imguiManager_->Finalize();
	imguiManager_.reset();
	TextureManager::GetInstance()->Finalize();
	dxCommon_.reset();
	spriteCommon_.reset();
	objectCommon_.reset();
	ModelManager::GetInstance()->Finalize();
	SkinnedModelManager::GetInstance()->Finalize();
	ParticleManager::GetInstance()->Finalize();
	InstancedModelPipelineManager::GetInstance()->Finalize();
	SkinningPipelineManager::GetInstance()->Finalize();
	Input::GetInstance()->Finalize();
	lightManager_.reset();
	LineManager::GetInstance()->Finalize();
	mainView_.reset();
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance() && postProcessManager_)
	{
		DebugUIManager::GetInstance()->Unregister(postProcessManager_.get());
	}
#endif
	postProcessManager_.reset();
	brightPassRT_.reset();

	// ブラー用レンダーターゲットの解放
	for (int i = 0; i < kBlurRenderTargetCount; i++)
	{
		blurRT_[i].reset();
	}

	// シャドウマップ関連の解放
	shadowMapPipeline_.reset();
	shadowMapManager_.reset();

	// パスは各マネージャーを参照するだけで所有しないため、解放順は問わない
	renderPipeline_.reset();
	subViewPipeline_.reset();
	renderProfiler_.reset();
	// 反射のサブビューは自分が持っているので、ビューを消す前に畳む
	if (planarReflection_)
	{
		planarReflection_->Finalize();
	}
	planarReflection_.reset();
	volumetricLightRenderer_.reset();
	depthOfFieldRenderer_.reset();
	fxaaRenderer_.reset();
	subViews_.clear();
	ownedSubViews_.clear();
	pendingDestroySubViews_.clear();
	// 文字の Sprite は GPU リソースを持つので、デバイスより先に畳む
	textOverlay_.reset();
	textSpritePipeline_.reset();
	text3DRenderer_.reset();
	glyphAtlas_.reset();

	GameObjectEditor::GetInstance()->Finalize();
	JsonEditor::GetInstance()->Finalize();
}

void Framework::Update()
{
	// 前のフレームで登録を外したサブビューを破棄する。PostDraw で前のフレームの GPU 完了を待ち終わっているので、
	// もうどのコマンドリストも指していない
	pendingDestroySubViews_.clear();

	// 画面サイズ変更のキー入力チェック（F12でフルスクリーントグル）
	if (Input::GetInstance()->TriggerKey(DIK_F12))
	{
		dxCommon_->SetFullscreen(!dxCommon_->IsFullscreen());
	}

	// ウィンドウメッセージ処理
	if (winApp_->ProcessMessage())
	{
		endRequest_ = true;
	}

	// ImGuiフレーム開始
	imguiManager_->Begin();

	// 入力の更新
	Input::GetInstance()->Update();

	// 時間計測の更新
	TimeManager::GetInstance().Update();

	// タイマーマネージャーの更新
	TimerManager::GetInstance().Update();

	// シェーダーの更新を検出してリロードする
	ShaderHotReload::GetInstance()->Update();

	// 演出シーケンサの更新。カメラの行列を作る前にトラックを評価しておく必要がある。
	SequencerEditor::GetInstance()->Update();

	// ゲームから再生されたカットシーンの更新（カメラのブレンドもここで行う）
	CutsceneManager::GetInstance()->Update();

#ifdef USE_IMGUI
	// デバッグカメラは右ドラッグしている間だけ main に触る
	debugCamera_->Update();
#endif

	// カメラの更新
	cameraManager_->Update();

	// オーディオの更新
	Audio::GetInstance()->Update();

	// ライトマネージャーの更新
	lightManager_->Update();

	// Skyboxの更新
	skybox_->Update(cameraManager_->GetActiveCamera());

	// JSONエディターの更新
	JsonEditor::GetInstance()->RenderEditUI();
}

RenderPassContext Framework::MakeRenderPassContext(RenderView* view, RenderTexture* outputTarget) const
{
	RenderPassContext ctx;
	ctx.dxCommon = dxCommon_.get();
	ctx.srvManager = srvManager_.get();
	ctx.cameraManager = cameraManager_.get();
	ctx.lightManager = lightManager_.get();
	ctx.sceneManager = sceneManager_.get();
	ctx.skybox = skybox_.get();
	ctx.objectCommon = objectCommon_.get();
	ctx.spriteCommon = spriteCommon_.get();
	ctx.shadowMapManager = shadowMapManager_.get();
	ctx.shadowMapPipeline = shadowMapPipeline_.get();
	ctx.shadowNearPlane = shadowNearPlane_;
	ctx.shadowFarPlane = shadowFarPlane_;
	ctx.deferredRenderer = deferredRenderer_.get();
	ctx.view = view;
	ctx.outputTarget = outputTarget;
	ctx.postProcessManager = postProcessManager_.get();
	ctx.frameConstantAllocator = frameConstantAllocator_.get();
	ctx.fogRenderer = fogRenderer_.get();
	ctx.beamRenderer = beamRenderer_.get();
	ctx.outlineRenderer = outlineRenderer_.get();
	ctx.textOverlay = textOverlay_.get();
	ctx.text3DRenderer = text3DRenderer_.get();
	ctx.fxaaRenderer = fxaaRenderer_.get();
	ctx.depthOfFieldRenderer = depthOfFieldRenderer_.get();
	ctx.volumetricLightRenderer = volumetricLightRenderer_.get();
	ctx.planarReflection = planarReflection_.get();
	ctx.renderProfiler = renderProfiler_.get();
	return ctx;
}

void Framework::ExecuteRenderPipeline(RenderTexture* outputTarget)
{
	if (!renderPipeline_ || !mainView_)
	{
		return;
	}
	// PostDrawで前フレームのGPU完了を待つため、ここで同じ領域を再利用できる。
	frameConstantAllocator_->BeginFrame();
	// 前のフレームで描き足した文字の転送バッファは、もう GPU が使い終わっている
	if (glyphAtlas_)
	{
		glyphAtlas_->BeginFrame();
	}

	// 本編は全レイヤーを描く
	if (GameObjectManager::HasInstance())
	{
		GameObjectManager::GetInstance()->SetRenderLayerMask(mainView_->GetLayerMask());
		// 更新が全部終わった後、どのビューを描くより前に、行列を全員分1回だけ確定させる
		GameObjectManager::GetInstance()->UpdateRenderTransforms();
	}

	// 前のフレームの計測結果を読んでから、このフレームの計測を始める
	renderProfiler_->BeginFrame();
	renderPipeline_->Execute(MakeRenderPassContext(mainView_.get(), outputTarget));
	// コマンドリストを閉じる（PostDraw）前に、測った値を読み出し用へ移す
	renderProfiler_->EndFrame();
}

void Framework::RegisterSubView(RenderView* view)
{
	if (!view || std::find(subViews_.begin(), subViews_.end(), view) != subViews_.end())
	{
		return;
	}
	subViews_.push_back(view);
}

void Framework::UnregisterSubView(RenderView* view)
{
	subViews_.erase(std::remove(subViews_.begin(), subViews_.end(), view), subViews_.end());
}

RenderView* Framework::CreateSubView(const std::string& name, uint32_t width, uint32_t height)
{
	auto view = std::make_unique<RenderView>();
	view->Initialize(dxCommon_.get(), srvManager_.get(), name, width, height);
	RenderView* raw = view.get();
	ownedSubViews_.push_back(std::move(view));
	RegisterSubView(raw);
	return raw;
}

void Framework::DestroySubView(RenderView* view)
{
	if (!view)
	{
		return;
	}
	// 次のフレームからは描かないよう登録はすぐ外す
	UnregisterSubView(view);

	// 破棄は次のフレームの頭まで遅らせる。このフレームのコマンドリストが既にこのビューを指していることがあるため
	const auto owned = std::find_if(ownedSubViews_.begin(), ownedSubViews_.end(),
		[view](const std::unique_ptr<RenderView>& candidate) { return candidate.get() == view; });
	if (owned == ownedSubViews_.end())
	{
		return;
	}
	pendingDestroySubViews_.push_back(std::move(*owned));
	ownedSubViews_.erase(owned);
}

void Framework::RenderSubView(RenderView* view)
{
	if (!subViewPipeline_ || !view || !view->IsEnabled() || !view->IsValid())
	{
		return;
	}
	// 間隔を決めたビュー（24fps のモニターなど）は、時間が来たフレームだけ描く。
	// 描かないフレームは前の絵がそのまま使われる
	// ビューが時計を指定していなければ Editor（編集中やゲームの一時停止中でも描き直す）
	TimeManager& time = TimeManager::GetInstance();
	const ClockId viewClock = view->GetClock().IsSpecified() ? view->GetClock() : time.EditorClock();
	if (!view->AdvanceAndCheckUpdate(time.GetRealDeltaTime(viewClock)))
	{
		return;
	}

	Camera* camera = view->GetCamera();
	if (camera && camera != cameraManager_->GetPrimaryCamera())
	{
		// サブビュー専用のカメラは CameraManager::Update() の対象外なので、
		// 行列がこのフレームの値になるようここで更新する。
		camera->Update();
	}

	// 描画の間だけアクティブカメラと描画対象レイヤーを差し替える
	cameraManager_->SetRenderCameraOverride(camera);
	if (GameObjectManager::HasInstance())
	{
		GameObjectManager::GetInstance()->SetRenderLayerMask(view->GetLayerMask());
	}

	// サブビューはシーンを焼くだけなので、ポストプロセスの出力先は持たない
	subViewPipeline_->Execute(MakeRenderPassContext(view, nullptr));

	// 差し替えたままにするとゲームのロジックがサブビューのカメラを見てしまう
	cameraManager_->ClearRenderCameraOverride();
	if (GameObjectManager::HasInstance())
	{
		GameObjectManager::GetInstance()->SetRenderLayerMask(kRenderLayerAll);
	}
}

void Framework::Draw3DSetting()
{
	// 実体はフォワードパスと共有する。ここで二重に書くと、
	// ルートパラメータ番号を変えたときに片方だけ直し忘れる。
	ApplyCommon3DRenderingSetting(MakeRenderPassContext(mainView_.get(), nullptr));
}




void Framework::Draw2DSetting()
{
	ApplyCommon2DRenderingSetting(MakeRenderPassContext(mainView_.get(), nullptr));
}

void Framework::Run()
{
	// 初期化
	Initialize();

	// メインループ開始ログ
	KCE::Logger::Log("\n/******* Start Main Loop *******/\n\n");

	// メインループ
	while (true)
	{
		// 更新
		Update();

		// 終了リクエストがあればループ終了
		if (IsEndRequest())
		{
			break;
		}

		// 描画
		Draw();
	}

	// メインループ終了ログ
	KCE::Logger::Log("\n/******* End Main Loop *******/\n\n");

	// 終了処理
	Finalize();
}

void Framework::ShowPerformanceInfo()
{
	// 何もしない（Hierarchy内の DrawArea で描画されるため）
}
} // namespace KCE
