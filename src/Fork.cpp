#include "Fork.h"
//Statt fork zu umgehen versuche ich hier eine implementierung für Windows


typedef struct _CLIENT_ID {
	PVOID UniqueProcess;
	PVOID UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

typedef struct _SECTION_IMAGE_INFORMATION {
	PVOID EntryPoint;
	ULONG StackZeroBits;
	ULONG StackReserved;
	ULONG StackCommit;
	ULONG ImageSubsystem;
	WORD SubSystemVersionLow;
	WORD SubSystemVersionHigh;
	ULONG Unknown1;
	ULONG ImageCharacteristics;
	ULONG ImageMachineType;
	ULONG Unknown2[3];
} SECTION_IMAGE_INFORMATION, *PSECTION_IMAGE_INFORMATION;

typedef struct _RTL_USER_PROCESS_INFORMATION {
	ULONG Size;
	HANDLE Process;
	HANDLE Thread;
	CLIENT_ID ClientId;
	SECTION_IMAGE_INFORMATION ImageInformation;
} RTL_USER_PROCESS_INFORMATION, *PRTL_USER_PROCESS_INFORMATION;

#define RTL_CLONE_PROCESS_FLAGS_CREATE_SUSPENDED	0x00000001
#define RTL_CLONE_PROCESS_FLAGS_INHERIT_HANDLES		0x00000002
#define RTL_CLONE_PROCESS_FLAGS_NO_SYNCHRONIZE		0x00000004

#define RTL_CLONE_PARENT				0
#define RTL_CLONE_CHILD					297

typedef DWORD pid_t;

typedef NTSTATUS(*RtlCloneUserProcess_f)(ULONG ProcessFlags,
	PSECURITY_DESCRIPTOR ProcessSecurityDescriptor /* optional */,
	PSECURITY_DESCRIPTOR ThreadSecurityDescriptor /* optional */,
	HANDLE DebugPort /* optional */,
	PRTL_USER_PROCESS_INFORMATION ProcessInformation);



DWORD fork()
{
	HMODULE mod;
	RtlCloneUserProcess_f clone_p;
	RTL_USER_PROCESS_INFORMATION process_info;
	NTSTATUS result;

	mod = GetModuleHandle("ntdll.dll");
	if (!mod)
		return -ENOSYS;

	clone_p = (RtlCloneUserProcess_f)GetProcAddress(mod, "RtlCloneUserProcess");
	if (clone_p == NULL)
		return -ENOSYS;

	/* lets do this */
	result = clone_p(RTL_CLONE_PROCESS_FLAGS_CREATE_SUSPENDED | RTL_CLONE_PROCESS_FLAGS_INHERIT_HANDLES, NULL, NULL, NULL, &process_info);

	if (result == RTL_CLONE_PARENT)
	{
		HANDLE me, hp, ht, hcp = 0;
		DWORD pi, ti, mi;
		me = GetCurrentProcess();
		pi = (DWORD)process_info.ClientId.UniqueProcess;
		ti = (DWORD)process_info.ClientId.UniqueThread;

		assert(hp = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pi));
		assert(ht = OpenThread(THREAD_ALL_ACCESS, FALSE, ti));

		ResumeThread(ht);
		CloseHandle(ht);
		CloseHandle(hp);
		return (pid_t)pi;
	}
	else if (result == RTL_CLONE_CHILD)
	{
		/* fix stdio */
		AllocConsole();
		return 0;
	}
	else
		return -1;

	/* NOTREACHED */
	return -1;

}


