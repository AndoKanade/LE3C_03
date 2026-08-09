#include "RailEditor.h"
#include "ImGuiManager.h"
#include "imgui.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include "EditorWidgets.h"
#include "EditorContext.h"
#include "externals/json.hpp"
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iomanip>

namespace{
	// 追加 制御点データの保存先(LevelManagerと同じくresource配下の相対パス)
	const std::string kRailSaveFilePath = "resource/rail/rail.json";
	// 追加 制御点リセット用のデフォルト値(初期制御点/Add Control Pointボタンと同じ初期値)
	constexpr RailEditor::ControlPoint kDefaultControlPoint = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f};
}

RailEditor::RailEditor() = default;
RailEditor::~RailEditor() = default;

// 制御点描画用のObj3Dを1つ生成する(Initialize/追加ボタン/ロードで共通利用)
std::unique_ptr<Obj3D> RailEditor::CreatePointObject(){
	auto obj = std::make_unique<Obj3D>();
	obj->Initialize(objCommon_);
	obj->SetModel("Sphere/sphere.obj");
	return obj;
}

// pointObjects_の個数をcontrolPoints_の個数にそろえる
void RailEditor::SyncPointObjectsToControlPoints(){
	// 足りなければ生成、多ければ末尾を削る
	while(pointObjects_.size() < controlPoints_.size()){
		pointObjects_.push_back(CreatePointObject());
	}
	while(pointObjects_.size() > controlPoints_.size()){
		pointObjects_.pop_back();
	}
}

// エディターの初期化処理
void RailEditor::Initialize(Obj3dCommon* objCommon){
	objCommon_ = objCommon;

	// 初期制御点を追加
	controlPoints_.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f});

	// 制御点描画用のモデルをロード
	ModelManager::GetInstance()->LoadModel("Sphere/sphere.obj");

	// 最初の制御点用の3Dオブジェクトを生成
	pointObjects_.push_back(CreatePointObject());

	// 追加 レール曲線可視化用のサンプリング点オブジェクトを固定数だけ生成しておく
	curveObjects_.reserve(kCurveSampleCount);
	for(int i = 0; i < kCurveSampleCount; ++i){
		curveObjects_.push_back(CreatePointObject());
	}

	// 追加 保存済みのJSONがあれば読み込んで初期状態を上書きする(ホットロードの起動時オート読込)
	LoadFromJson();
}

// エディターの更新処理
void RailEditor::Update(){
#ifdef USE_IMGUI
	// Playモード中はレール編集UIを出さない
	if(EditorContext::GetInstance()->IsPlayMode()){ return; }

	// 固定タイルレイアウトの左・下段に配置
	EditorWidgets::BeginFixedPanel("Rail Editor",EditorWidgets::ComputeLayout().railEditor);

	// 制御点の追加ボタン
	if(ImGui::Button("Add Control Point")){
		// リストの最後に新しい制御点を追加
		controlPoints_.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f});

		// 追加した制御点用の3Dオブジェクトを生成
		pointObjects_.push_back(CreatePointObject());
	}

	ImGui::Separator();

	// 追加 JSONへの保存/読み込みボタン(ImGuiで編集した値をそのまま反映させる)
	if(ImGui::Button("Save")){
		SaveToJson();
	}
	ImGui::SameLine();
	if(ImGui::Button("Load")){
		LoadFromJson();
	}
	ImGui::SameLine();
	ImGui::Text("(%s)",kRailSaveFilePath.c_str());

	ImGui::Separator();

	// 追加 制御点モデルの表示ON/OFF切り替え(他のオブジェクトが見づらくなるとき用)
	ImGui::Checkbox("Show Control Point Models",&showControlPointModels_);
	// 追加 レール曲線(サンプリング点列)の表示ON/OFF切り替え
	ImGui::Checkbox("Show Rail Curve",&showCurve_);

	ImGui::Separator();

	// 追加 削除ボタンが押された制御点のインデックス(未押下なら-1のまま)
	int deleteIndex = -1;

	// 各制御点の編集UI
	for(size_t i = 0; i < controlPoints_.size(); ++i){
		// IDの重複を防ぐためPushIDを使用
		ImGui::PushID(static_cast<int>(i));
		ImGui::Text("Point %zu",i);

		// 座標、回転、速度の編集UI
		ImGui::DragFloat3("Position",&controlPoints_[i].position.x,0.1f);
		ImGui::DragFloat3("Rotation",&controlPoints_[i].rotation.x,0.1f);
		ImGui::DragFloat("Speed",&controlPoints_[i].speed,0.1f);

		// 追加 制御点のパラメータをデフォルト値に戻すボタン
		if(ImGui::Button("Reset")){
			controlPoints_[i] = kDefaultControlPoint;
		}
		ImGui::SameLine();

		// 追加 制御点の削除ボタン(最低1点は残す必要があるため、1点しかないときは押せないようにする)
		bool isOnlyPoint = (controlPoints_.size() <= 1);
		ImGui::BeginDisabled(isOnlyPoint);
		if(ImGui::Button("Delete")){
			deleteIndex = static_cast<int>(i);
		}
		ImGui::EndDisabled();

		ImGui::Separator();
		ImGui::PopID();
	}

	// 追加 ループ中の削除はイテレータを壊すため、ループを抜けてから実行する
	if(deleteIndex >= 0){
		controlPoints_.erase(controlPoints_.begin() + deleteIndex);
		// pointObjects_の個数もcontrolPoints_に合わせる(以降Draw()でインデックスがずれないようにする)
		SyncPointObjectsToControlPoints();
	}

	ImGui::End();
#endif
}

// デバッグ用の描画処理
void RailEditor::Draw(){
	// Playモード中は制御点の球やレール曲線(編集用ギズモ)を隠す
	if(EditorContext::GetInstance()->IsPlayMode()){ return; }

	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();

	// 追加 表示OFFのときは制御点のモデルを描画しない
	if(showControlPointModels_){
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

	// 追加 レール曲線をサンプリング点列(小さい球)で可視化する簡易版
	// Catmull-Romは制御点4つ以上必要なので、それ未満のときは何も描画しない
	if(showCurve_ && controlPoints_.size() >= 4){
		for(int i = 0; i < kCurveSampleCount; ++i){
			float t = static_cast<float>(i) / static_cast<float>(kCurveSampleCount - 1);
			Vector3 pos = GetPositionOnRail(t);

			curveObjects_[i]->SetTranslate(pos);
			// 制御点よりさらに小さくして、線のように見せる
			curveObjects_[i]->SetScale({0.04f, 0.04f, 0.04f});

			if(activeCamera){
				curveObjects_[i]->SetCamera(activeCamera);
			}

			curveObjects_[i]->Update();
			curveObjects_[i]->Draw();
		}
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

// 現在の制御点をJSONファイルに保存する
void RailEditor::SaveToJson(){
	// ルート要素。nameは読み込み時の簡易フォーマットチェック用
	nlohmann::json root;
	root["name"] = "rail";

	// 各制御点を配列に詰める
	nlohmann::json pointsJson = nlohmann::json::array();
	for(const auto& cp : controlPoints_){
		nlohmann::json pj;
		pj["position"] = {cp.position.x, cp.position.y, cp.position.z};
		pj["rotation"] = {cp.rotation.x, cp.rotation.y, cp.rotation.z};
		pj["speed"] = cp.speed;
		pointsJson.push_back(pj);
	}
	root["control_points"] = pointsJson;

	// 保存先フォルダが無ければ作成しておく
	std::filesystem::path savePath(kRailSaveFilePath);
	if(savePath.has_parent_path()){
		std::filesystem::create_directories(savePath.parent_path());
	}

	// ファイルへ書き出し(setwで人が読める整形出力にする)
	std::ofstream file(kRailSaveFilePath);
	if(!file.is_open()){
		return;
	}
	file << std::setw(4) << root << std::endl;
}

// JSONファイルから制御点を読み込む(ファイルが無ければ何もしない)
void RailEditor::LoadFromJson(){
	std::ifstream file(kRailSaveFilePath);
	if(!file.is_open()){
		// ファイルがまだ無い(初回起動など)ときは初期状態のまま
		return;
	}

	nlohmann::json root;
	file >> root;

	// 最低限のフォーマットチェック。想定外なら読み込みを中止して現状維持
	if(!root.is_object() || !root.contains("control_points") || !root["control_points"].is_array()){
		return;
	}

	// 読み込んだ内容で制御点を置き換える
	controlPoints_.clear();
	for(const auto& pj : root["control_points"]){
		ControlPoint cp{};
		cp.position.x = pj["position"][0].get<float>();
		cp.position.y = pj["position"][1].get<float>();
		cp.position.z = pj["position"][2].get<float>();
		cp.rotation.x = pj["rotation"][0].get<float>();
		cp.rotation.y = pj["rotation"][1].get<float>();
		cp.rotation.z = pj["rotation"][2].get<float>();
		cp.speed = pj["speed"].get<float>();
		controlPoints_.push_back(cp);
	}

	// 制御点が0個だと他の処理(補間など)が破綻するので最低1点は保証する
	if(controlPoints_.empty()){
		controlPoints_.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f});
	}

	// 描画用オブジェクトの数を制御点数に合わせる
	SyncPointObjectsToControlPoints();
}