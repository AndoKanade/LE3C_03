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
class Sprite;

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

	// 追加 プレイヤー(三人称視点用の人型モデル)
	std::unique_ptr<Obj3D> player_;
	// 追加 プレイヤーモデルの表示スケール(スプラトゥーン風に小さめの見た目にするための仮値)
	const float kPlayerScale_ = 0.3f;
	// 追加 プレイヤーモデルをレール位置よりさらに下げるオフセット(仮値、後で調整)
	// 注意: カメラのFOV(0.45rad≒26度)が狭いため、大きくしすぎると視野から外れて描画されなくなる
	const float kPlayerDownOffset_ = 0.1f;

	// 追加 レール移動の進行度(0〜1)と速度
	float railT_ = 0.0f;
	float railSpeed_ = 0.05f; // 1秒あたりの進行量(仮値、後で調整)

	// 追加 前フレームがPlayモードだったか(Playに入った瞬間を検出してリセットするのに使う)
	bool wasPlayMode_ = false;

	// 追加 三人称視点用に、カメラをレールそのものより少し上に置くオフセット(仮値、後で調整)
	const float kCameraHeightOffset_ = 1.25f;
	// 追加 引きのカメラにするため、進行方向の後方にも下げるオフセット(仮値、後で調整)
	// 変更 プレイヤーの配置距離としても使用。狭いFOVでも全身が収まるよう距離を増やした
	const float kCameraBackOffset_ = 6.0f;

	// 追加 俯瞰デバッグカメラON/OFF状態
	bool useDebugTopCamera_ = false;

	// 追加 カメラの現在位置を可視化するためのマーカー
	std::unique_ptr<Obj3D> cameraMarker_;
	// 追加 カメラの向きを可視化するためのマーカー(cameraMarker_より前方に置く)
	std::unique_ptr<Obj3D> cameraFacingMarker_;
	// 追加 メインカメラの位置・向きマーカーを描画するかどうか(ImGuiで切り替え)
	bool showCameraDebugMarkers_ = true;

	// 追加 プレイヤー入力によるカメラの照準オフセット(レールの向きに上乗せする)
	float aimYawOffset_ = 0.0f;   // 左右(Y軸回転)
	float aimPitchOffset_ = 0.0f; // 上下(X軸回転)
	// 追加 照準の可動範囲・速度(仮値、後で調整)
	const float kAimSpeed_ = 1.5f;       // 1秒あたりの回転量(ラジアン)
	const float kAimYawLimit_ = 0.6f;    // 左右の可動範囲(約34度)
	const float kAimPitchLimit_ = 0.5f;  // 上下の可動範囲(約29度)

	// 追加 テスト用の的
	struct Target{
		std::unique_ptr<Obj3D> obj;
		Vector3 position;
		bool isAlive = true;
	};
	std::vector<Target> targets_;
	// 追加 Hierarchy/Inspectorで選択中の的のインデックス(-1は未選択)
	int selectedTargetIndex_ = -1;
	// 追加 照準判定の許容角度(ラジアン)。画面中央のレティクルがこの角度以内に的を捉えていればヒット
	const float kAimHitAngle_ = 0.09f; // 約5度

	// 追加 画面中央固定のレティクル(照準)を表示するスプライト
	// 外枠(常時表示)と中心ドット(狙えているときに強調表示)の2枚構成
	std::unique_ptr<Sprite> reticleOutlineSprite_;
	std::unique_ptr<Sprite> reticleCenterSprite_;
};