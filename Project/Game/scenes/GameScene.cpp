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

// 変更 レールエディターをインクルードしました
#include "Editor/RailEditor.h"

namespace{
	// スカイボックスのテクスチャパス
	const std::string kSkyboxTexture = "resource/Skybox/rostock_laage_airport_4k.dds";
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

	//// 追加 画面中央固定のレティクルを生成(外枠+中心ドットの2枚構成)
	//TextureManager::GetInstance()->LoadTexture("reticle/reticleOutline.png");
	//TextureManager::GetInstance()->LoadTexture("reticle/reticle.png");

	//reticleOutlineSprite_ = std::make_unique<Sprite>();
	//reticleOutlineSprite_->Initialize(spriteCommon_,"reticle/reticleOutline.png");
	//reticleOutlineSprite_->SetSize({36.0f, 36.0f});
	//reticleOutlineSprite_->SetAnchorPoint({0.5f, 0.5f}); // 中心を基準点にする
	//reticleOutlineSprite_->SetPosition({
	//	static_cast<float>(WinAPI::kClientWidth) * 0.5f,
	//	static_cast<float>(WinAPI::kClientHeight) * 0.5f
	//	});

	//reticleCenterSprite_ = std::make_unique<Sprite>();
	//reticleCenterSprite_->Initialize(spriteCommon_,"reticle/reticle.png");
	//reticleCenterSprite_->SetSize({30.0f, 30.0f});
	//reticleCenterSprite_->SetAnchorPoint({0.5f, 0.5f});
	//reticleCenterSprite_->SetPosition({
	//	static_cast<float>(WinAPI::kClientWidth) * 0.5f,
	//	static_cast<float>(WinAPI::kClientHeight) * 0.5f
	//	});
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
		// エンジンが固定60fps前提(TimeManagerで60fps固定)なので、そのまま合わせる
		const float deltaTime = 1.0f / 60.0f;
		railT_ += railSpeed_ * deltaTime;
		if(railT_ > 1.0f) railT_ -= 1.0f; // ひとまずループさせる(後で終端で止める形に変更予定)

		Vector3 railPos = railEditor_->GetPositionOnRail(railT_);
		Vector3 railRot = railEditor_->GetRotationOnRail(railT_);

		// 追加 プレイヤー入力で照準(カメラの向き)をレールの向きに上乗せする
		// 現状エンジンにキーボード入力しかないため、矢印キーで操作(将来的にコントローラー対応時はここを差し替え)
		if(input_){
			if(input_->PushKey(DIK_LEFT))  aimYawOffset_ -= kAimSpeed_ * deltaTime;
			if(input_->PushKey(DIK_RIGHT)) aimYawOffset_ += kAimSpeed_ * deltaTime;
			if(input_->PushKey(DIK_UP))    aimPitchOffset_ -= kAimSpeed_ * deltaTime;
			if(input_->PushKey(DIK_DOWN))  aimPitchOffset_ += kAimSpeed_ * deltaTime;

			// 可動範囲でクランプ(振り向きすぎないように)
			if(aimYawOffset_ > kAimYawLimit_) aimYawOffset_ = kAimYawLimit_;
			if(aimYawOffset_ < -kAimYawLimit_) aimYawOffset_ = -kAimYawLimit_;
			if(aimPitchOffset_ > kAimPitchLimit_) aimPitchOffset_ = kAimPitchLimit_;
			if(aimPitchOffset_ < -kAimPitchLimit_) aimPitchOffset_ = -kAimPitchLimit_;
		}

		// レールの向き + 照準オフセットを最終的なカメラの向きとする
		Vector3 finalRot = {railRot.x + aimPitchOffset_, railRot.y + aimYawOffset_, railRot.z};

		if(Camera* mainCamera = CameraManager::GetInstance()->GetCamera("default")){
			mainCamera->SetTranslate(railPos);
			mainCamera->SetRotate(finalRot);
		}

		// 追加 カメラマーカーもレール上の位置に追従させる
		if(cameraMarker_){
			cameraMarker_->SetTranslate(railPos);
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
			const float kNoseOffset = 2.0f; // カメラ中心から前方にどれだけ離すか
			Vector3 nosePos = railPos + cameraForward * kNoseOffset;

			cameraFacingMarker_->SetTranslate(nosePos);
			cameraFacingMarker_->SetScale({0.3f, 0.3f, 0.3f}); // 本体より小さくして区別
			if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()) cameraFacingMarker_->SetCamera(activeCamera);
			cameraFacingMarker_->Update();
		}

		// 追加 的の当たり判定と射撃処理(画面中央固定のレティクル方式)
		// レティクルは常に画面中央=カメラの前方ベクトル方向なので、
		// 「カメラ→的」の方向とカメラ前方ベクトルのなす角が閾値以内なら狙えている
		bool shootPressed = input_ && input_->PushKey(DIK_SPACE);
		bool isAimingAtAnyTarget = false; // 追加 レティクル中心の色変えに使う
		for(auto& target : targets_){
			if(!target.isAlive) continue;

			Vector3 toTarget = target.position - railPos;
			float distance = Length(toTarget);
			if(distance < 0.001f) continue; // カメラと的が重なっている異常値は無視

			float angle = AngleBetween(cameraForward,toTarget);
			bool isAimed = angle <= kAimHitAngle_;
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
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		// デバッグ用のメインウィンドウ
		ImGui::Begin("GameScene Debug");

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
	}
#endif
}

// シーンの描画処理
void GameScene::Draw(){
	object3dCommon_->Draw();

	// 変更 レールエディターの描画を追加しました
	if(railEditor_) railEditor_->Draw();

	// 追加 カメラ位置・向きのデバッグマーカーを描画(ON/OFF切り替え可能)
	if(showCameraDebugMarkers_){
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