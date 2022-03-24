// FileFinder.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>
#include <iostream>
#include <list>
#include <vector>
#include "ScanManager.h"

std::unique_ptr<ScanManager> ScanManager::s_ScanManager = nullptr;

int wmain(int argc, wchar_t *argv[])
{
	int iChoice;

	//
	//	Program should have atleast 3 commadline arguments.
	//
	if (argc < 3)
	{
		wprintf(L"Invalid commandline arguments\n");
		wprintf(L"file-finder <dir> <substring1>[<substring2> [<substring3>]\n");
		return 0;
	}

	//
	//	I am assuming that it will have full path of directory as an argument to scan files.
	//
	std::wstring directoryToScan = argv[1];

	//
	//	Check directory existance.
	//
	if (-1 == _waccess(directoryToScan.c_str(), 00))
	{
		wprintf(L"Invalid directory path!!!\n");
		return 0;
	}

	//
	//	Add substrings to the list.
	//
	std::vector<std::wstring> substrList;
	for (int i = 2; i < argc; i++)
	{
		substrList.push_back(argv[i]);
	}

	ScanManager::Create(directoryToScan, (int)substrList.size(), substrList);

	bool boRet = ScanManager::GetInstance()->InitScan();
	if (false == boRet)
	{
		ScanManager::Release();
		return 0;
	}

	do
	{
		wprintf(L"\n1. Dump scan result \n0. Exit \nEnter Choice: ");
		int iRet = wscanf_s(L"%d", &iChoice);
		while (iRet != 1)
		{
			//
			//	To handle condition when user enter choice other than number.
			//
			int temp;
			while ((temp = getchar()) != EOF && temp != '\n');
			wprintf(L"Invalid input\n");
			wprintf(L"\n1. Dump scan result \n0. Exit \nEnter Choice: ");
			iRet = wscanf_s(L"%d", &iChoice);
		}

		switch (iChoice)
		{
		case 0:
			break;

		case 1:
			//
			//	set event to dump scanning result.
			//
			SetEvent(ScanManager::GetInstance()->GetScanResultDumpEvent());
			break;

		default:
			wprintf(L"\nYou have entered invalid choice!!!");

		}

	} while (iChoice != 0);

	ScanManager::GetInstance()->DeinitScan();
	ScanManager::Release();

	return 0;
}
