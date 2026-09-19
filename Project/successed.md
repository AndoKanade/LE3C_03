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

---

## 2026-09-19

### 目標

- 09/16まで: 体力が0になったときにシーンを切り替えて GAMEOVER シーンを作る
- 09/20まで: 敵の配置をエディターで置けるようにする
- 余裕があれば: 敵にも体力を入れる

### 1. ゲームオーバーシーン

対象ファイル: `Game/scenes/GameOverScene.h` / `Game/scenes/GameOverScene.cpp`（新規）、`Game/systems/SceneFactory.cpp`、`Game/scenes/GameScene.cpp`

- ClearScene と同じ構成で GameOverScene を新規作成した。専用素材が無いため、背景は既存の `resource/circle.png` を赤系の色（`kBackgroundColor` = 1.0, 0.3, 0.3, 0.6）で表示している。
- 文字表示は専用のフォント描画機能が無いため、ClearScene と同様に ImGui で "GAME OVER" と操作説明を表示している。
- SPACE キーでタイトルへ戻る。
- SceneFactory に `"GAMEOVER"` の分岐を追加した。
- GameScene 側では、敵弾の被弾処理の直後に判定を追加した。Playモード中に `playerHp_` が0以下になると `sceneManager_->ChangeScene("GAMEOVER")` を呼ぶ。

前回の残課題だった「体力が0になっても何も起きない」状態が解消された。

### 2. 敵の配置エディター

対象ファイル: `Game/Editor/EnemyEditor.h` / `Game/Editor/EnemyEditor.cpp`（新規）、`Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp`

TargetEditor と同じ操作感になるよう、同じ構成で新規作成した。

- Editモード中のみ動作する（Playモード中は即 return してドラッグ状態も解除する）。
- `Add Enemy`: 現在のカメラ前方 `kSpawnDistance`（15.0）の位置に敵を1体追加する。追加した敵はそのまま選択状態になる。
- Sceneビュー上で敵を左ドラッグして移動できる。掴んだ瞬間のレイ方向を法線とする平面上を動かすため、画面に平行な移動になる。掴める距離は `kPickRadius`（1.0）。
- `Delete Enemy`: 選択中の敵を削除する（未選択のときは押せない）。
- 敵ごとに `Position` / `Patrol Dir`（往復移動の方向） / `Max HP` をボタンUIで編集できる。往復方向が0ベクトルに近くなったときは既定値 (1, 0, 0) に戻す。
- `Save` / `Load` で `resource/enemy/enemy.json` へ保存・再読込できる。保存形式は `position` / `patrolDirection` / `maxHp`。古い保存データに `patrolDirection` や `maxHp` が無くても既定値で補って読めるようにした。
- 配置された敵の一覧を表示し、クリックで選択できるようにした。

GameScene 側の変更:

- 単体の `std::unique_ptr<Enemy> enemy_` を `std::vector<std::unique_ptr<Enemy>> enemies_` に変更した。
- `SyncEnemiesFromEditor()` を追加した。的（`SyncTargetsFromEditor`）と同じ方式で、**体数が変わったときだけ** Enemy の実体を生成・削除し、毎フレームの生成が発生しないようにした。座標・往復方向・最大HPは毎フレーム上書きするので、エディターでの編集が即座に反映される。
- 保存データが無い初回起動時のみ、従来どおりレール中間地点（`kEnemySpawnRailT_` = 0.5）の少し上へ1体を自動配置する。
- Play開始時のリセットで全ての敵を `Reset()` するようにした。
- 敵弾とプレイヤーの当たり判定は全ての敵に対して行い、命中数を合計してから体力を1減らす。複数の敵が同時に撃っていても、当たった弾はすべて消える。

### 3. 敵の体力

対象ファイル: `Game/objects/Enemy.h` / `Game/objects/Enemy.cpp` / `Game/scenes/GameScene.cpp`

- Enemy に `hp_` / `maxHp_` を追加した。`Reset()` で最大値まで回復する。
- `TakeDamage(damage)` を追加した。撃破済みの敵には効果が無く、体力が0になったときだけ `isAlive_` を false にして true を返す。
- `Initialize()` に `maxHp` 引数を追加し、配置エディターからの編集反映用に `SetBasePosition` / `SetPatrolDirection` / `SetMaxHp` を追加した。`SetMaxHp` は `kMinMaxHp`（1）を下回らないよう制限する（0以下だと生成直後に撃破された状態になってしまうため）。
- `Kill()` は体力も0にして、表示と状態が食い違わないようにした。
- GameScene のプレイヤー弾の判定を、1発で撃破する方式から `kBulletDamageToEnemy_`（1）ぶん体力を減らす方式に変更した。当たるたびに火花パーティクルが出て、体力が0になったときだけ撃破される。

追加した定数:

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `Enemy::kDefaultMaxHp` | 3 | 体力の最大値の初期値 |
| `Enemy::kMinMaxHp` | 1 | 最大HPとして設定できる下限 |
| `EnemyEditor::kDefaultMaxHp` | 3 | Add Enemy で追加したときの最大HP |
| `EnemyEditor::kMinMaxHp` | 1 | エディターで設定できる最大HPの下限 |
| `EnemyEditor::kSpawnDistance` | 15.0f | 新規追加時にカメラ前方へ置く距離 |
| `EnemyEditor::kPickRadius` | 1.0f | マウスで敵を掴めるとみなす距離 |
| `GameScene::kBulletDamageToEnemy_` | 1 | プレイヤーの弾1発が敵に与えるダメージ量 |

### 4. エディタパネルのレイアウト調整

対象ファイル: `Engine/Manager/EditorWidgets.h`、`Game/Editor/TargetEditor.cpp`、`Game/Editor/EnemyEditor.cpp`

Enemy Editor を追加した時点で、下段に置くパネルが4枚（GameScene Debug / Target Editor / Enemy Editor / PostProcess Settings）になり、同じ位置に重なって見えなくなる問題があった。

- `Layout` に `bottomCenterLeft` / `bottomCenterRight` を追加し、`ComputeLayout()` で下段を4等分するようにした。右端のパネルだけは割り算の余りを含めた幅にして、下段に隙間ができないようにしている。
- Target Editor を `bottomCenterLeft`、Enemy Editor を `bottomCenterRight` に割り当てた。GameScene Debug（`bottomLeft`）と PostProcess Settings（`bottomRight`）は割り当て名の変更なしで、それぞれ左端・右端に配置される。

### 5. デバッグ表示

対象ファイル: `Game/scenes/GameScene.cpp`

ImGui の "Rail Branch Debug" ウィンドウの敵の表示を、複数体に対応した内容へ変更した。

- `Enemies`: 配置されている体数と生存数
- `Enemy Bullets`: 全ての敵の発射中の弾の合計数
- `Enemy n HP`: 敵ごとの現在HP / 最大HP と検知状態

生存数と弾の合計は、体ごとの表示と同じループでまとめて数えるようにした。

### その他

- `MyGameEngine.vcxproj` / `MyGameEngine.vcxproj.filters` に新規4ファイル（EnemyEditor.h/.cpp、GameOverScene.h/.cpp）を登録した。
- `CLAUDE.md` のロードマップと「現状の課題」を現状に合わせて更新した（1〜4を完了、解決済み項目を 3.3 へ移動）。

### ビルド確認

`MyGameEngine.vcxproj` を Debug / x64 でビルドし、エラー・警告なしで成功することを確認した。

### 残課題

- 敵の配置は `Save` を押さないと次回起動時に反映されない（自動保存は未対応）。
- 体力・敵のHP・撃破数などの HUD 表示（スプライト）が未実装で、現状は ImGui のデバッグ表示のみ。前回からの持ち越し。
- ゲームオーバー画面の "GAME OVER" 表示も、クリア画面と同じく ImGui での代用のまま。
- プレイヤーの弾は発射のたびに `make_unique<Obj3D>` している。敵弾は使い回しているので、同じ方式に揃えたい。
- 敵の体力以外のパラメータ（移動速度・検知範囲・発射間隔など）は `Enemy.h` の定数固定のままで、敵ごとの設定には対応していない。
