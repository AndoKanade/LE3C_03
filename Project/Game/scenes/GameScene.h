#pragma once
#include "systems/BaseScene.h"
#include "MyMath.h"
#include <memory>
#include <string>
#include <vector>

// 前方宣言
class Input;
class Obj3D;
class Obj3dCommon;
class SpriteCommon;
class Skybox;
class SkyboxCommon;
class Application;
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

	// レールエディター
	std::unique_ptr<RailEditor> railEditor_;

	// レール移動の進行度(0〜1)と速度
	float railT_ = 0.0f;
	float railSpeed_ = 0.05f; // 1秒あたりの進行量

	// 俯瞰デバッグカメラON/OFF状態
	bool useDebugTopCamera_ = false;

	// カメラの現在位置を可視化するためのマーカー
	std::unique_ptr<Obj3D> cameraMarker_;
	// カメラの向きを可視化するためのマーカー
	std::unique_ptr<Obj3D> cameraFacingMarker_;

	// プレイヤー入力によるカメラの照準オフセット
	float aimYawOffset_ = 0.0f;   // 左右(Y軸回転)
	float aimPitchOffset_ = 0.0f; // 上下(X軸回転)

	// 照準の可動範囲・速度
	const float kAimSpeed_ = 1.5f;       // 1秒あたりの回転量(ラジアン)
	const float kAimYawLimit_ = 0.6f;    // 左右の可動範囲
	const float kAimPitchLimit_ = 0.5f;  // 上下の可動範囲
};