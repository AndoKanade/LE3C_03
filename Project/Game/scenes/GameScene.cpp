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

	// メインカメラの生成と設定
	CameraManager::GetInstance()->CreateCamera("default",object3dCommon_->GetDxCommon()->GetDevice());
	auto* defaultCamera = CameraManager::GetInstance()->GetCamera("default");
	defaultCamera->SetTranslate({0.0f, 0.0f, -30.0f});
	CameraManager::GetInstance()->SetActiveCamera("default");
	object3dCommon_->SetDefaultCamera(CameraManager::GetInstance()->GetActiveCamera());

	// テクスチャとスカイボックスの読み込み・初期化
	TextureManager::GetInstance()->LoadTexture(kSkyboxTexture);
	skyboxCommon_ = std::make_unique<SkyboxCommon>();
	skyboxCommon_->Initialize(object3dCommon_->GetDxCommon());
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(skyboxCommon_.get(),kSkyboxTexture);

	// レールエディターの生成と初期化
	railEditor_ = std::make_unique<RailEditor>();
	railEditor_->Initialize(object3dCommon_);

	// 俯瞰用デバッグカメラの生成 (真上から見下ろす位置に配置)
	CameraManager::GetInstance()->CreateCamera("debug_top",object3dCommon_->GetDxCommon()->GetDevice());
	auto* debugTopCamera = CameraManager::GetInstance()->GetCamera("debug_top");
	debugTopCamera->SetTranslate({0.0f, 30.0f, 0.0f});
	debugTopCamera->SetRotate({3.14159265f * 0.5f, 0.0f, 0.0f});

	// カメラ位置を可視化するマーカーモデルの生成
	ModelManager::GetInstance()->LoadModel("Sphere/sphere.obj");

	cameraMarker_ = std::make_unique<Obj3D>();
	cameraMarker_->Initialize(object3dCommon_);
	cameraMarker_->SetModel("Sphere/sphere.obj");

	// カメラの向きを可視化するマーカーの生成
	cameraFacingMarker_ = std::make_unique<Obj3D>();
	cameraFacingMarker_->Initialize(object3dCommon_);
	cameraFacingMarker_->SetModel("Sphere/sphere.obj");
}

// シーンの終了処理
void GameScene::Finalize(){}

// シーンの更新処理
void GameScene::Update(){
	// スカイボックスの更新
	if(skybox_){
		skybox_->Update(*CameraManager::GetInstance()->GetActiveCamera());
	}

	// レールエディターの更新
	if(railEditor_){
		railEditor_->Update();
	}

	// レール移動処理
	if(railEditor_){
		// フレームレート固定(60fps)での時間進行
		const float deltaTime = 1.0f / 60.0f;
		railT_ += railSpeed_ * deltaTime;
		if(railT_ > 1.0f){
			railT_ -= 1.0f; // ループ処理
		}

		Vector3 railPos = railEditor_->GetPositionOnRail(railT_);
		Vector3 railRot = railEditor_->GetRotationOnRail(railT_);

		// プレイヤー入力による照準(カメラの向き)のオフセット処理
		if(input_){
			if(input_->PushKey(DIK_LEFT))  aimYawOffset_ -= kAimSpeed_ * deltaTime;
			if(input_->PushKey(DIK_RIGHT)) aimYawOffset_ += kAimSpeed_ * deltaTime;
			if(input_->PushKey(DIK_UP))    aimPitchOffset_ -= kAimSpeed_ * deltaTime;
			if(input_->PushKey(DIK_DOWN))  aimPitchOffset_ += kAimSpeed_ * deltaTime;

			// 可動範囲のクランプ処理
			if(aimYawOffset_ > kAimYawLimit_) aimYawOffset_ = kAimYawLimit_;
			if(aimYawOffset_ < -kAimYawLimit_) aimYawOffset_ = -kAimYawLimit_;
			if(aimPitchOffset_ > kAimPitchLimit_) aimPitchOffset_ = kAimPitchLimit_;
			if(aimPitchOffset_ < -kAimPitchLimit_) aimPitchOffset_ = -kAimPitchLimit_;
		}

		// レールの向きと照準オフセットを合算
		Vector3 finalRot = {railRot.x + aimPitchOffset_, railRot.y + aimYawOffset_, railRot.z};

		// メインカメラへ座標と回転を適用
		if(Camera* mainCamera = CameraManager::GetInstance()->GetCamera("default")){
			mainCamera->SetTranslate(railPos);
			mainCamera->SetRotate(finalRot);
		}

		// デバッグマーカーの位置同期
		if(cameraMarker_){
			cameraMarker_->SetTranslate(railPos);
			cameraMarker_->SetScale({0.7f, 0.7f, 0.7f});
			if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
				cameraMarker_->SetCamera(activeCamera);
			}
			cameraMarker_->Update();
		}

		// カメラの向きマーカー（鼻）の位置計算と同期
		if(cameraFacingMarker_){
			// オイラー角から回転行列を生成し前方ベクトルを計算
			Matrix4x4 rotateX = MakeRotateXMatrix(finalRot.x);
			Matrix4x4 rotateY = MakeRotateYMatrix(finalRot.y);
			Matrix4x4 rotateZ = MakeRotateZMatrix(finalRot.z);
			Matrix4x4 rotateMatrix = Multiply(Multiply(rotateX,rotateY),rotateZ);

			Vector3 forward = {0.0f, 0.0f, 1.0f};
			Vector3 forwardRotated = {
				forward.x * rotateMatrix.m[0][0] + forward.y * rotateMatrix.m[1][0] + forward.z * rotateMatrix.m[2][0],
				forward.x * rotateMatrix.m[0][1] + forward.y * rotateMatrix.m[1][1] + forward.z * rotateMatrix.m[2][1],
				forward.x * rotateMatrix.m[0][2] + forward.y * rotateMatrix.m[1][2] + forward.z * rotateMatrix.m[2][2]
			};

			// カメラ中心から前方にオフセット配置
			const float kNoseOffset = 2.0f;
			Vector3 nosePos = railPos + forwardRotated * kNoseOffset;

			cameraFacingMarker_->SetTranslate(nosePos);
			cameraFacingMarker_->SetScale({0.3f, 0.3f, 0.3f}); // 本体より縮小
			if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
				cameraFacingMarker_->SetCamera(activeCamera);
			}
			cameraFacingMarker_->Update();
		}
	}

#ifdef USE_IMGUI
	// UI描画処理
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		ImGui::Begin("GameScene Debug");

		// カメラ設定UI
		if(ImGui::CollapsingHeader("Camera Settings")){
			Vector3 camPos = activeCamera->GetTranslate();
			if(ImGui::DragFloat3("Camera Pos",&camPos.x,0.1f)) activeCamera->SetTranslate(camPos);

			Vector3 camRot = activeCamera->GetRotate();
			if(ImGui::DragFloat3("Camera Rotate",&camRot.x,0.01f)) activeCamera->SetRotate(camRot);

			// 俯瞰デバッグカメラの切り替えと自動フィット
			if(ImGui::Checkbox("Debug Top-Down View",&useDebugTopCamera_)){
				CameraManager::GetInstance()->SetActiveCamera(useDebugTopCamera_?"debug_top":"default");

				// ONにした瞬間のみ、制御点全体を囲むように自動フィット
				if(useDebugTopCamera_ && railEditor_){
					Vector3 center = railEditor_->GetControlPointsCenter();
					float radius = railEditor_->GetControlPointsRadius();

					const float kMinHeight = 10.0f;
					const float kMarginFactor = 2.2f;
					float height = radius * kMarginFactor + kMinHeight;

					if(Camera* debugTopCamera = CameraManager::GetInstance()->GetCamera("debug_top")){
						debugTopCamera->SetTranslate({center.x, height, center.z});
						debugTopCamera->SetRotate({3.14159265f * 0.5f, 0.0f, 0.0f});
					}
				}
			}
		}

		// ライティング設定UI
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

	// デバッグ用オブジェクト群の描画
	if(railEditor_){
		railEditor_->Draw();
	}
	if(cameraMarker_){
		cameraMarker_->Draw();
	}
	if(cameraFacingMarker_){
		cameraFacingMarker_->Draw();
	}
}