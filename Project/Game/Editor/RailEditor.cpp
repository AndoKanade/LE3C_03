#include "RailEditor.h"
#include "ImGuiManager.h"
#include "imgui.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"

RailEditor::RailEditor() = default;
RailEditor::~RailEditor() = default;

// エディターの初期化処理 引数を受け取るように変更
void RailEditor::Initialize(Obj3dCommon* objCommon){
	controlPoints_.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f});

	// 追加: モデルを事前にロード
	ModelManager::GetInstance()->LoadModel("Sphere/sphere.obj");

	pointObject_ = std::make_unique<Obj3D>();
	pointObject_->Initialize(objCommon);
	pointObject_->SetModel("Sphere/sphere.obj");
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
	}

	ImGui::Separator();

	// 各制御点の編集UI
	for(size_t i = 0; i < controlPoints_.size(); ++i){
		// ImGuiのIDが被らないようにPushIDを使用
		ImGui::PushID(static_cast<int>(i));
		ImGui::Text("Point %zu",i);

		// 座標、回転、速度を編集できるUI
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
	// 制御点の位置に球体を描画する処理を追加
	for(const auto& point : controlPoints_){
		pointObject_->SetTranslate(point.position);
		// エディター上で見やすいように少し小さくする
		pointObject_->SetScale({0.5f, 0.5f, 0.5f});
		pointObject_->Update();
		pointObject_->Draw();
	}
}

// 追加 レール全体の進行度t(0〜1)からレール上の座標を取得
Vector3 RailEditor::GetPositionOnRail(float t) const{
	// Catmull-Romは前後の点を含めて最低4点必要
	if(controlPoints_.size() < 4) return {0.0f, 0.0f, 0.0f};

	// tを区間数でスケールして、今どの区間(segment)にいるか求める
	size_t numSegments = controlPoints_.size() - 3;
	float scaledT = t * static_cast<float>(numSegments);
	size_t segment = static_cast<size_t>(scaledT);
	if(segment >= numSegments) segment = numSegments - 1;
	// 区間内でのローカルなt(0〜1)
	float localT = scaledT - static_cast<float>(segment);

	const Vector3& p0 = controlPoints_[segment].position;
	const Vector3& p1 = controlPoints_[segment + 1].position;
	const Vector3& p2 = controlPoints_[segment + 2].position;
	const Vector3& p3 = controlPoints_[segment + 3].position;

	return CatmullRom(p0,p1,p2,p3,localT);
}

// 追加 レール全体の進行度t(0〜1)からレール上の回転(オイラー角)を取得
// 暫定でLerpによる線形補間 のちにQuaternion+Slerpへの差し替えも可能
Vector3 RailEditor::GetRotationOnRail(float t) const{
	if(controlPoints_.size() < 2) return {0.0f, 0.0f, 0.0f};

	size_t numSegments = controlPoints_.size() - 1;
	float scaledT = t * static_cast<float>(numSegments);
	size_t segment = static_cast<size_t>(scaledT);
	if(segment >= numSegments) segment = numSegments - 1;
	float localT = scaledT - static_cast<float>(segment);

	const Vector3& r0 = controlPoints_[segment].rotation;
	const Vector3& r1 = controlPoints_[segment + 1].rotation;

	return Lerp(r0,r1,localT);
}