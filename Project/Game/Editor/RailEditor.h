#pragma once

#include "MyMath.h"
#include <vector>
#include <memory>

// クラスの前方宣言を追加
class Obj3D;
class Obj3dCommon;

class RailEditor{
public:
	// レールの制御点を表す構造体をクラス内に定義
	struct ControlPoint{
		// 制御点の3D座標
		Vector3 position;
		// 通過時のカメラの角度
		Vector3 rotation;
		// 次の点への移動速度
		float speed;
	};

	RailEditor();
	~RailEditor();

	// エディターの初期化処理 引数にObj3dCommonを追加
	void Initialize(Obj3dCommon* objCommon);
	// エディターの更新処理（ImGuiでの編集など）
	void Update();
	// デバッグ用の描画処理（今後点や線を書く用）
	void Draw();

	// 追加 レール全体の進行度t(0〜1)からレール上の座標を取得
	Vector3 GetPositionOnRail(float t) const;
	// 追加 レール全体の進行度t(0〜1)からレール上の回転(オイラー角)を取得
	Vector3 GetRotationOnRail(float t) const;

private:
	// 制御点を保存する配列
	std::vector<ControlPoint> controlPoints_;
	// 制御点描画用の3Dオブジェクトを追加
	std::unique_ptr<Obj3D> pointObject_;
};