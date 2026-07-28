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
#include <cstdio>

// 変更 レールエディターをインクルードしました
#include "Editor/RailEditor.h"

namespace{
	// スカイボックスのテクスチャパス
	const std::string kSkyboxTexture = "resource/Skybox/rostock_laage_airport_4k.dds";
	// 追加 GlobalVariablesのグループ名(GameSceneの調整項目)
	const char* kGameSceneGroup = "GameScene";
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

	// 変更 レールエディターの生成と初期化を追加しました
	railEditor_ = std::make_unique<RailEditor>();
	railEditor_->Initialize(object3dCommon_);

	// 追加 調整項目(GlobalVariables)にゲームプレイ用パラメータを登録
	// ImGuiの "Global Variables" ウィンドウから実行中に編集・保存でき、
	// resource/GlobalVariables/GameScene.json を外部で書き換えると自動反映される(ホットリロード)
	{
		GlobalVariables* gv = GlobalVariables::GetInstance();
		gv->CreateGroup(kGameSceneGroup);
		// 第3引数はデフォルト値。保存済みJSONがあればそちらが優先される(AddItemは未登録キーのみ追加)
		gv->AddItem(kGameSceneGroup,"railSpeed",railSpeed_);
		gv->AddItem(kGameSceneGroup,"cameraHeightOffset",kCameraHeightOffset_);
		// 照準(エイム)まわりの手触り調整用パラメータ
		gv->AddItem(kGameSceneGroup,"aimSpeed",kAimSpeed_);           // 1秒あたりの回転量(ラジアン)
		gv->AddItem(kGameSceneGroup,"aimYawLimit",kAimYawLimit_);     // 左右の可動範囲
		gv->AddItem(kGameSceneGroup,"aimPitchLimit",kAimPitchLimit_); // 上下の可動範囲
		gv->AddItem(kGameSceneGroup,"aimHitAngle",kAimHitAngle_);     // ヒット判定の許容角度
		gv->AddItem(kGameSceneGroup,"noseOffset",2.0f);              // 向きマーカーを前方に離す距離
	}

	// 追加 俯瞰用のデバッグカメラを生成
	CameraManager::GetInstance()->CreateCamera("debug_top",object3dCommon_->GetDxCommon()->GetDevice());
	auto* debugTopCamera = CameraManager::GetInstance()->GetCamera("debug_top");
	// 真上から見下ろす位置に配置(X回転90度=pi/2で真下向き)
	debugTopCamera->SetTranslate({0.0f, 30.0f, 0.0f});
	debugTopCamera->SetRotate({3.14159265f * 0.5f, 0.0f, 0.0f});

	// 追加 カメラ位置を可視化するマーカーを生成
	ModelManager::GetInstance()->LoadModel("Sphere/sphere.obj");
	cameraMarker_ = std::make_unique<Obj3D>();
	cameraMarker_->Initialize(object3dCommon_);
	cameraMarker_->SetModel("Sphere/sphere.obj");


	// 追加 カメラの向きを可視化する小さいマーカーを生成
	cameraFacingMarker_ = std::make_unique<Obj3D>();
	cameraFacingMarker_->Initialize(object3dCommon_);
	cameraFacingMarker_->SetModel("Sphere/sphere.obj");

	// 追加 テスト用の的をレール沿いに仮配置(あとでレベルデータ化する想定)
	{
		std::vector<Vector3> testPositions = {
			{2.0f, 0.0f, 8.0f},
			{-2.0f, 1.0f, 17.0f},
			{0.0f, -1.0f, 24.0f},
		};
		for(const auto& pos : testPositions){
			Target target;
			target.obj = std::make_unique<Obj3D>();
			target.obj->Initialize(object3dCommon_);
			target.obj->SetModel("Sphere/sphere.obj");
			target.position = pos;
			target.isAlive = true;
			targets_.push_back(std::move(target));
		}
	}

	// 追加 画面中央固定のレティクルを生成(外枠+中心ドットの2枚構成)
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
}

// シーンの終了処理
void GameScene::Finalize(){}

// シーンの更新処理
void GameScene::Update(){
	// スカイボックスの更新
	if(skybox_) skybox_->Update(*CameraManager::GetInstance()->GetActiveCamera());

	// 追加 レティクル(画面中央固定なので位置は変わらないが、内部行列更新のため毎フレーム呼ぶ)
	if(reticleOutlineSprite_) reticleOutlineSprite_->Update();
	if(reticleCenterSprite_) reticleCenterSprite_->Update();

	// 変更 レールエディターの更新を追加しました
	if(railEditor_) railEditor_->Update();

	// 追加 レール進行度を時間で進めて、カメラをレール上に乗せる
	if(railEditor_){
		// 追加 調整項目から最新の値を取得(ImGui編集/ホットリロードが即反映される)
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
			aimYawOffset_ = 0.0f;
			aimPitchOffset_ = 0.0f;
			for(auto& t : targets_){ t.isAlive = true; }
		}
		wasPlayMode_ = isPlayMode;

		// レール進行はPlayモードのときだけ進める(Edit中はその位置で静止)
		if(isPlayMode){
			railT_ += railSpeed_ * deltaTime;
			if(railT_ > 1.0f) railT_ -= 1.0f; // ひとまずループさせる(後で終端で止める形に変更予定)
		}

		Vector3 railPos = railEditor_->GetPositionOnRail(railT_);
		Vector3 railRot = railEditor_->GetRotationOnRail(railT_);

		// 追加 三人称視点用に、カメラの実位置はレールそのものではなく少し上に置く
		// (レール上に直接カメラがあると、キャラクター視点っぽくなり三人称の見た目として不自然なため)
		Vector3 cameraPos = railPos + Vector3{0.0f, cameraHeightOffset, 0.0f};

		// 追加 プレイヤー入力で照準(カメラの向き)をレールの向きに上乗せする
		// 現状エンジンにキーボード入力しかないため、矢印キーで操作(将来的にコントローラー対応時はここを差し替え)
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

		// 追加 カメラマーカーもレール上の位置に追従させる
		if(cameraMarker_){
			cameraMarker_->SetTranslate(cameraPos);
			cameraMarker_->SetScale({0.7f, 0.7f, 0.7f});
			// 追加 アクティブカメラが切り替わっても正しく描画されるよう毎フレーム同期
			if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()) cameraMarker_->SetCamera(activeCamera);
			cameraMarker_->Update();
		}

		// 追加 カメラの前方ベクトルを計算(finalRotベースの回転行列を適用)
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

		// 追加 カメラの向きを表す「鼻」マーカーを、カメラ前方ベクトルの方向に置く
		if(cameraFacingMarker_){
			Vector3 nosePos = cameraPos + cameraForward * noseOffset;

			cameraFacingMarker_->SetTranslate(nosePos);
			cameraFacingMarker_->SetScale({0.3f, 0.3f, 0.3f}); // 本体より小さくして区別
			if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()) cameraFacingMarker_->SetCamera(activeCamera);
			cameraFacingMarker_->Update();
		}

		// 追加 的の当たり判定と射撃処理(画面中央固定のレティクル方式)
		// レティクルは常に画面中央=カメラの前方ベクトル方向なので、
		// 「カメラ→的」の方向とカメラ前方ベクトルのなす角が閾値以内なら狙えている
		bool shootPressed = isPlayMode && input_ && input_->PushKey(DIK_SPACE);
		bool isAimingAtAnyTarget = false; // 追加 レティクル中心の色変えに使う
		for(auto& target : targets_){
			if(!target.isAlive) continue;

			Vector3 toTarget = target.position - cameraPos;
			float distance = Length(toTarget);
			if(distance < 0.001f) continue; // カメラと的が重なっている異常値は無視

			float angle = AngleBetween(cameraForward,toTarget);
			bool isAimed = angle <= aimHitAngle;
			if(isAimed) isAimingAtAnyTarget = true;

			if(shootPressed && isAimed){
				target.isAlive = false;
			}

			if(target.obj){
				target.obj->SetTranslate(target.position);
				// 狙えているときは少し大きくして視覚的にフィードバック(仮の演出)
				float scale = isAimed?0.6f:0.4f;
				target.obj->SetScale({scale, scale, scale});
				if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()) target.obj->SetCamera(activeCamera);
				target.obj->Update();
			}
		}

		// 追加 狙えているときはレティクル中心を赤く、それ以外は白のままにする
		if(reticleCenterSprite_){
			reticleCenterSprite_->SetColor(isAimingAtAnyTarget?Vector4{1.0f, 0.2f, 0.2f, 1.0f}:Vector4{1.0f, 1.0f, 1.0f, 1.0f});
		}
	}

	// 追加 俯瞰デバッグカメラON中は、制御点全体を囲むように自動でフィットさせる
	// (チェックボックスON時の一度きりの処理に変更。GameScene::Updateの下部ImGuiブロックへ移動済み)

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
			if(ImGui::DragFloat3("Camera Pos",&camPos.x,0.1f)) activeCamera->SetTranslate(camPos);

			Vector3 camRot = activeCamera->GetRotate();
			if(ImGui::DragFloat3("Camera Rotate",&camRot.x,0.01f)) activeCamera->SetRotate(camRot);

			// 変更 俯瞰デバッグカメラの切り替えボタン(ONにした瞬間だけ制御点全体にフィットさせる)
			if(ImGui::Checkbox("Debug Top-Down View",&useDebugTopCamera_)){
				CameraManager::GetInstance()->SetActiveCamera(useDebugTopCamera_?"debug_top":"default");

				// ONにしたときだけ、制御点全体を囲むように自動フィット。以降は手動で自由に調整できる
				if(useDebugTopCamera_ && railEditor_){
					Vector3 center = railEditor_->GetControlPointsCenter();
					float radius = railEditor_->GetControlPointsRadius();

					const float kMinHeight = 10.0f;
					const float kMarginFactor = 2.2f; // 画角に対する余白の目安(仮値)
					float height = radius * kMarginFactor + kMinHeight;

					if(Camera* debugTopCamera = CameraManager::GetInstance()->GetCamera("debug_top")){
						debugTopCamera->SetTranslate({center.x, height, center.z});
						debugTopCamera->SetRotate({3.14159265f * 0.5f, 0.0f, 0.0f});
					}
				}
			}

			// 追加 メインカメラの位置・向きマーカーの表示ON/OFF切り替え
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

		// 追加 シーン階層(Hierarchy)+ Inspector の最小版
		// Unity/Unreal風の「一覧から選択 → Inspectorで編集」フロー。まずは的(targets)を対象にする
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

	// 変更 レールエディターの描画を追加しました
	if(railEditor_) railEditor_->Draw();

	// 追加 カメラ位置・向きのデバッグマーカーを描画(ON/OFF切り替え可能)
	// Playモード中はギズモとして隠す(実行画面には出さない)
	if(showCameraDebugMarkers_ && !EditorContext::GetInstance()->IsPlayMode()){
		if(cameraMarker_) cameraMarker_->Draw();
		if(cameraFacingMarker_) cameraFacingMarker_->Draw();
	}

	// 追加 生存している的だけ描画
	for(auto& target : targets_){
		if(target.isAlive && target.obj) target.obj->Draw();
	}

	// 追加 画面中央固定のレティクルを描画(2D描画のため、SpriteCommonの描画前処理を先に呼ぶ)
	if((reticleOutlineSprite_ || reticleCenterSprite_) && spriteCommon_){
		spriteCommon_->Draw(); // 描画前処理
		if(reticleOutlineSprite_) reticleOutlineSprite_->Draw(); // 外枠を先に描画
		if(reticleCenterSprite_) reticleCenterSprite_->Draw();   // 中心ドットを上から重ねる
	}
}