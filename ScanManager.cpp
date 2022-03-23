#include <Windows.h>
#include <iostream>
#include <list>
#include <vector>
#include <algorithm>
#include"ScanManager.h"

ScanManager::ScanManager(std::wstring directoryToScan, int iNoOfScanningThreads, std::vector<std::wstring>& SubstrList) :m_strDirectoryToScan(directoryToScan), m_iNoOfScanningThreads(iNoOfScanningThreads), m_substringList(SubstrList)
{
	m_arrScanningThreads = NULL;
	m_arrScanContext = NULL;
	m_hEventStopThreads = NULL;
	m_hEventDumpScanResult = NULL;
	m_hScanResultDumperThread = NULL;

	InitializeCriticalSection(&m_scanResultListLock);

	m_arrScanningThreads = new(std::nothrow) HANDLE[m_iNoOfScanningThreads];
	if (NULL == m_arrScanningThreads)
	{
		return;
	}

	for (int i = 0; i < m_iNoOfScanningThreads; i++)
	{
		m_arrScanningThreads[i] = NULL;
	}

	m_arrScanContext = new(std::nothrow) ScanContext[m_iNoOfScanningThreads];
	if (NULL == m_arrScanContext)
	{
		return;
	}
}

ScanManager::~ScanManager()
{
	if (NULL != m_arrScanContext)
	{
		delete[] m_arrScanContext;
	}

	if (NULL != m_arrScanningThreads)
	{
		delete[] m_arrScanningThreads;
	}

	DeleteCriticalSection(&m_scanResultListLock);
}

ScanManager* ScanManager::GetInstance()
{
	return s_ScanManager.get();
}

void ScanManager::Create(std::wstring directoryToScan, int iNoOfScanningThreads, std::vector<std::wstring>& SubstrList)
{
	if (NULL == s_ScanManager)
	{
		s_ScanManager.reset(new ScanManager(directoryToScan, iNoOfScanningThreads, SubstrList));
	}
}

void ScanManager::Release()
{
	s_ScanManager.reset(nullptr);
}

bool ScanManager::InitScan()
{
	if (0 == m_iNoOfScanningThreads)
	{
		return false;
	}

	m_hEventStopThreads = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (NULL == m_hEventStopThreads)
	{
		return false;
	}

	m_hEventDumpScanResult = CreateEventW(NULL, FALSE, FALSE, NULL);
	if (NULL == m_hEventDumpScanResult)
	{
		CloseHandle(m_hEventStopThreads);
		m_hEventStopThreads = NULL;
		return false;
	}

	m_hScanResultDumperThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)this->ThreadScanResultDump, this, 0, NULL);
	if (NULL == m_hScanResultDumperThread)
	{
		CloseHandle(m_hEventDumpScanResult);
		m_hEventDumpScanResult = NULL;
		CloseHandle(m_hEventStopThreads);
		m_hEventStopThreads = NULL;
		return false;
	}

	for (int i = 0; i < m_iNoOfScanningThreads; i++)
	{
		m_arrScanContext[i].iSubstringIndex = i;

		m_arrScanningThreads[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)this->ThreadFileFinderScan, &m_arrScanContext[i], 0, NULL);
		if (NULL == m_arrScanningThreads[i])
		{
			//
			//	Unable to create thread. we must unblock previously created threads waiting for stop threads event.
			//
			SetEvent(m_hEventStopThreads);
			WaitForSingleObject(m_hScanResultDumperThread, INFINITE);
			CloseHandle(m_hScanResultDumperThread);
			m_hScanResultDumperThread = NULL;

			if (i > 0)
			{
				WaitForMultipleObjects(i-1, m_arrScanningThreads, TRUE, INFINITE);
				for (int j = (i - 1); j >= 0; j--)
				{
					CloseHandle(m_arrScanningThreads[j]);
					m_arrScanningThreads[j] = NULL;
				}
			}

			CloseHandle(m_hEventDumpScanResult);
			m_hEventDumpScanResult = NULL;
			CloseHandle(m_hEventStopThreads);
			m_hEventStopThreads = NULL;
			return false;
		}
	}

	return true;
}

bool ScanManager::DeinitScan()
{
	SetEvent(m_hEventStopThreads);

	WaitForSingleObject(m_hScanResultDumperThread, INFINITE);
	CloseHandle(m_hScanResultDumperThread);
	m_hScanResultDumperThread = NULL;

	for (int i = 0; i < m_iNoOfScanningThreads; i++)
	{
		WaitForSingleObject(m_arrScanningThreads[i], INFINITE);
		CloseHandle(m_arrScanningThreads[i]);
		m_arrScanningThreads[i] = NULL;
	}

	CloseHandle(m_hEventDumpScanResult);
	m_hEventDumpScanResult = NULL;

	CloseHandle(m_hEventStopThreads);
	m_hEventStopThreads = NULL;

	return true;
}

DWORD __stdcall ScanManager::ThreadFileFinderScan(void* parameter)
{
	if (NULL == parameter)
	{
		wprintf(L"ThreadFileFinderScan: Invalid parameter.");
		return 0;
	}

	ScanContext* pScanContext = (ScanContext*)parameter;

	bool boRet = ScanManager::GetInstance()->StartScanning(pScanContext->iSubstringIndex);
	if (false == boRet)
	{
		wprintf(L"\n ThreadFileFinderScan: StartScanning() failed.");
		return 0;
	}

	wprintf(L"\n ThreadFileFinderScan: terminating scanner thread.");

	return 0;
}

DWORD __stdcall ScanManager::ThreadScanResultDump(void* parameter)
{
	UNREFERENCED_PARAMETER(parameter);

	HANDLE hEvents[2];
	DWORD dwWaitResult;

	hEvents[0] = ScanManager::GetInstance()->GetThreadStopEvent();
	hEvents[1] = ScanManager::GetInstance()->GetScanResultDumpEvent();

	while (true)
	{
		dwWaitResult = WaitForMultipleObjects(2, hEvents, FALSE, SCAN_RESULT_DUMP_TIMEOUT);
		if ((dwWaitResult - WAIT_OBJECT_0) == 0)
		{
			wprintf(L"\nThreadScanResultDump: stop threads event signalled, terminating scan result dumper thread.");
			break;
		}

		if (WAIT_FAILED == dwWaitResult)
		{
			wprintf(L"\nThreadScanResultDump: WaitForMultipleObjects failed with error(%u).", GetLastError());
			break;
		}

		ScanManager::GetInstance()->DumpAndClearScanResult();
	}

	return 0;
}

HANDLE& ScanManager::GetThreadStopEvent()
{
	return m_hEventStopThreads;
}

HANDLE& ScanManager::GetScanResultDumpEvent()
{
	return m_hEventDumpScanResult;
}

bool ScanManager::StartScanning(int iSubstringFilterIndex)
{
	std::wstring directoryToScan = m_strDirectoryToScan;

	//
	//	Append "\*" to search complete directory, append only "*" if last character is "\\"
	//
	if (L'\\' == directoryToScan.back())
	{
		directoryToScan += L"*";
	}
	else
	{
		directoryToScan += L"\\*";
	}

	std::wstring lcfilter;
	bool boRet = GetSubstringFilter(iSubstringFilterIndex, lcfilter);
	if (false == boRet)
	{
		wprintf(L"StartScanning: GetSubstringFilter failed.");
		return false;
	}

	ToLowerCase(lcfilter);

	DWORD dwError;
	boRet = EnumerateFilesWithFilter(directoryToScan, true, lcfilter, &dwError);
	if (false == boRet)
	{
		wprintf(L"StartScanning: EnumerateFilesWithFilter failed with error(%u).", dwError);
		return false;
	}

	return true;
}

bool ScanManager::DumpAndClearScanResult()
{
	//
	//	Dump scanning result by acquiring lock.
	//
	Lock();

	if (m_ScanResultList.empty())
	{
		wprintf(L"Scan result list is empty. \n");
		Unlock();
		return true;
	}

	std::list<std::wstring>::iterator iter;
	for (iter = m_ScanResultList.begin(); iter != m_ScanResultList.end(); iter++)
	{
		std::wstring filePath = *iter;
		wprintf(L"File : (%s)\n", filePath.c_str());
	}

	m_ScanResultList.clear();

	Unlock();

	return true;
}

void ScanManager::Lock()
{
	EnterCriticalSection(&m_scanResultListLock);
}

void ScanManager::Unlock()
{
	LeaveCriticalSection(&m_scanResultListLock);
}

bool ScanManager::GetSubstringFilter(int iIndex, std::wstring& substringFilter)
{
	if ((size_t)iIndex > m_substringList.size())
	{
		return false;
	}
	substringFilter = m_substringList[iIndex];
	return true;
}

bool ScanManager::EnumerateFilesWithFilter(const std::wstring& dirPath, bool bRecursive, const std::wstring &lcfilter, DWORD* pdwError)
{
	HANDLE hFind;
	WIN32_FIND_DATA FindData;

	if (NULL == pdwError)
	{
		return false;
	}
	*pdwError = 0;

	hFind = FindFirstFileW(dirPath.c_str(), &FindData);
	if (INVALID_HANDLE_VALUE == hFind)
	{
		*pdwError = GetLastError();
		return false;
	}

	do
	{
		DWORD dwWaitResult;
		dwWaitResult = WaitForSingleObject(m_hEventStopThreads, 0);
		if (WAIT_OBJECT_0 == dwWaitResult)
		{
			//wprintf(L"EnumerateFilesWithFilter: Stop thread event signal.");
			break;
		}

		//
		//	Skip current and parent directory entry.
		//
		if (0 == _wcsicmp(FindData.cFileName, L".") || 0 == _wcsicmp(FindData.cFileName, L".."))
		{
			continue;
		}

		if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (true == bRecursive)
			{
				std::wstring newDirPath = dirPath;

				std::size_t pos = newDirPath.rfind(L"*");
				if (std::wstring::npos != pos)
				{
					//
					//	Create a new directory path and pass recursively for scanning.
					//
					newDirPath.erase(pos);
					newDirPath += FindData.cFileName;
					newDirPath += L"\\*";
				}
				EnumerateFilesWithFilter(newDirPath, bRecursive, lcfilter, pdwError);
			}
		}
		else
		{
			std::wstring lcfileName = FindData.cFileName;
			ToLowerCase(lcfileName);

			std::size_t found = lcfileName.find(lcfilter);
			if (std::wstring::npos == found)
			{
				//
				//	File name is not matching with the filter hence skip this file.
				//
				continue;
			}

			std::wstring newFilePath = dirPath;

			std::size_t pos = newFilePath.rfind(L"*");
			if (std::wstring::npos != pos)
			{
				newFilePath.erase(pos);
			}
			newFilePath += FindData.cFileName;

			//
			//	Acquire lock and insert file path to list.
			//
			Lock();
			m_ScanResultList.push_back(newFilePath);
			Unlock();
		}

	} while (FindNextFileW(hFind, &FindData));

	FindClose(hFind);
	return true;
}

void ScanManager::ToLowerCase(std::wstring& str)
{
	std::transform(str.begin(), str.end(), str.begin(), [](wchar_t c) { return std::tolower(c); });
}
