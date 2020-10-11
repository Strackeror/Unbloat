// dllmain.cpp : Defines the entry point for the DLL application.
#include "Windows.h"
#include <Psapi.h>

#include <algorithm>
#include <vector>

#include "loader.h"

using namespace loader;

__declspec(dllexport) void load() {};

typedef unsigned char byte;

std::vector<byte*> scanmem(const std::vector<byte>& bytes)
{
    std::vector<byte*> results;
    auto module = GetModuleHandle("MonsterHunterWorld.exe");
    if (module == nullptr) return results;

    MODULEINFO moduleInfo;
    if (!GetModuleInformation(GetCurrentProcess(), module, &moduleInfo, sizeof(moduleInfo)))
        return results;

    byte* startAddr = (byte*) module;
    byte* endAddr = startAddr + moduleInfo.SizeOfImage;
    byte* addr = startAddr;

    while (addr < endAddr)
    {
        MEMORY_BASIC_INFORMATION memInfo;
        if (!VirtualQuery(addr, &memInfo, sizeof(memInfo)) || memInfo.State != MEM_COMMIT || (memInfo.Protect & PAGE_GUARD))
            continue;
        byte* begin = (byte*) memInfo.BaseAddress;
        byte* end = begin + memInfo.RegionSize;

        
        byte* found = std::search(begin, end, bytes.begin(), bytes.end());
        while (found != end) {
            results.push_back(found);
            found = std::search(found + 1, end, bytes.begin(), bytes.end());
        }

        addr = end;
        memInfo = {};
    }

    return results;
}

bool unprotect(byte* ptr, int len, PDWORD oldp) {
    return VirtualProtect((LPVOID)(ptr), len, PAGE_EXECUTE_READWRITE, oldp);
}
bool protect(byte* ptr, int len, PDWORD oldp) {
    DWORD dummy;
    return VirtualProtect((LPVOID)(ptr), len, *oldp, &dummy);
}


bool apply(std::vector<byte> search, std::vector<byte> replace)
{
    auto results = scanmem(search);
    if (results.size() != 1)
        return false;
    byte* found = results[0];
    DWORD protection;
	unprotect(found, replace.size(), &protection);
    memcpy(found, &replace[0], replace.size());
	protect(found, replace.size(), &protection);
    

    return true;
}


std::vector<byte> eleSearchBytes = { 0x8D, 0x04, 0x89, 0x03, 0xC0, 0xC3 };
std::vector<byte> eleReplaceBytes = { 0x48, 0x8B, 0xC1, 0x90, 0x90 };

std::vector<byte> rawSearchBytes = { 0xF3, 0x42, 0x0F, 0x59, 0x04, 0x81 };
std::vector<byte> rawReplaceBytes = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

std::vector<byte> awakenSearchBytes = { 0x48, 0x83, 0xec, 0x28, 0x8b, 0xc1, 0x83, 0xfa, 0x01, 0x75, 0x1f, 0x0f, 0x57, 0xc0, 0xf3, 0x48, 0x0f, 0x2a, 0xc0 };
std::vector<byte> awakenReplaceBytes = { 0x90, 0x90, 0x90, 0x90, 0x90 };


void onLoad()
{
    auto results = scanmem(awakenSearchBytes);
    if (results.size() == 1)
    {
        for (auto offs : { 0x20, 0x48, 0x58 }) {
            DWORD protection;
            unprotect(results[0] + offs, awakenReplaceBytes.size(), &protection);
			memcpy(results[0] + offs, &awakenReplaceBytes[0], awakenReplaceBytes.size());
            protect(results[0] + offs, awakenReplaceBytes.size(), &protection);
        }
    }
    else
    {
        LOG(ERR) << "Unbloat : awakenBloat failed " << results.size();
        for (auto ptr : results)
            LOG(ERR) << ptr;
    }

    if (!apply(eleSearchBytes, eleReplaceBytes))
        LOG(ERR) << "Unbloat : eleBloat failed";
    if (!apply(rawSearchBytes, rawReplaceBytes))
        LOG(ERR) << "Unbloat : rawBloat failed";
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        onLoad();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

