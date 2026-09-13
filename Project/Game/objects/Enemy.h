#pragma once
#include "MyMath.h"
#include <memory>
#include <vector>

// クラスの前方宣言
class Obj3D;
class Obj3dCommon;

/// <summary>
/// 雑魚敵
///
/// 指定された基準座標を中心に、決められた方向へ一定距離を往復する固定パターンで移動する。
/// プレイヤーが検知範囲内に入るとそちらを向き、弾が当たると撃破される。
/// 検知中は一定間隔でプレイヤーへ向けて弾を発射する。
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

	// 描画処理(撃破済みのときは本体を描画しない。発射済みの弾は残っていれば描画する)
	void Draw();

	// 初期状態(生存・移動位置の中心)へ戻す(Play開始時のリセット用)
	void Reset();
	// 撃破する
	void Kill();

	// ここから追加: 敵弾とプレイヤーの当たり判定
	/// <summary>
	/// 発射済みの弾とプレイヤーの当たり判定を行い、命中した弾を消滅させる
	/// </summary>
	/// <param name="playerPosition">プレイヤーのワールド座標</param>
	/// <param name="playerHitRadius">プレイヤーに命中したとみなす、プレイヤー中心からの距離</param>
	/// <returns>このフレームでプレイヤーに命中した弾の数</returns>
	int CheckHitToPlayer(const Vector3& playerPosition,float playerHitRadius);

	// 発射済みで生存している弾の数を取得(デバッグ表示用)
	int GetActiveBulletCount() const;
	// ここまで追加

	// 現在のワールド座標を取得(当たり判定・撃破演出の発生位置に使用)
	const Vector3& GetPosition() const{ return position_; }
	// 生存しているかどうかを取得
	bool IsAlive() const{ return isAlive_; }
	// プレイヤーを検知しているかどうかを取得(デバッグ表示用)
	bool IsDetectingPlayer() const{ return isDetectingPlayer_; }
	// 弾が命中したとみなす半径を取得
	float GetHitRadius() const{ return kHitRadius; }

private:
	// ここから追加: 敵弾
	// 発射のたびに生成・破棄すると無駄が多いため、初期化時に必要数だけ作っておき使い回す
	struct Bullet{
		std::unique_ptr<Obj3D> obj;              // 描画用の3Dオブジェクト(初期化時に一度だけ生成する)
		Vector3 position = {0.0f, 0.0f, 0.0f};   // 現在のワールド座標
		Vector3 velocity = {0.0f, 0.0f, 0.0f};   // 1秒あたりの移動量
		float lifeTime = 0.0f;                   // 発射してからの経過時間(秒)
		bool isAlive = false;                    // 使用中かどうか(falseなら未使用=再利用できる)
	};

	// 弾の更新処理(移動・寿命切れの判定・描画用トランスフォームの更新)
	void UpdateBullets(float deltaTime);
	// プレイヤーへ向けて弾を1発発射する(未使用の弾が無いときは何もしない)
	void FireBullet(const Vector3& playerPosition);
	// ここまで追加

	std::unique_ptr<Obj3D> obj_; // 描画用の3Dオブジェクト

	// ここから追加: 敵弾の管理用
	std::vector<Bullet> bullets_;  // 使い回す弾の実体(サイズはkMaxBulletCountで固定)
	float shotTimer_ = 0.0f;       // 次の発射までの残り時間(秒)
	// ここまで追加

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

	// ここから追加: 敵弾のパラメータ
	// 同時に存在できる弾の最大数(この数だけ初期化時に生成して使い回す)
	static constexpr size_t kMaxBulletCount = 16;
	// 弾を発射する間隔(秒)
	static constexpr float kShotInterval = 1.2f;
	// 弾の移動速度(1秒あたりの移動量)
	static constexpr float kBulletSpeed = 18.0f;
	// 弾が消滅するまでの生存時間(秒)
	static constexpr float kBulletLifeTime = 4.0f;
	// 弾の表示スケール
	static constexpr float kBulletScale = 0.2f;
	// 弾の発射位置を敵の足元より上へずらす量(胸の高さから撃っているように見せる)
	static constexpr float kBulletSpawnUpOffset = 0.5f;
	// ここまで追加
};
