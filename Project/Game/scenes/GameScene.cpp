#include "GameScene.h"
#include "CameraManager.h"
#include "ImGuiManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "SoundManager.h"
#include "TextureManager.h"
#include "Skybox.h"
#include "SkyboxCommon.h"
#include "Input.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "WinAPI.h"
#include "Application.h"
#include "Logger.h"
#include "GlobalVariables.h"
#include "EditorWidgets.h"
#include "EditorContext.h"
#include <algorithm>
#include <cstdio>

// レールエディター
#include "Editor/RailEditor.h"

namespace{
	// スカイボックスのテクスチャパス
	const std::string kSkyboxTexture = "resource/Skybox/rostock_laage_airport_4k.dds";
	// GlobalVariablesのグループ名(GameSceneの調整項目)
	const char* kGameSceneGroup = "GameScene";

	// 撃破演出(パーティクル)関連
	// 変更箇所: 撃破演出の追加
	// パーティクルに使用するテクスチャパス
	const std::string kHitParticleTexture = "resource/circle.png";
	// ParticleManager::EmitSpark()が内部で使用するグループ名と合わせる必要がある
	const char* kHitParticleGroupName = "Spark";
}

GameScene::GameScene() = default;
GameScene::~GameScene() = default;

// シーンの初期化
void GameScene::Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon){
	object3dCommon_ = object3dCommon;
	input_ = input;
	spriteCommon_ = spriteCommon;

	// カメラの生成と設定
	CameraManager::GetInstance()->CreateCamera("default",object3dCommon_->GetDxCommon()->GetDevice());
	auto* defaultCamera = CameraManager::GetInstance()->GetCamera("default");
	defaultCamera->SetTranslate({0.0f, 0.0f, -30.0f});
	CameraManager::GetInstance()->SetActiveCamera("default");
	object3dCommon_->SetDefaultCamera(CameraManager::GetInstance()->GetActiveCamera());

	// テクスチャの読み込み
	TextureManager::GetInstance()->LoadTexture(kSkyboxTexture);

	// スカイボックスの生成と初期化
	skyboxCommon_ = std::make_unique<SkyboxCommon>();
	skyboxCommon_->Initialize(object3dCommon_->GetDxCommon());
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(skyboxCommon_.get(),kSkyboxTexture);

	// レールエディターの生成と初期化
	railEditor_ = std::make_unique<RailEditor>();
	railEditor_->Initialize(object3dCommon_);

	// プレイヤー(人型モデル)の読み込みと生成
	ModelManager::GetInstance()->LoadModel("human/walk.gltf");
	player_ = std::make_unique<Obj3D>();
	player_->Initialize(object3dCommon_);
	player_->SetModel("human/walk.gltf");

	// 調整項目(GlobalVariables)にゲームプレイ用パラメータを登録
	// ImGuiの "Global Variables" ウィンドウから実行中に編集・保存でき、
	// resource/GlobalVariables/GameScene.json を外部で書き換えると自動反映される(ホットリロード)
	{
		GlobalVariables* gv = GlobalVariables::GetInstance();
		gv->CreateGroup(kGameSceneGroup);
		// 第3引数はデフォルト値。保存済みJSONがあればそちらが優先される(AddItemは未登録キーのみ追加)
		gv->AddItem(kGameSceneGroup,"railSpeed",railSpeed_);
		gv->AddItem(kGameSceneGroup,"cameraHeightOffset",kCameraHeightOffset_);
		// 照準(エイム)まわりの手触り調整用パラメータ
		gv->AddItem(kGameSceneGroup,"aimSpeed",kAimSpeed_);             // 1秒あたりの回転量(ラジアン)
		gv->AddItem(kGameSceneGroup,"aimYawLimit",kAimYawLimit_);       // 左右の可動範囲
		gv->AddItem(kGameSceneGroup,"aimPitchLimit",kAimPitchLimit_);   // 上下の可動範囲
		gv->AddItem(kGameSceneGroup,"aimHitAngle",kAimHitAngle_);       // ヒット判定の許容角度
		gv->AddItem(kGameSceneGroup,"noseOffset",2.0f);                 // 向きマーカーを前方に離す距離
	}

	// 俯瞰用のデバッグカメラを生成
	CameraManager::GetInstance()->CreateCamera("debug_top",object3dCommon_->GetDxCommon()->GetDevice());
	auto* debugTopCamera = CameraManager::GetInstance()->GetCamera("debug_top");
	// 真上から見下ろす位置に配置(X回転90度=pi/2で真下向き)
	debugTopCamera->SetTranslate({0.0f, 30.0f, 0.0f});
	debugTopCamera->SetRotate({3.14159265f * 0.5f, 0.0f, 0.0f});

	// カメラ位置を可視化するマーカーを生成
	ModelManager::GetInstance()->LoadModel("Sphere/sphere.obj");
	cameraMarker_ = std::make_unique<Obj3D>();
	cameraMarker_->Initialize(object3dCommon_);
	cameraMarker_->SetModel("Sphere/sphere.obj");

	// カメラの向きを可視化する小さいマーカーを生成
	cameraFacingMarker_ = std::make_unique<Obj3D>();
	cameraFacingMarker_->Initialize(object3dCommon_);
	cameraFacingMarker_->SetModel("Sphere/sphere.obj");

	// 的をレール沿いの複数の進行度(t)に、左右・上下へオフセットして配置(ゲームらしく散らばらせる)
	if(railEditor_){
		constexpr float kTargetRailT[] = {0.1f, 0.25f, 0.4f, 0.55f, 0.7f, 0.85f};
		constexpr float kTargetSideOffset[] = {-3.0f, 3.0f, -2.0f, 2.0f, -3.0f, 3.0f};
		constexpr float kTargetUpOffset[] = {1.0f, 2.0f, 3.0f, 1.5f, 2.5f, 1.0f};
		constexpr size_t kTargetCount = 6;
		constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};

		for(size_t i = 0; i < kTargetCount; ++i){
			// レール上の基準位置と進行方向から、進行方向に対して直角な「右」方向を求める
			Vector3 basePos = railEditor_->GetPositionOnRail(kTargetRailT[i]);
			Vector3 forward = railEditor_->GetForwardOnRail(kTargetRailT[i]);
			Vector3 right = Normalize(Cross(kWorldUp,forward));

			Vector3 pos = basePos + right * kTargetSideOffset[i] + kWorldUp * kTargetUpOffset[i];

			Target target;
			target.obj = std::make_unique<Obj3D>();
			target.obj->Initialize(object3dCommon_);
			target.obj->SetModel("Sphere/sphere.obj");
			target.position = pos;
			target.isAlive = true;
			targets_.push_back(std::move(target));
		}
	}

	// 画面中央固定のレティクルを生成(外枠+中心ドットの2枚構成)
	TextureManager::GetInstance()->LoadTexture("resource/Reticle/reticleOutline.png");
	TextureManager::GetInstance()->LoadTexture("resource/Reticle/reticle.png");

	reticleOutlineSprite_ = std::make_unique<Sprite>();
	reticleOutlineSprite_->Initialize(spriteCommon_,"resource/Reticle/reticleOutline.png");
	reticleOutlineSprite_->SetSize({48.0f, 48.0f});
	reticleOutlineSprite_->SetAnchorPoint({0.5f, 0.5f}); // 中心を基準点にする
	reticleOutlineSprite_->SetPosition({
		static_cast<float>(WinAPI::kClientWidth) * 0.5f,
		static_cast<float>(WinAPI::kClientHeight) * 0.5f
		});

	reticleCenterSprite_ = std::make_unique<Sprite>();
	reticleCenterSprite_->Initialize(spriteCommon_,"resource/Reticle/reticle.png");
	reticleCenterSprite_->SetSize({48.0f, 48.0f});
	reticleCenterSprite_->SetAnchorPoint({0.5f, 0.5f});
	reticleCenterSprite_->SetPosition({
		static_cast<float>(WinAPI::kClientWidth) * 0.5f,
		static_cast<float>(WinAPI::kClientHeight) * 0.5f
		});

	// 変更箇所: 撃破演出の追加
	// 的の撃破時に発生させる火花パーティクルのグループを事前に生成しておく
	TextureManager::GetInstance()->LoadTexture(kHitParticleTexture);
	ParticleManager::GetInstance()->CreateParticleGroup(kHitParticleGroupName,kHitParticleTexture);
}

// シーンの終了処理
void GameScene::Finalize(){}

// シーンの更新処理
void GameScene::Update(){
	// スカイボックスの更新
	if(skybox_){
		skybox_->Update(*CameraManager::GetInstance()->GetActiveCamera());
	}

	// 変更箇所: 撃破演出の追加
	// パーティクルの更新(ビルボード行列・寿命の進行など)
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		ParticleManager::GetInstance()->Update(activeCamera);
	}

	// レティクルの更新(画面中央固定なので位置は変わらないが、内部行列更新のため毎フレーム呼ぶ)
	if(reticleOutlineSprite_) reticleOutlineSprite_->Update();
	if(reticleCenterSprite_) reticleCenterSprite_->Update();

	// レールエディターの更新
	if(railEditor_){
		railEditor_->Update();
	}

	// レール進行度を時間で進めて、カメラをレール上に乗せる
	if(railEditor_){
		// 調整項目から最新の値を取得(ImGui編集/ホットリロードが即反映される)
		GlobalVariables* gv = GlobalVariables::GetInstance();
		railSpeed_ = gv->GetFloatValue(kGameSceneGroup,"railSpeed");
		float cameraHeightOffset = gv->GetFloatValue(kGameSceneGroup,"cameraHeightOffset");
		float aimSpeed = gv->GetFloatValue(kGameSceneGroup,"aimSpeed");
		float aimYawLimit = gv->GetFloatValue(kGameSceneGroup,"aimYawLimit");
		float aimPitchLimit = gv->GetFloatValue(kGameSceneGroup,"aimPitchLimit");
		float aimHitAngle = gv->GetFloatValue(kGameSceneGroup,"aimHitAngle");
		float noseOffset = gv->GetFloatValue(kGameSceneGroup,"noseOffset");

		// エンジンが固定60fps前提(TimeManagerで60fps固定)なので、そのまま合わせる
		const float deltaTime = 1.0f / 60.0f;

		// EditモードではゲームのシミュレーションをUnityのように止める(Playを押して初めて動く)。
		// カメラ位置や的の配置反映などの「表示」は両モードで行い、進行/入力/射撃だけをPlay限定にする。
		const bool isPlayMode = EditorContext::GetInstance()->IsPlayMode();

		// Playに入った瞬間、ゲームを初期状態から開始する(UnityのPlayと同じく毎回リセットして始まる)。
		// これでPlayを押すとレール先頭=編集で見えていた画から始まる。
		if(isPlayMode && !wasPlayMode_){
			railT_ = 0.0f;
			isRailFinished_ = false; // レール終端フラグもリセットする
			aimYawOffset_ = 0.0f;
			aimPitchOffset_ = 0.0f;
			for(auto& t : targets_){
				t.isAlive = true;
			}
			bullets_.clear(); // Play開始時に残っている弾もリセットする
		}
		wasPlayMode_ = isPlayMode;

		// レール進行はPlayモードかつ終端未到達のときだけ進める(Edit中はその位置で静止)
		// 終端に到達したらループさせず、その場で停止させる
		if(isPlayMode && !isRailFinished_){
			// 現在位置に対応する制御点のSpeed値を取得し、全体速度(railSpeed_)に掛けて反映する
			float pointSpeed = railEditor_->GetSpeedOnRail(railT_);
			railT_ += pointSpeed * railSpeed_ * deltaTime;
			if(railT_ >= 1.0f){
				railT_ = 1.0f; // 終端で固定する
				isRailFinished_ = true;
			}
		}

		Vector3 railPos = railEditor_->GetPositionOnRail(railT_);
		Vector3 railRot = railEditor_->GetRotationOnRail(railT_);

		// 三人称視点用に、カメラの実位置はレールそのものではなく少し上に置く
		Vector3 cameraPos = railPos + Vector3{0.0f, cameraHeightOffset, 0.0f};

		// プレイヤー入力で照準(カメラの向き)をレールの向きに上乗せする
		// Edit中は入力を受け付けない(ゲームは静止)
		if(isPlayMode && input_){
			if(input_->PushKey(DIK_LEFT))  aimYawOffset_ -= aimSpeed * deltaTime;
			if(input_->PushKey(DIK_RIGHT)) aimYawOffset_ += aimSpeed * deltaTime;
			if(input_->PushKey(DIK_UP))    aimPitchOffset_ -= aimSpeed * deltaTime;
			if(input_->PushKey(DIK_DOWN))  aimPitchOffset_ += aimSpeed * deltaTime;

			// 可動範囲でクランプ(振り向きすぎないように)
			if(aimYawOffset_ > aimYawLimit) aimYawOffset_ = aimYawLimit;
			if(aimYawOffset_ < -aimYawLimit) aimYawOffset_ = -aimYawLimit;
			if(aimPitchOffset_ > aimPitchLimit) aimPitchOffset_ = aimPitchLimit;
			if(aimPitchOffset_ < -aimPitchLimit) aimPitchOffset_ = -aimPitchLimit;
		}

		// レールの向き + 照準オフセットを最終的なカメラの向きとする
		Vector3 finalRot = {railRot.x + aimPitchOffset_, railRot.y + aimYawOffset_, railRot.z};

		if(Camera* mainCamera = CameraManager::GetInstance()->GetCamera("default")){
			mainCamera->SetTranslate(cameraPos);
			mainCamera->SetRotate(finalRot);
		}

		// カメラマーカーもレール上の位置に追従させる
		if(cameraMarker_){
			cameraMarker_->SetTranslate(cameraPos);
			cameraMarker_->SetScale({0.7f, 0.7f, 0.7f});
			// アクティブカメラが切り替わっても正しく描画されるよう毎フレーム同期
			if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
				cameraMarker_->SetCamera(activeCamera);
			}
			cameraMarker_->Update();
		}

		// カメラの前方ベクトルを計算(finalRotベースの回転行列を適用)
		Matrix4x4 rotateX = MakeRotateXMatrix(finalRot.x);
		Matrix4x4 rotateY = MakeRotateYMatrix(finalRot.y);
		Matrix4x4 rotateZ = MakeRotateZMatrix(finalRot.z);
		Matrix4x4 rotateMatrix = Multiply(Multiply(rotateX,rotateY),rotateZ);

		Vector3 baseForward = {0.0f, 0.0f, 1.0f};
		Vector3 cameraForward = {
			baseForward.x * rotateMatrix.m[0][0] + baseForward.y * rotateMatrix.m[1][0] + baseForward.z * rotateMatrix.m[2][0],
			baseForward.x * rotateMatrix.m[0][1] + baseForward.y * rotateMatrix.m[1][1] + baseForward.z * rotateMatrix.m[2][1],
			baseForward.x * rotateMatrix.m[0][2] + baseForward.y * rotateMatrix.m[1][2] + baseForward.z * rotateMatrix.m[2][2]
		};

		// カメラの向きを表す「鼻」マーカーを、カメラ前方ベクトルの方向に置く
		if(cameraFacingMarker_){
			Vector3 nosePos = cameraPos + cameraForward * noseOffset;

			cameraFacingMarker_->SetTranslate(nosePos);
			cameraFacingMarker_->SetScale({0.3f, 0.3f, 0.3f}); // 本体より小さくして区別
			if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
				cameraFacingMarker_->SetCamera(activeCamera);
			}
			cameraFacingMarker_->Update();
		}

		// プレイヤー(人型モデル)をカメラの前方下(レール上)に配置し、進行方向(レールの向き)を向かせる
		if(player_){
			Vector3 playerPos = railPos + cameraForward * kCameraBackOffset_;
			playerPos.y -= kPlayerDownOffset_; // スプラトゥーン風に、レール位置よりさらに下に表示する

			player_->SetTranslate(playerPos);
			player_->SetRotate(railRot);
			player_->SetScale({kPlayerScale_, kPlayerScale_, kPlayerScale_}); // 小さめのスケールで表示
			if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
				player_->SetCamera(activeCamera);
			}
			player_->Update();
		}

		// 弾の発射処理(SPACEキーを押した瞬間に1発だけ発射する。本実装への置き換えでPushKeyからTriggerKeyに変更)
		bool shootTriggered = isPlayMode && input_ && input_->TriggerKey(DIK_SPACE);
		if(shootTriggered){
			Bullet bullet;
			bullet.obj = std::make_unique<Obj3D>();
			bullet.obj->Initialize(object3dCommon_);
			bullet.obj->SetModel("Sphere/sphere.obj");
			bullet.position = cameraPos;
			bullet.velocity = cameraForward * kBulletSpeed_;
			bullets_.push_back(std::move(bullet));
		}

		// 弾の移動更新と生存時間チェック(的に当たらなくても一定時間で消滅させる)
		for(auto& bullet : bullets_){
			if(!bullet.isAlive) continue;

			bullet.position += bullet.velocity * deltaTime;
			bullet.lifeTime += deltaTime;
			if(bullet.lifeTime >= kBulletLifeTime_){
				bullet.isAlive = false;
			}
		}

		// 的の当たり判定(画面中央固定のレティクル方式)
		// レティクルは常に画面中央=カメラの前方ベクトル方向なので、
		// 「カメラ→的」の方向とカメラ前方ベクトルのなす角が閾値以内なら狙えている(表示上のフィードバック用)
		// 実際の命中判定は、発射した弾と的との距離が一定値以下になったかどうかで行う
		bool isAimingAtAnyTarget = false; // レティクル中心の色変えに使う
		for(auto& target : targets_){
			if(!target.isAlive) continue;

			Vector3 toTarget = target.position - cameraPos;
			float distance = Length(toTarget);
			if(distance < 0.001f) continue; // カメラと的が重なっている異常値は無視

			float angle = AngleBetween(cameraForward,toTarget);
			bool isAimed = angle <= aimHitAngle;
			if(isAimed){
				isAimingAtAnyTarget = true;
			}

			// 生存している弾との距離判定(中心間距離がkBulletHitRadius_以下ならヒット)
			for(auto& bullet : bullets_){
				if(!bullet.isAlive) continue;
				if(Length(target.position - bullet.position) <= kBulletHitRadius_){
					target.isAlive = false;
					bullet.isAlive = false;
					// 変更箇所: 撃破演出の追加
					// 的の撃破位置に火花パーティクルを発生させる
					ParticleManager::GetInstance()->EmitSpark(target.position);
					break;
				}
			}

			if(target.obj){
				target.obj->SetTranslate(target.position);
				// 狙えているときは少し大きくして視覚的にフィードバック
				float scale = isAimed?0.6f:0.4f;
				target.obj->SetScale({scale, scale, scale});
				if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
					target.obj->SetCamera(activeCamera);
				}
				target.obj->Update();
			}
		}

		// 命中または生存時間切れで消えた弾をリストから削除する
		bullets_.erase(
			std::remove_if(bullets_.begin(),bullets_.end(),[](const Bullet& b){ return !b.isAlive; }),
			bullets_.end());

		// 弾のトランスフォームを更新する(描画用)
		for(auto& bullet : bullets_){
			if(bullet.obj){
				bullet.obj->SetTranslate(bullet.position);
				bullet.obj->SetScale({kBulletScale_, kBulletScale_, kBulletScale_});
				if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
					bullet.obj->SetCamera(activeCamera);
				}
				bullet.obj->Update();
			}
		}

		// 狙えているときはレティクル中心を赤く、それ以外は白のままにする
		if(reticleCenterSprite_){
			reticleCenterSprite_->SetColor(isAimingAtAnyTarget?Vector4{1.0f, 0.2f, 0.2f, 1.0f}:Vector4{1.0f, 1.0f, 1.0f, 1.0f});
		}
	}

	// ImGuiが無い環境でもレールの制御点を確認できるよう、F1キーで俯瞰デバッグカメラを切り替える
	if(input_ && input_->TriggerKey(DIK_F1)){
		useDebugTopCamera_ = !useDebugTopCamera_;
		Camera* switchedCamera = CameraManager::GetInstance()->GetCamera(useDebugTopCamera_?"debug_top":"default");
		CameraManager::GetInstance()->SetActiveCamera(useDebugTopCamera_?"debug_top":"default");
		// SetCamera()を毎フレーム呼んでいないフェンスや地面等のオブジェクトは
		// object3dCommon_のdefaultCamera_を参照し続けるため、こちらも切り替えないと追従しない
		if(switchedCamera && object3dCommon_){
			object3dCommon_->SetDefaultCamera(switchedCamera);
		}

		// ONにした瞬間だけ、制御点全体を囲むように自動フィットさせる
		if(useDebugTopCamera_ && railEditor_){
			Vector3 center = railEditor_->GetControlPointsCenter();
			float radius = railEditor_->GetControlPointsRadius();

			constexpr float kMinHeight = 10.0f;
			constexpr float kMarginFactor = 2.2f; // 画角に対する余白の目安(仮値)
			float height = radius * kMarginFactor + kMinHeight;

			if(Camera* debugTopCamera = CameraManager::GetInstance()->GetCamera("debug_top")){
				debugTopCamera->SetTranslate({center.x, height, center.z});
				debugTopCamera->SetRotate({3.14159265f * 0.5f, 0.0f, 0.0f});
			}
		}
	}

	// ImGuiが無い環境でも表示切り替えができるよう、キー操作で各種デバッグ表示をトグルする
	if(input_ && input_->TriggerKey(DIK_F3)){
		showCameraDebugMarkers_ = !showCameraDebugMarkers_;
	}
	if(input_ && input_->TriggerKey(DIK_F4) && railEditor_){
		railEditor_->ToggleShowControlPointModels();
	}
	if(input_ && input_->TriggerKey(DIK_F5) && railEditor_){
		railEditor_->ToggleShowCurve();
	}

	// 俯瞰デバッグカメラが有効な間は、ImGui無しでもWASD(平面移動)+QE(高さ)で自由に動かせるようにする
	if(useDebugTopCamera_ && input_){
		if(Camera* debugTopCamera = CameraManager::GetInstance()->GetCamera("debug_top")){
			constexpr float kDebugCameraMoveSpeed = 10.0f; // 1秒あたりの移動量
			const float moveDelta = kDebugCameraMoveSpeed * (1.0f / 60.0f);

			Vector3 debugCameraPos = debugTopCamera->GetTranslate();
			if(input_->PushKey(DIK_W)) debugCameraPos.z += moveDelta;
			if(input_->PushKey(DIK_S)) debugCameraPos.z -= moveDelta;
			if(input_->PushKey(DIK_A)) debugCameraPos.x -= moveDelta;
			if(input_->PushKey(DIK_D)) debugCameraPos.x += moveDelta;
			if(input_->PushKey(DIK_E)) debugCameraPos.y += moveDelta;
			if(input_->PushKey(DIK_Q)) debugCameraPos.y -= moveDelta;
			debugTopCamera->SetTranslate(debugCameraPos);
		}
	}

#ifdef USE_IMGUI
	// Playモード中は編集用パネルをすべて隠す(クリーンな実行画面にするため)
	if(EditorContext::GetInstance()->IsPlayMode()){
		// 何も表示しない
	} else if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		EditorWidgets::Layout L = EditorWidgets::ComputeLayout();
		// デバッグ用のメインウィンドウ(下段・左)
		EditorWidgets::BeginFixedPanel("GameScene Debug",L.bottomLeft);

		// カメラ設定のUI
		if(ImGui::CollapsingHeader("Camera Settings")){
			Vector3 camPos = activeCamera->GetTranslate();
			if(ImGui::DragFloat3("Camera Pos",&camPos.x,0.1f)){
				activeCamera->SetTranslate(camPos);
			}

			Vector3 camRot = activeCamera->GetRotate();
			if(ImGui::DragFloat3("Camera Rotate",&camRot.x,0.01f)){
				activeCamera->SetRotate(camRot);
			}

			// 俯瞰デバッグカメラの切り替えボタン(ONにした瞬間だけ制御点全体にフィットさせる)
			if(ImGui::Checkbox("Debug Top-Down View",&useDebugTopCamera_)){
				CameraManager::GetInstance()->SetActiveCamera(useDebugTopCamera_?"debug_top":"default");

				// ONにしたときだけ、制御点全体を囲むように自動フィット。以降は手動で自由に調整できる
				if(useDebugTopCamera_ && railEditor_){
					Vector3 center = railEditor_->GetControlPointsCenter();
					float radius = railEditor_->GetControlPointsRadius();

					const float kMinHeight = 10.0f;
					const float kMarginFactor = 2.2f; // 画角に対する余白の目安
					float height = radius * kMarginFactor + kMinHeight;

					if(Camera* debugTopCamera = CameraManager::GetInstance()->GetCamera("debug_top")){
						debugTopCamera->SetTranslate({center.x, height, center.z});
						debugTopCamera->SetRotate({3.14159265f * 0.5f, 0.0f, 0.0f});
					}
				}
			}

			// メインカメラの位置・向きマーカーの表示ON/OFF切り替え
			ImGui::Checkbox("Show Main Camera Markers",&showCameraDebugMarkers_);
		}

		// ライティング設定のUI
		if(ImGui::CollapsingHeader("Lighting")){
			if(PointLight* pData = object3dCommon_->GetPointLightData()){
				ImGui::Text("Point Light");
				ImGui::ColorEdit4("Point Color",&pData->color.x);
				ImGui::DragFloat3("Point Pos",&pData->position.x,0.1f);
				ImGui::DragFloat("Point Intensity",&pData->intensity,0.1f,0.0f,100.0f);
			}
			if(SpotLight* sData = object3dCommon_->GetSpotLightData()){
				ImGui::Text("Spot Light");
				ImGui::ColorEdit4("Spot Color",&sData->color.x);
			}
		}

		ModelManager::GetInstance()->UpdateLightGui();
		ImGui::End();

		Application::GetInstance()->ShowPostProcessUI();

		// シーン階層(Hierarchy)の最小版
		// Unity/Unreal風の「一覧から選択 → Inspectorで編集」フロー
		// 左・上段に配置
		EditorWidgets::BeginFixedPanel("Hierarchy",L.hierarchy);
		ImGui::Text("Targets: %d",static_cast<int>(targets_.size()));
		ImGui::Separator();
		for(int i = 0; i < static_cast<int>(targets_.size()); ++i){
			ImGui::PushID(i);
			// 行ラベル(倒された的は (dead) を付ける)
			char label[32];
			snprintf(label,sizeof(label),"Target %d%s",i,targets_[i].isAlive?"":" (dead)");
			// クリックで選択。選択中の行はハイライトされる
			if(ImGui::Selectable(label,selectedTargetIndex_ == i)){
				selectedTargetIndex_ = i;
			}
			ImGui::PopID();
		}
		ImGui::End();

		// Inspectorの最小版
		// 右・上段に配置
		EditorWidgets::BeginFixedPanel("Inspector",L.inspector);
		if(selectedTargetIndex_ >= 0 && selectedTargetIndex_ < static_cast<int>(targets_.size())){
			Target& sel = targets_[selectedTargetIndex_];
			ImGui::Text("Target %d",selectedTargetIndex_);
			ImGui::Separator();
			// 位置を[-]/[+]ボタンで編集。毎フレーム obj->SetTranslate(position) しているので即座に反映される
			EditorWidgets::ButtonVector3("Position",sel.position,0.1f,1.0f);
			// 生存フラグ(OFFで非表示、ONで復活)
			ImGui::Checkbox("Alive",&sel.isAlive);
		} else{
			ImGui::TextDisabled("Select a target in Hierarchy");
		}
		ImGui::End();
	}
#endif
}

// シーンの描画処理
void GameScene::Draw(){
	object3dCommon_->Draw();

	// レールエディターの描画
	if(railEditor_){
		railEditor_->Draw();
	}

	// プレイヤー(人型モデル)を描画
	if(player_){
		player_->Draw();
	}

	// カメラ位置・向きのデバッグマーカーを描画(ON/OFF切り替え可能)
	// Playモード中はギズモとして隠す(実行画面には出さない)
	if(showCameraDebugMarkers_ && !EditorContext::GetInstance()->IsPlayMode()){
		if(cameraMarker_) cameraMarker_->Draw();
		if(cameraFacingMarker_) cameraFacingMarker_->Draw();
	}

	// 生存している的だけ描画
	for(auto& target : targets_){
		if(target.isAlive && target.obj){
			target.obj->Draw();
		}
	}

	// 発射中の弾を描画
	for(auto& bullet : bullets_){
		if(bullet.isAlive && bullet.obj){
			bullet.obj->Draw();
		}
	}

	// 変更箇所: 撃破演出の追加
	// 撃破演出パーティクルの描画(3Dオブジェクトの後、2Dレティクルの前に描画する)
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		ParticleManager::GetInstance()->Draw(activeCamera->GetViewProjectionMatrix());
	}

	// 画面中央固定のレティクルを描画(2D描画のため、SpriteCommonの描画前処理を先に呼ぶ)
	if((reticleOutlineSprite_ || reticleCenterSprite_) && spriteCommon_){
		spriteCommon_->Draw(); // 描画前処理
		if(reticleOutlineSprite_) reticleOutlineSprite_->Draw(); // 外枠を先に描画
		if(reticleCenterSprite_) reticleCenterSprite_->Draw();   // 中心ドットを上から重ねる
	}
}
