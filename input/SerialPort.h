#pragma once
#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <string>

namespace KCE
{
/**
 * @brief COM ポートの薄いラッパー。
 *
 * - 開く・閉じる・読むだけ。届いたデータの中身は知らない
 * - Read は設定した時間だけ待って戻る（ずっと止まらない）。受信スレッドから使う前提
 * - コピー禁止。1 つのポートを 1 つのインスタンスで持つ
 */
class SerialPort
{
public:
	SerialPort() = default;
	~SerialPort();

	SerialPort(const SerialPort&) = delete;
	SerialPort& operator=(const SerialPort&) = delete;

	/**
	 * @brief ポートを開く。開いていたら閉じてから開き直す。
	 * @param portName "COM3" のようなポート名
	 * @param baudRate 通信速度。マイコン側の Serial.begin と合わせる
	 * @param readTimeoutMs Read が何も届かないときに待つ時間（ミリ秒）
	 * @return 開けたら true
	 */
	bool Open(const std::string& portName, uint32_t baudRate, uint32_t readTimeoutMs = 50);

	/**
	 * @brief ポートを閉じる。開いていなければ何もしない。
	 */
	void Close();

	/**
	 * @brief 開いているか。
	 * @return 開いていれば true
	 */
	bool IsOpen() const { return handle_ != INVALID_HANDLE_VALUE; }

	/**
	 * @brief 届いているデータを読む。
	 * @param buffer 読み込み先
	 * @param size buffer の大きさ
	 * @return 読めたバイト数。時間切れなら 0、エラー（ケーブルが抜けたなど）なら -1
	 */
	int Read(char* buffer, size_t size);

private:
	HANDLE handle_ = INVALID_HANDLE_VALUE;
};
} // namespace KCE
