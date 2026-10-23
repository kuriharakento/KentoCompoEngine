#include "input/SerialPort.h"

namespace KCE
{
SerialPort::~SerialPort()
{
	Close();
}

bool SerialPort::Open(const std::string& portName, uint32_t baudRate, uint32_t readTimeoutMs)
{
	Close();

	// COM10 以上は "\\.\" を付けないと開けないので、常に付けておく
	const std::string path = "\\\\.\\" + portName;
	handle_ = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (handle_ == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DCB dcb{};
	dcb.DCBlength = sizeof(dcb);
	if (!GetCommState(handle_, &dcb))
	{
		Close();
		return false;
	}
	dcb.BaudRate = baudRate;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.fBinary = TRUE;
	dcb.fParity = FALSE;
	dcb.fOutxCtsFlow = FALSE;
	dcb.fOutxDsrFlow = FALSE;
	dcb.fDtrControl = DTR_CONTROL_ENABLE;
	dcb.fRtsControl = RTS_CONTROL_ENABLE;
	dcb.fOutX = FALSE;
	dcb.fInX = FALSE;
	if (!SetCommState(handle_, &dcb))
	{
		Close();
		return false;
	}

	// 1 バイトでも届いたらすぐ戻る。何も届かなければ readTimeoutMs で戻る
	COMMTIMEOUTS timeouts{};
	timeouts.ReadIntervalTimeout = MAXDWORD;
	timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
	timeouts.ReadTotalTimeoutConstant = readTimeoutMs;
	if (!SetCommTimeouts(handle_, &timeouts))
	{
		Close();
		return false;
	}

	// 開く前に溜まっていた古いデータは捨てる
	PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);
	return true;
}

void SerialPort::Close()
{
	if (handle_ != INVALID_HANDLE_VALUE)
	{
		CloseHandle(handle_);
		handle_ = INVALID_HANDLE_VALUE;
	}
}

int SerialPort::Read(char* buffer, size_t size)
{
	if (!IsOpen())
	{
		return -1;
	}
	DWORD readBytes = 0;
	if (!ReadFile(handle_, buffer, static_cast<DWORD>(size), &readBytes, nullptr))
	{
		return -1;
	}
	return static_cast<int>(readBytes);
}
} // namespace KCE
