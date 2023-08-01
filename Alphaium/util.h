#ifndef UTIL_H
#define UTIL_H

#include <windows.h>
#include <inttypes.h>
#include <psapi.h>
#include <stdio.h>

class Util
{
private:
    static BOOL MaskCompare(PVOID pBuffer, LPCSTR lpPattern, LPCSTR lpMask)
    {
        for (auto value = static_cast<PBYTE>(pBuffer); *lpMask; ++lpPattern, ++lpMask, ++value)
        {
            if (*lpMask == 'x' && *reinterpret_cast<LPCBYTE>(lpPattern) != *value)
            {
                return false;
            }
        }

        return true;
    }

public:
    static VOID InitConsole()
    {
        AllocConsole();

        FILE* pFile;
        freopen_s(&pFile, "CONOUT$", "w", stdout);
    }

    static uintptr_t BaseAddress()
    {
        return reinterpret_cast<uintptr_t>(GetModuleHandle(0));
    }

    static PBYTE FindPattern(PVOID pBase, DWORD dwSize, LPCSTR lpPattern, LPCSTR lpMask)
    {
        dwSize -= static_cast<DWORD>(strlen(lpMask));

        for (auto i = 0UL; i < dwSize; ++i)
        {
            auto pAddress = static_cast<PBYTE>(pBase) + i;

            if (MaskCompare(pAddress, lpPattern, lpMask))
            {
                return pAddress;
            }
        }

        return NULL;
    }

    static PBYTE FindPattern(LPCSTR lpPattern, LPCSTR lpMask)
    {
        MODULEINFO info{};
        GetModuleInformation(GetCurrentProcess(), GetModuleHandle(0), &info, sizeof(info));

        return FindPattern(info.lpBaseOfDll, info.SizeOfImage, lpPattern, lpMask);
    }
};

#endif // UTIL_H
