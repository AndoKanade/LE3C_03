# 実装記録

日付ごとに、実際に実装・完了した内容を記録する。

---

## 2026-09-13

### 目標

- 09/08まで: 敵が自機に向かって弾を撃ってくるようにする
- 09/13まで: 自機と敵の弾との当たり判定を実装する
- 余裕があれば: 自機に体力を実装し、弾が当たると体力を減らす

### 1. 敵の弾の発射

対象ファイル: `Game/objects/Enemy.h` / `Game/objects/Enemy.cpp`

- Enemy クラス内に敵弾（`struct Bullet`）を追加した。
- 弾は `Initialize` で最大数ぶん（`kMaxBulletCount = 16`）まとめて生成し、以降は `isAlive` フラグで使い回す方式にした。発射のたびに `make_unique` するのを避けるため。
- プレイヤーを検知している間だけ、`kShotInterval`（1.2秒）ごとに1発発射する。非検知中は発射せず、待ち時間を戻して次の検知直後に即撃ちされないようにした。
- 弾は発射した瞬間のプレイヤー座標へ向かう等速直線運動（追尾なし・重力なし）。発射位置は敵の中心から `kBulletSpawnUpOffset`（0.5）ぶん上。
- 弾は `kBulletLifeTime`（4秒）で未使用に戻り、再利用される。
- 敵が撃破されても、発射済みの弾は飛び続けるようにした（`UpdateBullets` を本体の生存チェックより先に呼ぶ構成）。
- `Reset()` で弾と発射間隔も初期化されるため、Play開始時に前回の弾を持ち越さない。

追加した定数（すべて `Enemy.h` 内の `static constexpr`）:

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `kMaxBulletCount` | 16 | 同時に存在できる弾の最大数 |
| `kShotInterval` | 1.2f | 発射間隔（秒） |
| `kBulletSpeed` | 18.0f | 弾の移動速度（1秒あたり） |
| `kBulletLifeTime` | 4.0f | 弾の生存時間（秒） |
| `kBulletScale` | 0.2f | 弾の表示スケール |
| `kBulletSpawnUpOffset` | 0.5f | 発射位置を敵の中心より上へずらす量 |

### 2. 自機と敵弾の当たり判定

対象ファイル: `Game/objects/Enemy.cpp` / `Game/scenes/GameScene.cpp`

- `Enemy::CheckHitToPlayer(playerPosition, playerHitRadius)` を追加。発射済みの弾とプレイヤーの中心間距離で判定し、命中した弾を未使用に戻して命中数を返す。
- GameScene 側では、雑魚敵の更新直後にこの判定を呼ぶようにした。
- 命中した弾は無敵時間中でもその場で消滅させ、すり抜けて後から当たることがないようにした。

### 3. 自機の体力

対象ファイル: `Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp`

- `playerHp_`（最大 `kPlayerMaxHp_` = 3）と、被弾後の無敵時間 `playerInvincibleTimer_` を追加した。
- 被弾すると体力を1減らし、`kPlayerInvincibleTime_`（1秒）の無敵時間を設定する。同一フレームで複数の弾が当たっても減少は1回分だけ。
- 被弾位置に火花パーティクル（`ParticleManager::EmitSpark`）を発生させ、当たったことが見た目で分かるようにした。
- Play開始時のリセット処理で、体力と無敵時間も初期状態へ戻すようにした。

追加した定数（`GameScene.h`）:

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `kPlayerMaxHp_` | 3 | 体力の最大値 |
| `kPlayerHitRadius_` | 1.0f | 敵弾が命中したとみなす距離 |
| `kPlayerInvincibleTime_` | 1.0f | 被弾後の無敵時間（秒） |

### 4. デバッグ表示

対象ファイル: `Game/scenes/GameScene.cpp`

ImGui の "Rail Branch Debug" ウィンドウに以下を追加した。

- `Enemy Bullets`: 発射中の敵弾の数
- `Player HP`: 現在の体力 / 最大体力
- `Player Invincible`: 無敵時間の残り（秒）

### ビルド確認

`MyGameEngine.sln` を Debug / x64 でビルドし、エラー・警告なしで成功することを確認した。

### 残課題

- 体力が0になっても現状は何も起きない（体力の減少が止まるだけ）。SceneFactory に GAME_OVER 相当のシーンが無いため、ゲームオーバー処理を入れるには別途シーンの追加が必要。
- 体力や被弾状態の HUD 表示（スプライト）が未実装で、現状は ImGui のデバッグ表示のみ。
- 敵弾のパラメータ（発射間隔・速度など）が `Enemy.h` の定数固定で、GlobalVariables による実行中の調整に対応していない。
