#include <iostream>
#include <conio.h>
#include <windows.h>


using namespace std;
bool gameOver;
const int width = 20;
const int height = 20;
int x, y, fruitX, fruitY, score;
int tailX[100], tailY[100];
int nTail;
enum eDirecton { STOP = 0, LEFT, RIGHT, UP, DOWN };
eDirecton dir;
void Draw()
{
	system("cls"); //system("clear");
	for (int i = 0; i < width + 2; i++)
		cout << "#";
	cout << endl;

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			if (j == 0)
				cout << "#";
			if (i == y && j == x)
				cout << "O";
			else if (i == fruitY && j == fruitX)
				cout << "F";
			else
			{
				bool print = false;
				for (int k = 0; k < nTail; k++)
				{
					if (tailX[k] == j && tailY[k] == i)
					{
						cout << "o";
						print = true;
					}
				}
				if (!print)
					cout << " ";
			}


			if (j == width - 1)
				cout << "#";
		}
		cout << endl;
	}

	for (int i = 0; i < width + 2; i++)
		cout << "#";
	cout << endl;
	cout << "Score:" << score << endl;
}
void Input()
{
	if (_kbhit())
	{
		switch (_getch())
		{
		case 'a':
			dir = LEFT;
			break;
		case 'd':
			dir = RIGHT;
			break;
		case 'w':
			dir = UP;
			break;
		case 's':
			dir = DOWN;
			break;
		case 'x':
			gameOver = true;
			break;
		default:
			break;
		}
	}
}


typedef NTSTATUS(NTAPI *pfnNtSetInformationThread)(
	_In_ HANDLE ThreadHandle,
	_In_ ULONG  ThreadInformationClass,
	_In_ PVOID  ThreadInformation,
	_In_ ULONG  ThreadInformationLength
	);
const ULONG ThreadHideFromDebugger = 0x11;
#define FLG_HEAP_ENABLE_TAIL_CHECK   0x10
#define FLG_HEAP_ENABLE_FREE_CHECK   0x20
#define FLG_HEAP_VALIDATE_PARAMETERS 0x40
#define NT_GLOBAL_FLAG_DEBUGGED (FLG_HEAP_ENABLE_TAIL_CHECK | FLG_HEAP_ENABLE_FREE_CHECK | FLG_HEAP_VALIDATE_PARAMETERS)

PVOID GetPEB64()
{
	PVOID pPeb = 0;
#ifndef _WIN64
	// 1. There are two copies of PEB - PEB64 and PEB32 in WOW64 process
	// 2. PEB64 follows after PEB32
	// 3. This is true for versions lower than Windows 8, else __readfsdword returns address of real PEB64
	if (IsWin8OrHigher())
	{
		BOOL isWow64 = FALSE;
		typedef BOOL(WINAPI *pfnIsWow64Process)(HANDLE hProcess, PBOOL isWow64);
		pfnIsWow64Process fnIsWow64Process = (pfnIsWow64Process)
			GetProcAddress(GetModuleHandleA("Kernel32.dll"), "IsWow64Process");
		if (fnIsWow64Process(GetCurrentProcess(), &isWow64))
		{
			if (isWow64)
			{
				pPeb = (PVOID)__readfsdword(0x0C * sizeof(PVOID));
				pPeb = (PVOID)((PBYTE)pPeb + 0x1000);
			}
		}
	}
#endif
	return pPeb;
}


// Current PEB for 64bit and 32bit processes accordingly
PVOID GetPEB()
{
	return (PVOID)__readgsqword(0x0C * sizeof(PVOID));
}

PIMAGE_NT_HEADERS GetImageNtHeaders(PBYTE pImageBase)
{
	PIMAGE_DOS_HEADER pImageDosHeader = (PIMAGE_DOS_HEADER)pImageBase;
	return (PIMAGE_NT_HEADERS)(pImageBase + pImageDosHeader->e_lfanew);
}
PIMAGE_SECTION_HEADER FindRDataSection(PBYTE pImageBase)
{
	static const std::string rdata = ".rdata";
	PIMAGE_NT_HEADERS pImageNtHeaders = GetImageNtHeaders(pImageBase);
	PIMAGE_SECTION_HEADER pImageSectionHeader = IMAGE_FIRST_SECTION(pImageNtHeaders);
	int n = 0;
	for (; n < pImageNtHeaders->FileHeader.NumberOfSections; ++n)
	{
		if (rdata == (char*)pImageSectionHeader[n].Name)
		{
			break;
		}
	}
	return &pImageSectionHeader[n];
}
#pragma warning(disable : 4996)
WORD GetVersionWord()
{
	OSVERSIONINFO verInfo = { sizeof(OSVERSIONINFO) };
	GetVersionEx(&verInfo);
	return MAKEWORD(verInfo.dwMinorVersion, verInfo.dwMajorVersion);
}
BOOL IsWin8OrHigher() { return GetVersionWord() >= _WIN32_WINNT_WIN8; }
BOOL IsVistaOrHigher() { return GetVersionWord() >= _WIN32_WINNT_VISTA; }

int GetHeapFlagsOffset(bool x64)
{
	return x64 ?
		IsVistaOrHigher() ? 0x70 : 0x14 : //x64 offsets
		IsVistaOrHigher() ? 0x40 : 0x0C; //x86 offsets
}
int GetHeapForceFlagsOffset(bool x64)
{
	return x64 ?
		IsVistaOrHigher() ? 0x74 : 0x18 : //x64 offsets
		IsVistaOrHigher() ? 0x44 : 0x10; //x86 offsets
}

typedef NTSTATUS(NTAPI *pfnNtQueryInformationProcess)(
	_In_      HANDLE           ProcessHandle,
	_In_      UINT             ProcessInformationClass,
	_Out_     PVOID            ProcessInformation,
	_In_      ULONG            ProcessInformationLength,
	_Out_opt_ PULONG           ReturnLength
	);
const UINT ProcessDebugPort = 7;

DWORD CalcFuncCrc(PUCHAR funcBegin, PUCHAR funcEnd)
{
	DWORD crc = 0;
	for (; funcBegin < funcEnd; ++funcBegin)
	{
		crc += *funcBegin;
	}
	return crc;
}
const int g_doSmthExecutionTime = 1050;
void DoSmth()
{
	Sleep(1000);
}

void SetupMain()
{
	HMODULE hNtDll = LoadLibrary(TEXT("ntdll.dll"));
	gameOver = false;
	pfnNtSetInformationThread NtSetInformationThread = (pfnNtSetInformationThread)GetProcAddress(hNtDll, "NtSetInformationThread");
	NTSTATUS status = NtSetInformationThread(GetCurrentThread(),ThreadHideFromDebugger, NULL, 0);
	dir = STOP;
	HANDLE hProcess = NULL;
	DEBUG_EVENT de;
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	x = width / 2;
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&si, sizeof(STARTUPINFO));
	ZeroMemory(&de, sizeof(DEBUG_EVENT));
	y = height / 2;
	GetStartupInfo(&si);
	fruitX = rand() % width;
	// Create the copy of ourself
	CreateProcess(NULL, GetCommandLine(), NULL, NULL, FALSE,DEBUG_PROCESS, NULL, NULL, &si, &pi);
	fruitY = rand() % height;
	// Continue execution
	ContinueDebugEvent(pi.dwProcessId, pi.dwThreadId, DBG_CONTINUE);
	score = 0;
	// Wait for an event
	WaitForDebugEvent(&de, INFINITE);




	if (IsDebuggerPresent())
	{
		std::cout << "Stop debugging program! 13" << std::endl;
		exit(-1);
	}
	BOOL tisDebuggerPresent = FALSE;
	if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &tisDebuggerPresent))
	{
		if (tisDebuggerPresent)
		{
			std::cout << "Stop debugging program! 12" << std::endl;
			exit(-1);
		}
	}
	PVOID pPeb = GetPEB();
	PVOID pPeb64 = GetPEB64();
	DWORD offsetNtGlobalFlag = 0;
	offsetNtGlobalFlag = 0xBC;
	DWORD NtGlobalFlag = *(PDWORD)((PBYTE)pPeb + offsetNtGlobalFlag);
	if (NtGlobalFlag & NT_GLOBAL_FLAG_DEBUGGED)
	{
		std::cout << "Stop debugging program! 11" << std::endl;
		exit(-1);
	}
	if (pPeb64)
	{
		DWORD NtGlobalFlagWow64 = *(PDWORD)((PBYTE)pPeb64 + 0xBC);
		if (NtGlobalFlagWow64 & NT_GLOBAL_FLAG_DEBUGGED)
		{
			std::cout << "Stop debugging program! 10" << std::endl;
			exit(-1);
		}
	}
	PBYTE pImageBase = (PBYTE)GetModuleHandle(NULL);
	PIMAGE_NT_HEADERS pImageNtHeaders = GetImageNtHeaders(pImageBase);
	PIMAGE_LOAD_CONFIG_DIRECTORY pImageLoadConfigDirectory = (PIMAGE_LOAD_CONFIG_DIRECTORY)(pImageBase
		+ pImageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress);
	if (pImageLoadConfigDirectory->GlobalFlagsClear != 0)
	{
		std::cout << "Stop debugging program! 9" << std::endl;
		exit(-1);
	}
	HANDLE hExecutable = INVALID_HANDLE_VALUE;
	HANDLE hExecutableMapping = NULL;
	PBYTE pMappedImageBase = NULL;
	__try
	{
		PBYTE pImageBase = (PBYTE)GetModuleHandle(NULL);
		PIMAGE_SECTION_HEADER pImageSectionHeader = FindRDataSection(pImageBase);
		TCHAR pszExecutablePath[MAX_PATH];
		DWORD dwPathLength = GetModuleFileName(NULL, pszExecutablePath, MAX_PATH);
		if (0 == dwPathLength) __leave;
		hExecutable = CreateFile(pszExecutablePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (INVALID_HANDLE_VALUE == hExecutable) __leave;
		hExecutableMapping = CreateFileMapping(hExecutable, NULL, PAGE_READONLY, 0, 0, NULL);
		if (NULL == hExecutableMapping) __leave;
		pMappedImageBase = (PBYTE)MapViewOfFile(hExecutableMapping, FILE_MAP_READ, 0, 0,
			pImageSectionHeader->PointerToRawData + pImageSectionHeader->SizeOfRawData);
		if (NULL == pMappedImageBase) __leave;
		PIMAGE_NT_HEADERS pImageNtHeaders = GetImageNtHeaders(pMappedImageBase);
		PIMAGE_LOAD_CONFIG_DIRECTORY pImageLoadConfigDirectory = (PIMAGE_LOAD_CONFIG_DIRECTORY)(pMappedImageBase
			+ (pImageSectionHeader->PointerToRawData
				+ (pImageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress - pImageSectionHeader->VirtualAddress)));
		if (pImageLoadConfigDirectory->GlobalFlagsClear != 0)
		{
			std::cout << "Stop debugging program! 8" << std::endl;
			exit(-1);
		}
	}
	__finally
	{
		if (NULL != pMappedImageBase)
			UnmapViewOfFile(pMappedImageBase);
		if (NULL != hExecutableMapping)
			CloseHandle(hExecutableMapping);
		if (INVALID_HANDLE_VALUE != hExecutable)
			CloseHandle(hExecutable);
	}
	PVOID tpPeb = GetPEB();
	PVOID tpPeb64 = GetPEB64();
	PVOID heap = 0;
	DWORD offsetProcessHeap = 0;
	PDWORD heapFlagsPtr = 0, heapForceFlagsPtr = 0;
	BOOL x64 = FALSE;
	x64 = TRUE;
	offsetProcessHeap = 0x30;

	heap = (PVOID)*(PDWORD_PTR)((PBYTE)tpPeb + offsetProcessHeap);
	heapFlagsPtr = (PDWORD)((PBYTE)heap + GetHeapFlagsOffset(x64));
	heapForceFlagsPtr = (PDWORD)((PBYTE)heap + GetHeapForceFlagsOffset(x64));
	if (*heapFlagsPtr & ~HEAP_GROWABLE || *heapForceFlagsPtr != 0)
	{
		std::cout << "Stop debugging program! 7" << std::endl;
		exit(-1);
	}
	if (tpPeb64)
	{
		heap = (PVOID)*(PDWORD_PTR)((PBYTE)tpPeb64 + 0x30);
		heapFlagsPtr = (PDWORD)((PBYTE)heap + GetHeapFlagsOffset(true));
		heapForceFlagsPtr = (PDWORD)((PBYTE)heap + GetHeapForceFlagsOffset(true));
		if (*heapFlagsPtr & ~HEAP_GROWABLE || *heapForceFlagsPtr != 0)
		{
			std::cout << "Stop debugging program! 6" << std::endl;
			exit(-1);
		}
	}
	BOOL ttisDebuggerPresent = FALSE;
	if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &ttisDebuggerPresent))
	{
		if (ttisDebuggerPresent)
		{
			std::cout << "Stop debugging program! 5" << std::endl;
			exit(-1);
		}
	}
	pfnNtQueryInformationProcess NtQueryInformationProcess = NULL;
	NTSTATUS ttstatus;
	DWORD tttisDebuggerPresent = 0;
	HMODULE thNtDll = LoadLibrary(TEXT("ntdll.dll"));

	if (NULL != thNtDll)
	{
		NtQueryInformationProcess = (pfnNtQueryInformationProcess)GetProcAddress(thNtDll, "NtQueryInformationProcess");
		if (NULL != NtQueryInformationProcess)
		{
			ttstatus = NtQueryInformationProcess(
				GetCurrentProcess(),
				ProcessDebugPort,
				&tttisDebuggerPresent,
				sizeof(DWORD),
				NULL);
			if (ttstatus == 0x00000000 && tttisDebuggerPresent != 0)
			{
				std::cout << "Stop debugging program! 4" << std::endl;
				exit(-1);
			}
		}

	}
	


	while (!gameOver)
	{

		SYSTEMTIME sysTimeStart;
		SYSTEMTIME sysTimeEnd;
		FILETIME timeStart, timeEnd;
		HANDLE hProcess = NULL;
		DEBUG_EVENT de;
		PROCESS_INFORMATION pi;
		STARTUPINFO si;

		GetSystemTime(&sysTimeStart);
		DoSmth();
		GetSystemTime(&sysTimeEnd);
		SystemTimeToFileTime(&sysTimeStart, &timeStart);
		SystemTimeToFileTime(&sysTimeEnd, &timeEnd);
		double timeExecution = (timeEnd.dwLowDateTime - timeStart.dwLowDateTime) / 10000.0;
		if (timeExecution < g_doSmthExecutionTime)
		{
			while (!gameOver)
			{

				Draw();


				SYSTEMTIME sysTimeStart;
				SYSTEMTIME sysTimeEnd;
				FILETIME timeStart, timeEnd;
				HANDLE hProcess = NULL;
				DEBUG_EVENT de;
				PROCESS_INFORMATION pi;
				STARTUPINFO si;
				ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
				ZeroMemory(&si, sizeof(STARTUPINFO));
				ZeroMemory(&de, sizeof(DEBUG_EVENT));



				if (IsDebuggerPresent())
				{
					std::cout << "Stop debugging program! 13" << std::endl;
					exit(-1);
				}
				BOOL isDebuggerPresent = FALSE;
				if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &isDebuggerPresent))
				{
					if (isDebuggerPresent)
					{
						std::cout << "Stop debugging program! 12" << std::endl;
						exit(-1);
					}
				}
				PVOID pPeb = GetPEB();
				PVOID pPeb64 = GetPEB64();
				DWORD offsetNtGlobalFlag = 0;
				offsetNtGlobalFlag = 0xBC;
				DWORD NtGlobalFlag = *(PDWORD)((PBYTE)pPeb + offsetNtGlobalFlag);
				if (NtGlobalFlag & NT_GLOBAL_FLAG_DEBUGGED)
				{
					std::cout << "Stop debugging program! 11" << std::endl;
					exit(-1);
				}
				if (pPeb64)
				{
					DWORD NtGlobalFlagWow64 = *(PDWORD)((PBYTE)pPeb64 + 0xBC);
					if (NtGlobalFlagWow64 & NT_GLOBAL_FLAG_DEBUGGED)
					{
						std::cout << "Stop debugging program! 10" << std::endl;
						exit(-1);
					}
				}
				PBYTE pImageBase = (PBYTE)GetModuleHandle(NULL);
				PIMAGE_NT_HEADERS pImageNtHeaders = GetImageNtHeaders(pImageBase);
				PIMAGE_LOAD_CONFIG_DIRECTORY pImageLoadConfigDirectory = (PIMAGE_LOAD_CONFIG_DIRECTORY)(pImageBase
					+ pImageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress);
				if (pImageLoadConfigDirectory->GlobalFlagsClear != 0)
				{
					std::cout << "Stop debugging program! 9" << std::endl;
					exit(-1);
				}
				HANDLE hExecutable = INVALID_HANDLE_VALUE;
				HANDLE hExecutableMapping = NULL;
				PBYTE pMappedImageBase = NULL;
				__try
				{
					PBYTE pImageBase = (PBYTE)GetModuleHandle(NULL);
					PIMAGE_SECTION_HEADER pImageSectionHeader = FindRDataSection(pImageBase);
					TCHAR pszExecutablePath[MAX_PATH];
					DWORD dwPathLength = GetModuleFileName(NULL, pszExecutablePath, MAX_PATH);
					if (0 == dwPathLength) __leave;
					hExecutable = CreateFile(pszExecutablePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
					if (INVALID_HANDLE_VALUE == hExecutable) __leave;
					hExecutableMapping = CreateFileMapping(hExecutable, NULL, PAGE_READONLY, 0, 0, NULL);
					if (NULL == hExecutableMapping) __leave;
					pMappedImageBase = (PBYTE)MapViewOfFile(hExecutableMapping, FILE_MAP_READ, 0, 0,
						pImageSectionHeader->PointerToRawData + pImageSectionHeader->SizeOfRawData);
					if (NULL == pMappedImageBase) __leave;
					PIMAGE_NT_HEADERS pImageNtHeaders = GetImageNtHeaders(pMappedImageBase);
					PIMAGE_LOAD_CONFIG_DIRECTORY pImageLoadConfigDirectory = (PIMAGE_LOAD_CONFIG_DIRECTORY)(pMappedImageBase
						+ (pImageSectionHeader->PointerToRawData
							+ (pImageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress - pImageSectionHeader->VirtualAddress)));
					if (pImageLoadConfigDirectory->GlobalFlagsClear != 0)
					{
						std::cout << "Stop debugging program! 8" << std::endl;
						exit(-1);
					}
				}
				__finally
				{
					if (NULL != pMappedImageBase)
						UnmapViewOfFile(pMappedImageBase);
					if (NULL != hExecutableMapping)
						CloseHandle(hExecutableMapping);
					if (INVALID_HANDLE_VALUE != hExecutable)
						CloseHandle(hExecutable);
				}
				PVOID tpPeb = GetPEB();
				PVOID tpPeb64 = GetPEB64();
				PVOID heap = 0;
				DWORD offsetProcessHeap = 0;
				PDWORD heapFlagsPtr = 0, heapForceFlagsPtr = 0;
				BOOL x64 = FALSE;
				x64 = TRUE;
				offsetProcessHeap = 0x30;
				if (_kbhit())
				{
					switch (_getch())
					{
					case 'a':
						dir = LEFT;
						break;
					case 'd':
						dir = RIGHT;
						break;
					case 'w':
						dir = UP;
						break;
					case 's':
						dir = DOWN;
						break;
					case 'x':
						gameOver = true;
						break;
					default:
						break;
					}
				}
				heap = (PVOID)*(PDWORD_PTR)((PBYTE)tpPeb + offsetProcessHeap);
				int prevX = tailX[0];
				int prevY = tailY[0];
				int prev2X, prev2Y;
				heapFlagsPtr = (PDWORD)((PBYTE)heap + GetHeapFlagsOffset(x64));
				tailX[0] = x;
				tailY[0] = y;
				heapForceFlagsPtr = (PDWORD)((PBYTE)heap + GetHeapForceFlagsOffset(x64));
				
				for (int i = 1; i < nTail; i++)
				{
					prev2X = tailX[i];
					prev2Y = tailY[i];
					tailX[i] = prevX;
					tailY[i] = prevY;
					prevX = prev2X;
					prevY = prev2Y;
				}
				switch (dir)
				{
				case LEFT:
					x--;
					break;
				case RIGHT:
					x++;
					break;
				case UP:
					y--;
					break;
				case DOWN:
					y++;
					break;
				default:
					break;
				}
				if (*heapFlagsPtr & ~HEAP_GROWABLE || *heapForceFlagsPtr != 0)
				{
					std::cout << "Stop debugging program! 7" << std::endl;
					exit(-1);
				}
				
				if (x > width || x < 0 || y > height || y < 0)
					gameOver = true;
				if (tpPeb64)
				{
					heap = (PVOID)*(PDWORD_PTR)((PBYTE)tpPeb64 + 0x30);
					heapFlagsPtr = (PDWORD)((PBYTE)heap + GetHeapFlagsOffset(true));
					heapForceFlagsPtr = (PDWORD)((PBYTE)heap + GetHeapForceFlagsOffset(true));
					if (*heapFlagsPtr & ~HEAP_GROWABLE || *heapForceFlagsPtr != 0)
					{
						std::cout << "Stop debugging program! 6" << std::endl;
						exit(-1);
					}
				}
				BOOL ttisDebuggerPresent = FALSE;
				if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &ttisDebuggerPresent))
				{
					if (ttisDebuggerPresent)
					{
						std::cout << "Stop debugging program! 5" << std::endl;
						exit(-1);
					}
				}
				pfnNtQueryInformationProcess NtQueryInformationProcess = NULL;
				NTSTATUS status;
				DWORD tisDebuggerPresent = 0;
				HMODULE hNtDll = LoadLibrary(TEXT("ntdll.dll"));

			
				//if (x >= width) x = 0; else if (x < 0) x = width - 1;
				//if (y >= height) y = 0; else if (y < 0) y = height - 1;

				for (int i = 0; i < nTail; i++)
					if (tailX[i] == x && tailY[i] == y)
						gameOver = true;

				

				if (NULL != hNtDll)
				{
					NtQueryInformationProcess = (pfnNtQueryInformationProcess)GetProcAddress(hNtDll, "NtQueryInformationProcess");
					if (NULL != NtQueryInformationProcess)
					{
						status = NtQueryInformationProcess(
							GetCurrentProcess(),
							ProcessDebugPort,
							&tisDebuggerPresent,
							sizeof(DWORD),
							NULL);
						if (status == 0x00000000 && tisDebuggerPresent != 0)
						{
							std::cout << "Stop debugging program! 4" << std::endl;
							exit(-1);
						}
					}

				}
				if (x == fruitX && y == fruitY)
				{
					score += 10;
					fruitX = rand() % width;
					fruitY = rand() % height;
					nTail++;
				}

				Sleep(50);
			}

		}
		else
		{
			std::cout << "Stop debugging program!" << std::endl;
			exit(-1);
		}
		Sleep(50); //sleep(10);
	}

}

ULONG_PTR GetParentProcessId() // By Napalm @ NetCore2K
{
	ULONG_PTR pbi[6];
	ULONG ulSize = 0;
	LONG(WINAPI *NtQueryInformationProcess)(HANDLE ProcessHandle, ULONG ProcessInformationClass,
		PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength);
	*(FARPROC *)&NtQueryInformationProcess =
		GetProcAddress(LoadLibraryA("NTDLL.DLL"), "NtQueryInformationProcess");
	if (NtQueryInformationProcess) {
		if (NtQueryInformationProcess(GetCurrentProcess(), 0,
			&pbi, sizeof(pbi), &ulSize) >= 0 && ulSize == sizeof(pbi))
			return pbi[5];
	}
	return (ULONG_PTR)-1;
}



#include <tlhelp32.h>
#include <vector>
int main(int argc, char *argv[])
{
	int pid = -1;
	int pid2;
	HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 pe = { 0 };
	pe.dwSize = sizeof(PROCESSENTRY32);

	//assume first arg is the PID to get the PPID for, or use own PID
	if (argc > 1) {
		pid = atoi(argv[1]);
	}
	else {
		pid = GetCurrentProcessId();
	}
	int intCheck = 0;

	if (Process32First(h, &pe)) {
		do {
			if (pe.th32ProcessID == pid) 
			{
				pid2 = pe.th32ParentProcessID;
				intCheck++;
			}
		} while (Process32Next(h, &pe));
	}
	CloseHandle(h);
	SYSTEMTIME sysTimeStart;
	SYSTEMTIME sysTimeEnd;
	FILETIME timeStart, timeEnd;
	HMODULE hNtDll = LoadLibrary(TEXT("ntdll.dll"));
	pfnNtSetInformationThread NtSetInformationThread = (pfnNtSetInformationThread)
		GetProcAddress(hNtDll, "NtSetInformationThread");
	NTSTATUS status = NtSetInformationThread(GetCurrentThread(),
		ThreadHideFromDebugger, NULL, 0);
	GetSystemTime(&sysTimeStart);
	DoSmth();
	GetSystemTime(&sysTimeEnd);
	SystemTimeToFileTime(&sysTimeStart, &timeStart);
	SystemTimeToFileTime(&sysTimeEnd, &timeEnd);
	double timeExecution = (timeEnd.dwLowDateTime - timeStart.dwLowDateTime) / 10000.0;
	if (timeExecution < g_doSmthExecutionTime && intCheck == 1 )
	{
		SYSTEMTIME sysTimeStart;
		SYSTEMTIME sysTimeEnd;
		FILETIME timeStart, timeEnd;
		HMODULE hNtDll = LoadLibrary(TEXT("ntdll.dll"));
		pfnNtSetInformationThread NtSetInformationThread = (pfnNtSetInformationThread)
			GetProcAddress(hNtDll, "NtSetInformationThread");
		NTSTATUS status = NtSetInformationThread(GetCurrentThread(),
			ThreadHideFromDebugger, NULL, 0);

		if (IsDebuggerPresent())
		{
			std::cout << "Stop debugging program! 13" << std::endl;
			exit(-1);
		}
		BOOL tisDebuggerPresent = FALSE;
		if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &tisDebuggerPresent))
		{
			if (tisDebuggerPresent)
			{
				std::cout << "Stop debugging program! 12" << std::endl;
				exit(-1);
			}
		}
		
		PVOID pPeb = GetPEB();
		PVOID pPeb64 = GetPEB64();
		DWORD offsetNtGlobalFlag = 0;
		offsetNtGlobalFlag = 0xBC;
		DWORD NtGlobalFlag = *(PDWORD)((PBYTE)pPeb + offsetNtGlobalFlag);
		if (NtGlobalFlag & NT_GLOBAL_FLAG_DEBUGGED)
		{
			std::cout << "Stop debugging program! 11" << std::endl;
			exit(-1);
		}
		if (pPeb64)
		{
			DWORD NtGlobalFlagWow64 = *(PDWORD)((PBYTE)pPeb64 + 0xBC);
			if (NtGlobalFlagWow64 & NT_GLOBAL_FLAG_DEBUGGED)
			{
				std::cout << "Stop debugging program! 10" << std::endl;
				exit(-1);
			}
		}
		PBYTE pImageBase = (PBYTE)GetModuleHandle(NULL);
		PIMAGE_NT_HEADERS pImageNtHeaders = GetImageNtHeaders(pImageBase);
		PIMAGE_LOAD_CONFIG_DIRECTORY pImageLoadConfigDirectory = (PIMAGE_LOAD_CONFIG_DIRECTORY)(pImageBase
			+ pImageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress);
		if (pImageLoadConfigDirectory->GlobalFlagsClear != 0)
		{
			std::cout << "Stop debugging program! 9" << std::endl;
			exit(-1);
		}
		HANDLE hExecutable = INVALID_HANDLE_VALUE;
		HANDLE hExecutableMapping = NULL;
		PBYTE pMappedImageBase = NULL;
		__try
		{
			PBYTE pImageBase = (PBYTE)GetModuleHandle(NULL);
			PIMAGE_SECTION_HEADER pImageSectionHeader = FindRDataSection(pImageBase);
			TCHAR pszExecutablePath[MAX_PATH];
			DWORD dwPathLength = GetModuleFileName(NULL, pszExecutablePath, MAX_PATH);
			if (0 == dwPathLength) __leave;
			hExecutable = CreateFile(pszExecutablePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
			if (INVALID_HANDLE_VALUE == hExecutable) __leave;
			hExecutableMapping = CreateFileMapping(hExecutable, NULL, PAGE_READONLY, 0, 0, NULL);
			if (NULL == hExecutableMapping) __leave;
			pMappedImageBase = (PBYTE)MapViewOfFile(hExecutableMapping, FILE_MAP_READ, 0, 0,
				pImageSectionHeader->PointerToRawData + pImageSectionHeader->SizeOfRawData);
			if (NULL == pMappedImageBase) __leave;
			PIMAGE_NT_HEADERS pImageNtHeaders = GetImageNtHeaders(pMappedImageBase);
			PIMAGE_LOAD_CONFIG_DIRECTORY pImageLoadConfigDirectory = (PIMAGE_LOAD_CONFIG_DIRECTORY)(pMappedImageBase
				+ (pImageSectionHeader->PointerToRawData
					+ (pImageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress - pImageSectionHeader->VirtualAddress)));
			if (pImageLoadConfigDirectory->GlobalFlagsClear != 0)
			{
				std::cout << "Stop debugging program! 8" << std::endl;
				exit(-1);
			}
		}
		__finally
		{
			if (NULL != pMappedImageBase)
				UnmapViewOfFile(pMappedImageBase);
			if (NULL != hExecutableMapping)
				CloseHandle(hExecutableMapping);
			if (INVALID_HANDLE_VALUE != hExecutable)
				CloseHandle(hExecutable);
		}
		PVOID tpPeb = GetPEB();
		PVOID tpPeb64 = GetPEB64();
		PVOID heap = 0;
		DWORD offsetProcessHeap = 0;
		PDWORD heapFlagsPtr = 0, heapForceFlagsPtr = 0;
		BOOL x64 = FALSE;
		x64 = TRUE;
		offsetProcessHeap = 0x30;
		heap = (PVOID)*(PDWORD_PTR)((PBYTE)tpPeb + offsetProcessHeap);
		heapFlagsPtr = (PDWORD)((PBYTE)heap + GetHeapFlagsOffset(x64));
		heapForceFlagsPtr = (PDWORD)((PBYTE)heap + GetHeapForceFlagsOffset(x64));
		if (*heapFlagsPtr & ~HEAP_GROWABLE || *heapForceFlagsPtr != 0)
		{
			std::cout << "Stop debugging program! 7" << std::endl;
			exit(-1);
		}
		if (tpPeb64)
		{
			heap = (PVOID)*(PDWORD_PTR)((PBYTE)tpPeb64 + 0x30);
			heapFlagsPtr = (PDWORD)((PBYTE)heap + GetHeapFlagsOffset(true));
			heapForceFlagsPtr = (PDWORD)((PBYTE)heap + GetHeapForceFlagsOffset(true));
			if (*heapFlagsPtr & ~HEAP_GROWABLE || *heapForceFlagsPtr != 0)
			{
				std::cout << "Stop debugging program! 6" << std::endl;
				exit(-1);
			}
		}
		BOOL ttisDebuggerPresent = FALSE;
		if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &ttisDebuggerPresent))
		{
			if (ttisDebuggerPresent)
			{
				std::cout << "Stop debugging program! 5" << std::endl;
				exit(-1);
			}
		}
		pfnNtQueryInformationProcess NtQueryInformationProcess = NULL;
		NTSTATUS ttstatus;
		DWORD tttisDebuggerPresent = 0;
		HMODULE thNtDll = LoadLibrary(TEXT("ntdll.dll"));
		SetupMain();
		if (NULL != thNtDll)
		{
			NtQueryInformationProcess = (pfnNtQueryInformationProcess)GetProcAddress(thNtDll, "NtQueryInformationProcess");
			if (NULL != NtQueryInformationProcess)
			{
				ttstatus = NtQueryInformationProcess(
					GetCurrentProcess(),
					ProcessDebugPort,
					&tttisDebuggerPresent,
					sizeof(DWORD),
					NULL);
				if (ttstatus == 0x00000000 && tttisDebuggerPresent != 0)
				{
					std::cout << "Stop debugging program! 4" << std::endl;
					exit(-1);
				}
			}

		}
	}
	return 0;
}