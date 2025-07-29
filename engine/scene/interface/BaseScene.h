#pragma once
class SceneManager;

class BaseScene
{
public: //メンバ関数
	//コンストラクタ
	BaseScene() = default;
	//デストラクタ
	virtual ~BaseScene() = default;
	//初期化
	virtual void Initialize() = 0;
	//終了
	virtual void Finalize() = 0;
	//更新
	virtual void Update() = 0;
	//描画
	virtual void Draw3D() = 0;
	virtual void Draw2D() = 0;

	//シーンマネージャーのセット
	void SetSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager; }

protected:
	SceneManager* sceneManager_ = nullptr;
};