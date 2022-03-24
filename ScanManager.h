#pragma once

#define SECOND						(1000 * 1)
#define MIN							(SECOND * 60)
#define SCAN_RESULT_DUMP_TIMEOUT	(5 * SECOND)

class ScanManager
{
	typedef struct _ScanContext
	{
		int iSubstringIndex;

	} ScanContext;

private:
	ScanManager(std::wstring directoryToScan, int iNoOfScanningThreads, std::vector<std::wstring>& SubstrList);

public:
	~ScanManager();

	static ScanManager* GetInstance();
	static void Create(std::wstring directoryToScan, int iNoOfScanningThreads, std::vector<std::wstring>& SubstrList);
	static void Release();

	bool InitScan();
	bool DeinitScan();

	static DWORD WINAPI ThreadFileFinderScan(void* parameter);
	static DWORD WINAPI ThreadScanResultDump(void* parameter);

	HANDLE& GetThreadStopEvent();
	HANDLE& GetScanResultDumpEvent();

	bool StartScanning(int iSubstringFilterIndex);
	bool DumpAndClearScanResult();

private:
	void Lock();
	void Unlock();

	bool GetSubstringFilter(int iIndex, std::wstring& substringFilter);
	bool EnumerateFilesWithFilter(const std::wstring& dirPath, bool bRecursive, const std::wstring& lcfilter, DWORD* pdwError);
	void ToLowerCase(std::wstring& str);

private:

	static std::unique_ptr<ScanManager> s_ScanManager;

	std::wstring m_strDirectoryToScan;
	int m_iNoOfScanningThreads;
	HANDLE m_hScanResultDumperThread;
	HANDLE* m_arrScanningThreads;
	ScanContext* m_arrScanContext;

	HANDLE m_hEventStopThreads;	//	Event which will be use to tell all threads to exit.
	HANDLE m_hEventDumpScanResult;	//	Event which will be use to tell result dumper thread to display scan result on console.

	std::vector<std::wstring> m_substringList;

	std::list<std::wstring> m_ScanResultList;
	CRITICAL_SECTION m_scanResultListLock;

};
