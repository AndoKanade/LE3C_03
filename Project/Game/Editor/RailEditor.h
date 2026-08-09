#pragma once
#include "MyMath.h"
#include <vector>
#include <memory>
#include <string>

// クラスの前方宣言
class Obj3D;
class Obj3dCommon;

class RailEditor{
public:
	// レールの制御点を表す構造体
	struct ControlPoint{
		Vector3 position; // 制御点の3D座標
		Vector3 rotation; // 通過時のカメラの角度
		float speed;      // 次の点への移動速度
	};

	RailEditor();
	~RailEditor();

	// 初期化処理
	void Initialize(Obj3dCommon* objCommon);
	// 更新処理（ImGuiでの編集など）
	void Update();
	// デバッグ用の描画処理
	void Draw();

	// レール上の座標を取得 (t: 0〜1)
	Vector3 GetPositionOnRail(float t) const;
	// レール上の回転を取得 (t: 0〜1)
	Vector3 GetRotationOnRail(float t) const;
	// レール上の正規化された進行方向を取得 (t: 0〜1)
	Vector3 GetForwardOnRail(float t) const;
	// レール上の速度(各制御点のSpeed値を補間したもの)を取得 (t: 0〜1)
	float GetSpeedOnRail(float t) const;

	// ImGui非表示時でもキー操作で表示切り替えを行うための関数
	void ToggleShowControlPointModels(){ showControlPointModels_ = !showControlPointModels_; }
	void ToggleShowCurve(){ showCurve_ = !showCurve_; }

	// 全制御点の中心座標を取得
	Vector3 GetControlPointsCenter() const;
	// 全制御点を囲む範囲の半径を取得
	float GetControlPointsRadius() const;

	// 現在の制御点をJSONファイルに保存する
	void SaveToJson();
	// JSONファイルから制御点を読み込む(ファイルが無い場合はスキップ)
	void LoadFromJson();

private:
	// 制御点描画用の3Dオブジェクト生成
	std::unique_ptr<Obj3D> CreatePointObject();
	// pointObjects_の個数をcontrolPoints_に同期
	void SyncPointObjectsToControlPoints();

	std::vector<ControlPoint> controlPoints_;          // 制御点を保存する配列
	std::vector<std::unique_ptr<Obj3D>> pointObjects_; // 制御点描画用の3Dオブジェクト群
	Obj3dCommon* objCommon_ = nullptr;                 // 3Dオブジェクト共通設定へのポインタ

	bool showControlPointModels_ = true;               // 制御点の球体モデルの描画フラグ

	// レール曲線の可視化用(一定間隔でサンプリングして球を並べる)
	std::vector<std::unique_ptr<Obj3D>> curveObjects_; // サンプリング点描画用のオブジェクト群(固定数)
	bool showCurve_ = true;                            // 曲線表示フラグ
	static constexpr int kCurveSampleCount = 100;      // サンプリング分割数
};