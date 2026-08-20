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
class Sprite;
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

	// プレイヤー(三人称視点用の人型モデル)
	std::unique_ptr<Obj3D> player_;
	// プレイヤーモデルの表示スケール
	const float kPlayerScale_ = 0.3f;
	// プレイヤーモデルをレール位置よりさらに下げるオフセット
	// 注意: カメラのFOV(0.45rad≒26度)が狭いため、大きくしすぎると視野から外れて描画されなくなる
	const float kPlayerDownOffset_ = 0.1f;

	// レール移動の進行度(0〜1)と速度
	float railT_ = 0.0f;
	float railSpeed_ = 0.05f; // 1秒あたりの進行量

	// レール終端(railT_=1.0)に到達したかどうか(到達後はループせず停止させる)
	bool isRailFinished_ = false;

	// 前フレームがPlayモードだったか(Playに入った瞬間を検出してリセットするのに使う)
	bool wasPlayMode_ = false;

	// 三人称視点用に、カメラをレールそのものより少し上に置くオフセット
	const float kCameraHeightOffset_ = 1.25f;
	// 引きのカメラにするため、進行方向の後方にも下げるオフセット(プレイヤーの配置距離としても使用)
	const float kCameraBackOffset_ = 6.0f;

	// 俯瞰デバッグカメラON/OFF状態
	bool useDebugTopCamera_ = false;

	// カメラの現在位置を可視化するためのマーカー
	std::unique_ptr<Obj3D> cameraMarker_;
	// カメラの向きを可視化するためのマーカー(cameraMarker_より前方に置く)
	std::unique_ptr<Obj3D> cameraFacingMarker_;
	// メインカメラの位置・向きマーカーを描画するかどうか(ImGuiで切り替え)
	bool showCameraDebugMarkers_ = true;

	// プレイヤー入力によるカメラの照準オフセット(レールの向きに上乗せする)
	float aimYawOffset_ = 0.0f;   // 左右(Y軸回転)
	float aimPitchOffset_ = 0.0f; // 上下(X軸回転)

	// 照準の可動範囲・感度
	const float kMouseSensitivity_ = 0.0004f; // マウス1移動量あたりの回転量(ラジアン)
	const float kAimYawLimit_ = 0.6f;        // 左右の可動範囲(約34度)
	const float kAimPitchLimit_ = 0.5f;      // 上下の可動範囲(約29度)

	// レール間分岐移動用の状態
	bool hasPendingBranch_ = false;           // 分岐先が判明し、乗り移り待ちかどうか
	int pendingBranchTargetRailIndex_ = -1;   // 乗り移り先レールのインデックス
	int pendingBranchTargetPointIndex_ = -1;  // 乗り移り先レール側の対応する制御点インデックス

	// テスト用の的
	struct Target{
		std::unique_ptr<Obj3D> obj;
		Vector3 position;
		bool isAlive = true;
	};
	std::vector<Target> targets_;

	// Hierarchy/Inspectorで選択中の的のインデックス(-1は未選択)
	int selectedTargetIndex_ = -1;

	// 弾(プロジェクタイル)。プレイヤーが発射し、的との距離判定でヒットを取る
	struct Bullet{
		std::unique_ptr<Obj3D> obj;
		Vector3 position;
		Vector3 velocity;
		float lifeTime = 0.0f;
		bool isAlive = true;
	};
	std::vector<Bullet> bullets_;

	// 弾の移動速度(1秒あたりの移動量)
	const float kBulletSpeed_ = 40.0f;
	// 弾の表示スケール
	const float kBulletScale_ = 0.15f;
	// 弾が的に命中したとみなす距離(弾と的の中心間距離がこの値以下でヒット)
	const float kBulletHitRadius_ = 0.6f;
	// 弾が的に当たらなかった場合に消滅するまでの生存時間(秒)
	const float kBulletLifeTime_ = 2.0f;

	// 照準判定の許容角度(ラジアン)。画面中央のレティクルがこの角度以内に的を捉えていればヒット
	const float kAimHitAngle_ = 0.09f; // 約5度

	// 画面中央固定のレティクル(照準)を表示するスプライト
	// 外枠(常時表示)と中心ドット(狙えているときに強調表示)の2枚構成
	std::unique_ptr<Sprite> reticleOutlineSprite_;
	std::unique_ptr<Sprite> reticleCenterSprite_;
};