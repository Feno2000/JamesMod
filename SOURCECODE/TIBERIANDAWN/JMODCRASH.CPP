/*
** JMODCRASH.CPP -- JamesMod: crash logger.
**
** The game's ClientLauncher attaches to InstanceServerG.exe as a debugger, so Visual Studio
** can't. A vectored exception handler still sees each exception (first chance) before
** the launcher reports the crash, so we log what we can to JAMESMOD_CRASH.LOG:
**   - exception code, faulting address, registers
**   - the EBP frame chain
**   - a stack scan for return addresses inside TiberianDawn.dll (works even when EIP
**     itself is garbage, e.g. a call through a deleted object)
** Addresses inside our DLL are printed as offsets so they can be looked up in the PDB.
*/
#include "function.h"
#include "JMOD.H"
#include <stdio.h>

static char CrashPath[MAX_PATH];
static DWORD ModBase;
static DWORD ModEnd;
static volatile LONG CrashCount;
static PVOID HandlerHandle;

/*
** Unregister the handler when the DLL unloads (global destructors run at DLL_PROCESS_DETACH).
*/
static struct HandlerRemover {
	~HandlerRemover() { if (HandlerHandle) { RemoveVectoredExceptionHandler(HandlerHandle); HandlerHandle = NULL; } }
} TheHandlerRemover;

static void Describe(FILE * f, const char * label, DWORD addr)
{
	HMODULE mod = NULL;
	char name[MAX_PATH] = "?";
	if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)addr, &mod) && mod) {
		GetModuleFileNameA(mod, name, MAX_PATH);
		char * base = strrchr(name, '\\');
		fprintf(f, "  %-6s %08X  %s+0x%X\n", label, addr, base ? base + 1 : name, addr - (DWORD)mod);
	} else {
		fprintf(f, "  %-6s %08X  (not in a module)\n", label, addr);
	}
}

static LONG CALLBACK Crash_Handler(PEXCEPTION_POINTERS ep)
{
	DWORD code = ep->ExceptionRecord->ExceptionCode;
	if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION &&
		code != EXCEPTION_PRIV_INSTRUCTION && code != EXCEPTION_INT_DIVIDE_BY_ZERO &&
		code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_ARRAY_BOUNDS_EXCEEDED) {
		return EXCEPTION_CONTINUE_SEARCH;
	}
	if (InterlockedIncrement(&CrashCount) > 5) return EXCEPTION_CONTINUE_SEARCH;

	FILE * f = fopen(CrashPath, "a");
	if (f == NULL) return EXCEPTION_CONTINUE_SEARCH;

	SYSTEMTIME t;
	GetLocalTime(&t);
	CONTEXT * c = ep->ContextRecord;
	fprintf(f, "==== %04d-%02d-%02d %02d:%02d:%02d  exception %08X at %08X  (game frame %ld)\n",
		t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, code, (DWORD)ep->ExceptionRecord->ExceptionAddress, Frame);
	if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
		fprintf(f, "  %s address %08X\n", ep->ExceptionRecord->ExceptionInformation[0] ? "write to" : "read from",
			(DWORD)ep->ExceptionRecord->ExceptionInformation[1]);
	}
	fprintf(f, "  EAX=%08X EBX=%08X ECX=%08X EDX=%08X ESI=%08X EDI=%08X EBP=%08X ESP=%08X\n",
		c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
	fprintf(f, "  TiberianDawn.dll loaded at %08X-%08X\n", ModBase, ModEnd);
	Describe(f, "EIP", c->Eip);

	fprintf(f, " frame chain:\n");
	__try {
		DWORD * bp = (DWORD *)c->Ebp;
		for (int i = 0; i < 40 && bp != NULL && ((DWORD)bp & 3) == 0; i++) {
			if ((DWORD)bp < c->Esp || (DWORD)bp > c->Esp + 0x100000) break;
			Describe(f, "ret", bp[1]);
			DWORD * next = (DWORD *)bp[0];
			if (next <= bp) break;
			bp = next;
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		fprintf(f, "  (frame chain unreadable)\n");
	}

	fprintf(f, " stack scan (TiberianDawn.dll return addresses):\n");
	__try {
		DWORD * sp = (DWORD *)c->Esp;
		int found = 0;
		for (int i = 0; i < 8192 && found < 48; i++) {
			DWORD v = sp[i];
			if (v >= ModBase && v < ModEnd) {
				fprintf(f, "  [esp+%04X] TiberianDawn.dll+0x%X\n", i * 4, v - ModBase);
				found++;
			}
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		fprintf(f, "  (end of readable stack)\n");
	}
	fprintf(f, "\n");
	fclose(f);
	return EXCEPTION_CONTINUE_SEARCH;
}

void JMod_Install_Crash_Logger(const char * root)
{
	static bool installed = false;
	if (installed) return;
	installed = true;

	_snprintf(CrashPath, MAX_PATH, "%s\\JAMESMOD_CRASH.LOG", root);
	CrashPath[MAX_PATH-1] = '\0';

	HMODULE self = NULL;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCSTR)&JMod_Install_Crash_Logger, &self);
	ModBase = (DWORD)self;
	IMAGE_DOS_HEADER * dos = (IMAGE_DOS_HEADER *)self;
	IMAGE_NT_HEADERS * nt = (IMAGE_NT_HEADERS *)((char *)self + dos->e_lfanew);
	ModEnd = ModBase + nt->OptionalHeader.SizeOfImage;

	HandlerHandle = AddVectoredExceptionHandler(1, Crash_Handler);
}
