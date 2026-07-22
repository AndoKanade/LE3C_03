#include "RailEditor.h"
#include "ImGuiManager.h"
#include "imgui.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include <cmath>

RailEditor::RailEditor() = default;
RailEditor::~RailEditor() = default;

// エディターの初期化処理
void RailEditor::Initialize(Obj3dCommon* objCommon){
	objCommon_ = objCommon;

	// 初期制御点を追加
	controlPoints_.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f});

	// 制御点描画用のモデルをロード
	ModelManager::GetInstance()->LoadModel("Sphere/sphere.obj");

	// 最初の制御点用の3Dオブジェクトを生成
	auto initialObj = std::make_unique<Obj3D>();
	initialObj->Initialize(objCommon_);
	initialObj->SetModel("Sphere/sphere.obj");
	pointObjects_.push_back(std::move(initialObj));
}

// エディターの更新処理
void RailEditor::Update(){
#ifdef USE_IMGUI
	// エディター用のImGuiウィンドウを作成
	ImGui::Begin("Rail Editor");

	// 制御点の追加ボタン
	if(ImGui::Button("Add Control Point")){
		// リストの最後に新しい制御点を追加
		controlPoints_.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f});

		// 追加した制御点用の3Dオブジェクトを生成
		auto newObj = std::make_unique<Obj3D>();
		newObj->Initialize(objCommon_);
		newObj->SetModel("Sphere/sphere.obj");
		pointObjects_.push_back(std::move(newObj));
	}

	ImGui::Separator();

	// 各制御点の編集UI
	for(size_t i = 0; i < controlPoints_.size(); ++i){
		// IDの重複を防ぐためPushIDを使用
		ImGui::PushID(static_cast<int>(i));
		ImGui::Text("Point %zu",i);

		// 座標、回転、速度の編集UI
		ImGui::DragFloat3("Position",&controlPoints_[i].position.x,0.1f);
		ImGui::DragFloat3("Rotation",&controlPoints_[i].rotation.x,0.1f);
		ImGui::DragFloat("Speed",&controlPoints_[i].speed,0.1f);

		ImGui::Separator();
		ImGui::PopID();
	}

	ImGui::End();
#endif
}

// デバッグ用の描画処理
void RailEditor::Draw(){
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();

	// 制御点ごとに専用の3Dオブジェクトで描画
	for(size_t i = 0; i < controlPoints_.size(); ++i){
		pointObjects_[i]->SetTranslate(controlPoints_[i].position);
		// 視認性向上のためスケールを縮小
		pointObjects_[i]->SetScale({0.5f, 0.5f, 0.5f});

		// アクティブカメラに毎フレーム同期
		if(activeCamera){
			pointObjects_[i]->SetCamera(activeCamera);
		}

		pointObjects_[i]->Update();
		pointObjects_[i]->Draw();
	}
}

// 進行度t(0〜1)からレール上の座標を取得
Vector3 RailEditor::GetPositionOnRail(float t) const{
	// Catmull-Rom補間には最低4点必要
	if(controlPoints_.size() < 4){
		return {0.0f, 0.0f, 0.0f};
	}

	// tを区間数でスケールし、現在の区間(segment)を算出
	size_t numSegments = controlPoints_.size() - 3;
	float scaledT = t * static_cast<float>(numSegments);
	size_t segment = static_cast<size_t>(scaledT);

	if(segment >= numSegments){
		segment = numSegments - 1;
	}

	// 区間内でのローカルな進行度(0〜1)
	float localT = scaledT - static_cast<float>(segment);

	const Vector3& p0 = controlPoints_[segment].position;
	const Vector3& p1 = controlPoints_[segment + 1].position;
	const Vector3& p2 = controlPoints_[segment + 2].position;
	const Vector3& p3 = controlPoints_[segment + 3].position;

	return CatmullRom(p0,p1,p2,p3,localT);
}

// 進行度t(0〜1)からレール上の回転(オイラー角)を取得
Vector3 RailEditor::GetRotationOnRail(float t) const{
	Vector3 forward = GetForwardOnRail(t);

	// Y成分をasinの定義域[-1, 1]にクランプ
	float clampedY = forward.y;
	if(clampedY > 1.0f) clampedY = 1.0f;
	if(clampedY < -1.0f) clampedY = -1.0f;

	// 進行方向からピッチ(X軸回転)とヨー(Y軸回転)を逆算
	float pitch = -std::asin(clampedY);
	float yaw = std::atan2(forward.x,forward.z);

	// ロール(Z軸回転)は制御点間の値を補間して使用(バンク演出用)
	float roll = 0.0f;
	if(controlPoints_.size() >= 2){
		size_t numSegments = controlPoints_.size() - 1;
		float scaledT = t * static_cast<float>(numSegments);
		size_t segment = static_cast<size_t>(scaledT);

		if(segment >= numSegments){
			segment = numSegments - 1;
		}

		float localT = scaledT - static_cast<float>(segment);
		roll = Lerp(controlPoints_[segment].rotation.z,controlPoints_[segment + 1].rotation.z,localT);
	}

	return {pitch, yaw, roll};
}

// 進行度t(0〜1)における進行方向(接線ベクトル)を取得
Vector3 RailEditor::GetForwardOnRail(float t) const{
	const float kEpsilon = 0.001f;
	float t0 = t - kEpsilon;
	float t1 = t + kEpsilon;

	if(t0 < 0.0f) t0 = 0.0f;
	if(t1 > 1.0f) t1 = 1.0f;

	Vector3 p0 = GetPositionOnRail(t0);
	Vector3 p1 = GetPositionOnRail(t1);
	Vector3 diff = p1 - p0;

	float length = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

	// 差分がほぼ0の場合は正面向きを返す
	if(length < 1e-5f){
		return {0.0f, 0.0f, 1.0f};
	}

	return {diff.x / length, diff.y / length, diff.z / length};
}

// 全制御点を囲む範囲の中心座標を取得
Vector3 RailEditor::GetControlPointsCenter() const{
	if(controlPoints_.empty()){
		return {0.0f, 0.0f, 0.0f};
	}

	Vector3 minPos = controlPoints_[0].position;
	Vector3 maxPos = controlPoints_[0].position;

	for(const auto& point : controlPoints_){
		minPos.x = (point.position.x < minPos.x)?point.position.x:minPos.x;
		minPos.y = (point.position.y < minPos.y)?point.position.y:minPos.y;
		minPos.z = (point.position.z < minPos.z)?point.position.z:minPos.z;

		maxPos.x = (point.position.x > maxPos.x)?point.position.x:maxPos.x;
		maxPos.y = (point.position.y > maxPos.y)?point.position.y:maxPos.y;
		maxPos.z = (point.position.z > maxPos.z)?point.position.z:maxPos.z;
	}

	return {
		(minPos.x + maxPos.x) * 0.5f,
		(minPos.y + maxPos.y) * 0.5f,
		(minPos.z + maxPos.z) * 0.5f
	};
}

// 全制御点を囲む範囲のおおよその半径を取得
float RailEditor::GetControlPointsRadius() const{
	if(controlPoints_.empty()){
		return 0.0f;
	}

	Vector3 center = GetControlPointsCenter();
	float maxDistSq = 0.0f;

	for(const auto& point : controlPoints_){
		float dx = point.position.x - center.x;
		float dz = point.position.z - center.z;
		float distSq = dx * dx + dz * dz; // XZ平面での距離の2乗

		if(distSq > maxDistSq){
			maxDistSq = distSq;
		}
	}

	return std::sqrt(maxDistSq);
}