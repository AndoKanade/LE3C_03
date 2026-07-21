#pragma once

#include "systems/BaseScene.h"
#include "MyMath.h"
#include <memory>
#include <string>
#include <vector>

class Input;
class Obj3D;
class Obj3dCommon;
class SpriteCommon;
class Skybox;
class SkyboxCommon;
class Application;

// 変更 レールエディターの前方宣言を追加しました
class RailEditor;

class GameScene : public BaseScene{
public:
	GameScene();
	~GameScene() override;

	// シーンの初期化
	void Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon) override;
	// シーンの終了処理
	void Finalize() override;
	// シーンの更新処理
	void Update() override;
	// シーンの描画処理
	void Draw() override;

private:
	// 外部から受け取るポインタ
	Obj3dCommon* object3dCommon_ = nullptr;
	Input* input_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;
	Application* app_ = nullptr;

	// スカイボックス
	std::unique_ptr<SkyboxCommon> skyboxCommon_;
	std::unique_ptr<Skybox> skybox_;

	// 変更 レールエディターを追加しました
	std::unique_ptr<RailEditor> railEditor_;

	// 追加 レール移動の進行度(0〜1)と速度
	float railT_ = 0.0f;
	float railSpeed_ = 0.05f; // 1秒あたりの進行量(仮値、後で調整)

	// 追加 俯瞰デバッグカメラON/OFF状態
	bool useDebugTopCamera_ = false;

	// 追加 カメラの現在位置を可視化するためのマーカー
	std::unique_ptr<Obj3D> cameraMarker_;
};