#pragma once
#include "MyMath.h"
#include <memory>

// クラスの前方宣言
class Obj3D;
class Obj3dCommon;

/// <summary>
/// 雑魚敵
///
/// 指定された基準座標を中心に、決められた方向へ一定距離を往復する固定パターンで移動する。
/// プレイヤーが検知範囲内に入るとそちらを向き、弾が当たると撃破される。
/// </summary>
class Enemy{
public:
	Enemy();
	~Enemy();

	/// <summary>
	/// 初期化処理
	/// </summary>
	/// <param name="objCommon">3Dオブジェクト共通設定</param>
	/// <param name="basePosition">往復移動の中心となるワールド座標</param>
	/// <param name="patrolDirection">往復移動を行う方向(内部で正規化する)</param>
	void Initialize(Obj3dCommon* objCommon,const Vector3& basePosition,const Vector3& patrolDirection);

	/// <summary>
	/// 更新処理
	/// </summary>
	/// <param name="playerPosition">プレイヤーのワールド座標(検知・向きの計算に使用)</param>
	/// <param name="deltaTime">経過時間(秒)。0を渡すと移動せず表示更新のみ行う</param>
	void Update(const Vector3& playerPosition,float deltaTime);

	// 描画処理(撃破済みのときは描画しない)
	void Draw();

	// 初期状態(生存・移動位置の中心)へ戻す(Play開始時のリセット用)
	void Reset();
	// 撃破する
	void Kill();

	// 現在のワールド座標を取得(当たり判定・撃破演出の発生位置に使用)
	const Vector3& GetPosition() const{ return position_; }
	// 生存しているかどうかを取得
	bool IsAlive() const{ return isAlive_; }
	// プレイヤーを検知しているかどうかを取得(デバッグ表示用)
	bool IsDetectingPlayer() const{ return isDetectingPlayer_; }
	// 弾が命中したとみなす半径を取得
	float GetHitRadius() const{ return kHitRadius; }

private:
	std::unique_ptr<Obj3D> obj_; // 描画用の3Dオブジェクト

	Vector3 basePosition_ = {0.0f, 0.0f, 0.0f};    // 往復移動の中心座標
	Vector3 patrolDirection_ = {1.0f, 0.0f, 0.0f}; // 往復移動の方向(正規化済み)
	Vector3 position_ = {0.0f, 0.0f, 0.0f};        // 現在のワールド座標

	float patrolOffset_ = 0.0f; // 中心座標からのずれ(往復のたびに符号が反転する)
	float patrolSign_ = 1.0f;   // 現在の進行向き(+1で正方向、-1で逆方向)
	float rotationY_ = 0.0f;    // 現在のY軸回転(向き)

	bool isAlive_ = true;             // 生存フラグ(falseで撃破済み)
	bool isDetectingPlayer_ = false;  // プレイヤーを検知しているかどうか

	// 往復移動の速度(1秒あたりの移動量)
	static constexpr float kMoveSpeed = 3.0f;
	// 中心座標から片側へ動ける距離(この距離に達すると折り返す)
	static constexpr float kPatrolRange = 5.0f;
	// プレイヤーを検知する距離(この距離以内ならプレイヤーの方を向く)
	static constexpr float kDetectionRange = 25.0f;
	// 弾が命中したとみなす、敵の中心からの距離
	static constexpr float kHitRadius = 1.0f;
	// 表示スケール
	static constexpr float kScale = 0.4f;
};
