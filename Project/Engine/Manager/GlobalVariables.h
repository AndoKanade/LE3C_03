#pragma once
#include "MyMath.h"
#include <string>
#include <map>
#include <variant>
#include <filesystem>

/// <summary>
/// 汎用調整項目マネージャー(シングルトン)
///
/// 任意のクラスから「グループ名 + キー + 値」を登録しておくと、
///  ・ImGuiウィンドウが自動生成され、実行中に値を編集できる
///  ・グループ単位でJSONファイルに保存/読込できる(resource/GlobalVariables/<グループ名>.json)
///  ・デバッグ時はJSONファイルの更新を監視し、外部で書き換えられたら自動でリロードする(ホットリロード)
///
/// 使い方(例):
///   auto* gv = GlobalVariables::GetInstance();
///   gv->CreateGroup("Player");
///   gv->AddItem("Player", "speed", 3.0f);   // 既に読み込み済みなら上書きしない(デフォルト値扱い)
///   float speed = gv->GetFloatValue("Player", "speed");
/// </summary>
class GlobalVariables{
public:
	// シングルトンのインスタンス取得
	static GlobalVariables* GetInstance();

	// グループを作成する(既にあれば何もしない)
	void CreateGroup(const std::string& groupName);

	// --- 項目の登録(まだ無いキーのときだけデフォルト値として追加する) ---
	void AddItem(const std::string& groupName,const std::string& key,int32_t value);
	void AddItem(const std::string& groupName,const std::string& key,float value);
	void AddItem(const std::string& groupName,const std::string& key,bool value);
	void AddItem(const std::string& groupName,const std::string& key,const Vector3& value);

	// --- 値の設定(既存キーを上書き、無ければ追加) ---
	void SetValue(const std::string& groupName,const std::string& key,int32_t value);
	void SetValue(const std::string& groupName,const std::string& key,float value);
	void SetValue(const std::string& groupName,const std::string& key,bool value);
	void SetValue(const std::string& groupName,const std::string& key,const Vector3& value);

	// --- 値の取得(キーが無い/型違いのときはゼロ値を返す) ---
	int32_t GetIntValue(const std::string& groupName,const std::string& key) const;
	float GetFloatValue(const std::string& groupName,const std::string& key) const;
	bool GetBoolValue(const std::string& groupName,const std::string& key) const;
	Vector3 GetVector3Value(const std::string& groupName,const std::string& key) const;

	// 毎フレーム更新:ファイル監視(ホットリロード)+ ImGui描画
	void Update();

	// 全グループを起動時にまとめて読み込む
	void LoadFiles();
	// 指定グループを1つJSONから読み込む
	void LoadFile(const std::string& groupName);
	// 指定グループをJSONへ保存する
	void SaveFile(const std::string& groupName);

private:
	GlobalVariables() = default;
	~GlobalVariables() = default;
	GlobalVariables(const GlobalVariables&) = delete;
	GlobalVariables& operator=(const GlobalVariables&) = delete;

	// デバッグ時のみ:JSONファイルの更新時刻を監視し、変わっていたら読み直す
	void CheckFileUpdate();

	// 1項目が取り得る値の型(この順序がJSON保存/復元の型判定に対応)
	using Item = std::variant<int32_t, float, bool, Vector3>;

	// 1グループ分のデータ
	struct Group{
		std::map<std::string, Item> items;                        // キー→値
		std::filesystem::file_time_type lastWriteTime{};          // 監視用:最後に読み書きしたファイルの更新時刻
		bool hasWriteTime = false;                                // lastWriteTimeが有効かどうか
	};

	// グループ名→グループ
	std::map<std::string, Group> datas_;

	// 保存先ディレクトリ(resource配下の相対パス。LevelManagerと同じ流儀)
	const std::string kDirectoryPath = "resource/GlobalVariables/";
};
