#pragma once
#include "MyMath.h"
#include <vector>
#include <memory>

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

	// 全制御点の中心座標を取得
	Vector3 GetControlPointsCenter() const;
	// 全制御点を囲む範囲の半径を取得
	float GetControlPointsRadius() const;

private:
	std::vector<ControlPoint> controlPoints_;         // 制御点を保存する配列
	std::vector<std::unique_ptr<Obj3D>> pointObjects_; // 制御点描画用の3Dオブジェクト群
	Obj3dCommon* objCommon_ = nullptr;                // 3Dオブジェクト共通設定へのポインタ
};