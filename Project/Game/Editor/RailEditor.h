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

	// レール上の座標を取得 (t: 0〜1)。対象はアクティブなレール
	Vector3 GetPositionOnRail(float t) const;
	// レール上の回転を取得 (t: 0〜1)。対象はアクティブなレール
	Vector3 GetRotationOnRail(float t) const;
	// レール上の正規化された進行方向を取得 (t: 0〜1)。対象はアクティブなレール
	Vector3 GetForwardOnRail(float t) const;
	// レール上の速度(各制御点のSpeed値を補間したもの)を取得 (t: 0〜1)。対象はアクティブなレール
	float GetSpeedOnRail(float t) const;

	// ImGui非表示時でもキー操作で表示切り替えを行うための関数(対象はアクティブなレール)
	void ToggleShowControlPointModels();
	void ToggleShowCurve();

	// アクティブなレールの全制御点の中心座標を取得
	Vector3 GetControlPointsCenter() const;
	// アクティブなレールの全制御点を囲む範囲の半径を取得
	float GetControlPointsRadius() const;

	// アクティブなレールの制御点をJSONファイルに保存する
	void SaveToJson();
	// アクティブなレールの制御点をJSONファイルから読み込む(ファイルが無い場合はスキップ)
	void LoadFromJson();

private:
	// 1本分のレールが持つデータ一式(制御点+描画用オブジェクト+表示設定)
	struct Rail{
		std::string name;                                  // レール名(UI表示・保存ファイル名の目印用)
		std::vector<ControlPoint> controlPoints;            // 制御点を保存する配列
		std::vector<std::unique_ptr<Obj3D>> pointObjects;   // 制御点描画用の3Dオブジェクト群
		std::vector<std::unique_ptr<Obj3D>> curveObjects;   // レール曲線サンプリング点描画用のオブジェクト群(固定数)
		bool showControlPointModels = true;                 // 制御点の球体モデルの描画フラグ
		bool showCurve = true;                              // 曲線表示フラグ
	};

	// 制御点描画用の3Dオブジェクト生成
	std::unique_ptr<Obj3D> CreatePointObject();
	// rail.pointObjectsの個数をrail.controlPointsに同期
	void SyncPointObjectsToControlPoints(Rail& rail);
	// 新しいレールを1本追加し、アクティブなレールとして切り替える
	void AddRail();
	// 指定インデックスのレールをアクティブなレールとして切り替える
	void SwitchActiveRail(int index);
	// レールのインデックスから保存先ファイルパスを求める(0番目のみ既存の互換パスを使う)
	std::string GetRailFilePath(int index) const;

	std::vector<Rail> rails_;      // 全レールの配列
	int activeRailIndex_ = 0;      // 現在編集・使用中のレールのインデックス

	Obj3dCommon* objCommon_ = nullptr; // 3Dオブジェクト共通設定へのポインタ

	// レール曲線可視化のサンプリング分割数
	static constexpr int kCurveSampleCount = 100;
};
