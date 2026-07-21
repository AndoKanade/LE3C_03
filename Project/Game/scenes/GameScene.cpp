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
}

// シーンの終了処理
void GameScene::Finalize(){}

// シーンの更新処理
void GameScene::Update(){
	// スカイボックスの更新
	if(skybox_) skybox_->Update(*CameraManager::GetInstance()->GetActiveCamera());

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

		if(Camera* mainCamera = CameraManager::GetInstance()->GetCamera("default")){
			mainCamera->SetTranslate(railPos);
			mainCamera->SetRotate(railRot);
		}

		// 追加 カメラマーカーもレール上の位置に追従させる
		if(cameraMarker_){
			cameraMarker_->SetTranslate(railPos);
			cameraMarker_->SetScale({0.7f, 0.7f, 0.7f});
			// 追加 アクティブカメラが切り替わっても正しく描画されるよう毎フレーム同期
			if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()) cameraMarker_->SetCamera(activeCamera);
			cameraMarker_->Update();
		}
	}

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

			// 追加 俯瞰デバッグカメラの切り替えボタン
			if(ImGui::Checkbox("Debug Top-Down View",&useDebugTopCamera_)){
				CameraManager::GetInstance()->SetActiveCamera(useDebugTopCamera_?"debug_top":"default");
			}
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
}