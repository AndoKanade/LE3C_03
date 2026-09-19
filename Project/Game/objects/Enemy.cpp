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
	// ここから追加: 敵弾の表示に使用するモデル
	const std::string kEnemyBulletModelPath = "Sphere/sphere.obj";
	// ここまで追加
}

Enemy::Enemy() = default;
Enemy::~Enemy() = default;

// 初期化処理
void Enemy::Initialize(Obj3dCommon* objCommon,const Vector3& basePosition,const Vector3& patrolDirection,int maxHp){
	// 描画用モデルの読み込みと3Dオブジェクトの生成
	ModelManager::GetInstance()->LoadModel(kEnemyModelPath);
	obj_ = std::make_unique<Obj3D>();
	obj_->Initialize(objCommon);
	obj_->SetModel(kEnemyModelPath);

	basePosition_ = basePosition;
	patrolDirection_ = Normalize(patrolDirection);

	// ここから追加: 体力の最大値を設定する(Reset()で現在の体力がこの値まで回復する)
	SetMaxHp(maxHp);
	// ここまで追加

	// ここから追加: 敵弾の生成
	// 発射のたびに生成すると無駄な処理が毎フレーム発生するため、ここで最大数ぶんまとめて作り、以降は使い回す
	ModelManager::GetInstance()->LoadModel(kEnemyBulletModelPath);
	bullets_.resize(kMaxBulletCount);
	for(auto& bullet : bullets_){
		bullet.obj = std::make_unique<Obj3D>();
		bullet.obj->Initialize(objCommon);
		bullet.obj->SetModel(kEnemyBulletModelPath);
		bullet.obj->SetScale({kBulletScale, kBulletScale, kBulletScale});
	}
	// ここまで追加

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

	// ここから追加: 体力を最大値まで戻す
	hp_ = maxHp_;
	// ここまで追加

	// ここから追加: 発射済みの弾をすべて未使用に戻し、発射間隔も初期化する
	for(auto& bullet : bullets_){
		bullet.isAlive = false;
		bullet.lifeTime = 0.0f;
	}
	shotTimer_ = kShotInterval;
	// ここまで追加
}

// 撃破する
void Enemy::Kill(){
	isAlive_ = false;

	// ここから追加: 撃破時は体力も0にして、表示と状態を食い違わせないようにする
	hp_ = 0;
	// ここまで追加
}

// ここから追加: 体力を減らす(撃破されたときtrueを返す)
bool Enemy::TakeDamage(int damage){
	// 撃破済みの敵はこれ以上体力が減らない
	if(!isAlive_){
		return false;
	}

	hp_ -= damage;
	if(hp_ <= 0){
		hp_ = 0;
		isAlive_ = false;
		return true;
	}

	return false;
}
// ここまで追加

// ここから追加: 往復移動の中心座標を設定する(現在の往復位置を保ったまま移動させる)
void Enemy::SetBasePosition(const Vector3& basePosition){
	basePosition_ = basePosition;
	position_ = basePosition_ + patrolDirection_ * patrolOffset_;
}
// ここまで追加

// ここから追加: 往復移動の方向を設定する(内部で正規化する)
void Enemy::SetPatrolDirection(const Vector3& patrolDirection){
	patrolDirection_ = Normalize(patrolDirection);
	position_ = basePosition_ + patrolDirection_ * patrolOffset_;
}
// ここまで追加

// ここから追加: 体力の最大値を設定する(現在の体力が最大値を超える場合は最大値に合わせる)
void Enemy::SetMaxHp(int maxHp){
	// 0以下だと生成直後に撃破された状態になってしまうため下限で制限する
	maxHp_ = (maxHp < kMinMaxHp)?kMinMaxHp:maxHp;
	if(hp_ > maxHp_){
		hp_ = maxHp_;
	}
}
// ここまで追加

// 更新処理
void Enemy::Update(const Vector3& playerPosition,float deltaTime){
	// ここから追加: 撃破済みでも発射済みの弾は飛び続けさせるため、弾の更新は本体より先に行う
	UpdateBullets(deltaTime);
	// ここまで追加

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

	// ここから追加: 弾の発射処理
	// プレイヤーを検知している間だけ、一定間隔でプレイヤーへ向けて撃つ
	if(isDetectingPlayer_){
		shotTimer_ -= deltaTime;
		if(shotTimer_ <= 0.0f){
			FireBullet(playerPosition);
			shotTimer_ = kShotInterval;
		}
	} else{
		// 非検知中は撃たない。次に検知した直後に即撃ちされないよう、待ち時間を戻しておく
		shotTimer_ = kShotInterval;
	}
	// ここまで追加

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

// 描画処理(撃破済みのときは本体を描画しない。発射済みの弾は残っていれば描画する)
void Enemy::Draw(){
	if(isAlive_ && obj_){
		obj_->Draw();
	}

	// ここから追加: 発射中の弾を描画する
	for(auto& bullet : bullets_){
		if(bullet.isAlive && bullet.obj){
			bullet.obj->Draw();
		}
	}
	// ここまで追加
}

// ここから追加: 弾の更新処理(移動・寿命切れの判定・描画用トランスフォームの更新)
void Enemy::UpdateBullets(float deltaTime){
	// アクティブカメラは全弾で共通なので、ループの外で一度だけ取得する
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();

	for(auto& bullet : bullets_){
		// 未使用の弾は移動も描画更新も不要
		if(!bullet.isAlive){
			continue;
		}

		// 等速直線運動で前進させる(プレイヤーが避けやすいよう重力は掛けない)
		bullet.position += bullet.velocity * deltaTime;

		// 何にも当たらなかった弾は一定時間で未使用に戻し、再利用できるようにする
		bullet.lifeTime += deltaTime;
		if(bullet.lifeTime >= kBulletLifeTime){
			bullet.isAlive = false;
			continue;
		}

		// 描画用オブジェクトへ反映する
		if(bullet.obj){
			bullet.obj->SetTranslate(bullet.position);
			if(activeCamera){
				bullet.obj->SetCamera(activeCamera);
			}
			bullet.obj->Update();
		}
	}
}
// ここまで追加

// ここから追加: プレイヤーへ向けて弾を1発発射する
void Enemy::FireBullet(const Vector3& playerPosition){
	// 未使用の弾を探して使い回す(全弾使用中のときは発射しない)
	for(auto& bullet : bullets_){
		if(bullet.isAlive){
			continue;
		}

		// 発射位置は敵の中心より少し上(胸の高さ)にする
		Vector3 spawnPosition = position_;
		spawnPosition.y += kBulletSpawnUpOffset;

		// 発射した瞬間のプレイヤー位置へ向かう方向を求める(以降は追尾しない)
		Vector3 toPlayer = playerPosition - spawnPosition;
		if(Length(toPlayer) < 1e-5f){
			return; // プレイヤーと発射位置がほぼ同じ場合は方向が定まらないため撃たない
		}

		bullet.position = spawnPosition;
		bullet.velocity = Normalize(toPlayer) * kBulletSpeed;
		bullet.lifeTime = 0.0f;
		bullet.isAlive = true;
		return;
	}
}
// ここまで追加

// ここから追加: 発射済みの弾とプレイヤーの当たり判定
int Enemy::CheckHitToPlayer(const Vector3& playerPosition,float playerHitRadius){
	int hitCount = 0;

	for(auto& bullet : bullets_){
		if(!bullet.isAlive){
			continue;
		}

		// 弾とプレイヤーの中心間距離が許容半径以下なら命中とみなし、その弾を未使用に戻す
		if(Length(playerPosition - bullet.position) <= playerHitRadius){
			bullet.isAlive = false;
			++hitCount;
		}
	}

	return hitCount;
}
// ここまで追加

// ここから追加: 発射済みで生存している弾の数を取得(デバッグ表示用)
int Enemy::GetActiveBulletCount() const{
	int count = 0;
	for(const auto& bullet : bullets_){
		if(bullet.isAlive){
			++count;
		}
	}
	return count;
}
// ここまで追加
