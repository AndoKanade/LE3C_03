#include "Enemy.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include <cmath>
#include <string>

namespace{
	// 雑魚敵の表示に使用するモデル(プレイヤーと区別できるよう別のモーションのモデルを使う)
	const std::string kEnemyModelPath = "human/sneakWalk.gltf";
}

Enemy::Enemy() = default;
Enemy::~Enemy() = default;

// 初期化処理
void Enemy::Initialize(Obj3dCommon* objCommon,const Vector3& basePosition,const Vector3& patrolDirection){
	// 描画用モデルの読み込みと3Dオブジェクトの生成
	ModelManager::GetInstance()->LoadModel(kEnemyModelPath);
	obj_ = std::make_unique<Obj3D>();
	obj_->Initialize(objCommon);
	obj_->SetModel(kEnemyModelPath);

	basePosition_ = basePosition;
	patrolDirection_ = Normalize(patrolDirection);

	Reset();
}

// 初期状態(生存・移動位置の中心)へ戻す
void Enemy::Reset(){
	position_ = basePosition_;
	patrolOffset_ = 0.0f;
	patrolSign_ = 1.0f;
	rotationY_ = 0.0f;
	isAlive_ = true;
	isDetectingPlayer_ = false;
}

// 撃破する
void Enemy::Kill(){
	isAlive_ = false;
}

// 更新処理
void Enemy::Update(const Vector3& playerPosition,float deltaTime){
	// 撃破済みのときは移動も向きの更新も行わない
	if(!isAlive_){
		return;
	}

	// 固定パターンの移動
	// 中心座標からのずれを進行向きに応じて増減させ、可動範囲の端に達したら折り返す
	patrolOffset_ += patrolSign_ * kMoveSpeed * deltaTime;
	if(patrolOffset_ > kPatrolRange){
		patrolOffset_ = kPatrolRange;
		patrolSign_ = -1.0f;
	} else if(patrolOffset_ < -kPatrolRange){
		patrolOffset_ = -kPatrolRange;
		patrolSign_ = 1.0f;
	}
	position_ = basePosition_ + patrolDirection_ * patrolOffset_;

	// プレイヤーの検知判定(検知範囲内かどうか)
	Vector3 toPlayer = playerPosition - position_;
	isDetectingPlayer_ = (Length(toPlayer) <= kDetectionRange);

	// 向きの決定
	// 検知中はプレイヤーの方向、非検知中は移動している方向を向く
	Vector3 facingDirection = isDetectingPlayer_?toPlayer:(patrolDirection_ * patrolSign_);

	// XZ平面での向き(Y軸回転)を求める。真上・真下方向しか成分が無い場合は前フレームの向きを維持する
	if(std::fabs(facingDirection.x) > 1e-5f || std::fabs(facingDirection.z) > 1e-5f){
		rotationY_ = std::atan2(facingDirection.x,facingDirection.z);
	}

	// 描画用オブジェクトへ反映する
	if(obj_){
		obj_->SetTranslate(position_);
		obj_->SetRotate({0.0f, rotationY_, 0.0f});
		obj_->SetScale({kScale, kScale, kScale});

		// アクティブカメラが切り替わっても正しく描画されるよう毎フレーム同期
		if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
			obj_->SetCamera(activeCamera);
		}

		obj_->Update();
	}
}

// 描画処理(撃破済みのときは描画しない)
void Enemy::Draw(){
	if(!isAlive_ || !obj_){
		return;
	}
	obj_->Draw();
}
