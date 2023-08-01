#include <windows.h>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <string>

#include <minhook.h>
#pragma comment(lib, "libMinHook.x86.lib")

#include "util.h"
#include <vector>

struct FName
{
public:
    int32_t ComparisonIndex;    // 0x0000
    int32_t Number;             // 0x0004
};

struct UClass;

struct UObject
{
public:
    void**      VTable;         // 0x0000
    int32_t     ObjectFlags;    // 64 = 0x0008 | 32 = 0x0004
    int32_t     InternalIndex;  // 64 = 0x000C | 32 = 0x0008
    UClass*     ClassPrivate;   // 64 = 0x0010 | 32 = 0x000C (Missing: UClass)
    FName       Name;           // 64 = 0x0018 | 32 = 0x0010
    UObject*    OuterPrivate;   // 64 = 0x0020 | 32 = 0x0018
};

class FUObjectItem
{
public:
    UObject* Object;
    __int32 ClusterIndex;
    __int32 SerialNumber;
};

class TUObjectArray
{
public:
    FUObjectItem* Objects;
    int32_t MaxElements;
    int32_t NumElements;
};

class FUObjectArray
{
public:
    TUObjectArray ObjObjects;
};

FUObjectArray* GlobalObjects = nullptr;

template<class T>
struct TArray
{
    friend struct FString;

public:
    inline TArray()
    {
        Data = nullptr;

        Count = Max = 0;
    };

    inline int Num() const
    {
        return Count;
    };

    inline T& operator[](int i)
    {
        return Data[i];
    };

    inline bool IsValid(int i)
    {
        return i < Num();
    }

private:
    T* Data;

    int Count;
    int Max;
};

struct FString : private TArray<wchar_t>
{
    FString()
    {
        Data = nullptr;

        Max = Count = 0;
    }

    FString(const wchar_t* Value)
    {
        Max = Count = *Value ? std::wcslen(Value) + 1 : 0;

        if (Count)
        {
            Data = const_cast<wchar_t*>(Value);
        }
    };

    inline bool IsValid()
    {
        return Data != nullptr;
    }

    inline wchar_t* c_str()
    {
        return Data;
    }
};

typedef void (__cdecl* fFree_Internal)
(
    void* Buffer
);

static fFree_Internal Free_Internal;

typedef FString (__cdecl* fGetObjectName_Internal)
(
    UObject* Object
);

static fGetObjectName_Internal GetObjectName_Internal;

static std::wstring GetObjectName(UObject* Object)
{
    std::wstring sName(L"");

    for (auto i = 0; Object; Object = Object->OuterPrivate, ++i)
    {
        FString objName = GetObjectName_Internal(Object);

        if (objName.IsValid())
        {
            sName = objName.c_str() + std::wstring(i > 0 ? L"." : L"") + sName;

            Free_Internal(objName.c_str());
        }
    }

    return sName;
}

static std::wstring GetObjectFirstName(UObject* Object)
{
    std::wstring sName(L"");

    FString objName = GetObjectName_Internal(Object);

    if (objName.IsValid())
    {
        sName = objName.c_str();

        Free_Internal(objName.c_str());
    }

    return sName;
}

std::wstring GetObjectFullName(UObject* obj)
{
    std::wstring name;

    if (obj->ClassPrivate != nullptr)
    {
        name = GetObjectFirstName((UObject*)obj->ClassPrivate);
        name += L" ";
        name += GetObjectName(obj);
    }

    return name;
}

typedef void* (__fastcall* fProcessEvent)
(
        UObject*    _Object
    ,   UObject*    Object
    ,   UObject*    Function
    ,   void*       Params
);

fProcessEvent ProcessEvent;

static std::vector<std::wstring> cache;

void* __fastcall ProcessEventHook(UObject* _Object, UObject* Object, UObject* Function, void* Params)
{
    // fuck this game and fuck my life
    if (_Object && Function)
    {
        std::wstring sObjectName = GetObjectFirstName(_Object);
        std::wstring sFunctionName = GetObjectFirstName(Function);

        if (sFunctionName.find(L"ShowStoreChanged") != std::string::npos)
            return NULL;
        if (sFunctionName.find(L"PlayZoneChanged") != std::string::npos)
            return NULL;
        if (sFunctionName.find(L"ShowHeroListChanged") != std::string::npos)
            return NULL;
        if (sFunctionName.find(L"ShowVaultChanged") != std::string::npos)
            return NULL;
        if (sFunctionName.find(L"ShowDailyRewardsChanged") != std::string::npos)
            return NULL;
        if (sFunctionName.find(L"ShowHomeBaseChanged") != std::string::npos)
        {
            UObject* frontendController = NULL;
            UObject* funcFrontend = NULL;

            for (int i = 0; i < GlobalObjects->ObjObjects.NumElements; i++)
            {
                FUObjectItem* what = &GlobalObjects->ObjObjects.Objects[i];
                if (what && what->Object)
                {
                    if (GetObjectFullName(what->Object).find(L"FortPlayerControllerFrontEnd /Game/Maps/FortniteEntry.FortniteEntry.PersistentLevel.FortPlayerControllerFrontEnd_0") != std::string::npos)
                    {
                        frontendController = what->Object;

                        break;
                    }
                }
            }

            for (int i = 0; i < GlobalObjects->ObjObjects.NumElements; i++)
            {
                FUObjectItem* what = &GlobalObjects->ObjObjects.Objects[i];
                if (what && what->Object)
                {
                    if (GetObjectFullName(what->Object).find(L"Function /Script/FortniteGame.FortPlayerController.ClientRegisterWithParty") != std::string::npos)
                    {
                        funcFrontend = what->Object;

                        break;
                    }
                }
            }

            ProcessEvent(frontendController, frontendController, funcFrontend, nullptr);

            return NULL;
        }

        std::wstring sValue = L"Object = " + sObjectName + L", Function = " + sFunctionName;

        if (std::find(cache.begin(), cache.end(), sValue) == cache.end())
        {
            wprintf(L"%s\n", sValue.c_str());

            cache.push_back(sValue);
        }
    }

    return ProcessEvent(_Object, Object, Function, Params);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason,
    LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        Util::InitConsole();

        GlobalObjects = reinterpret_cast<decltype(GlobalObjects)>(Util::BaseAddress() + 0x2DC3AAC);

        auto pFree_InternalOffset = Util::FindPattern
        (
            "\xE8\x00\x00\x00\x00\xFF\x76\xB0",
            "x????xxx"
        );

        auto pFree_InternalAddress = pFree_InternalOffset + 5 + *reinterpret_cast<int32_t*>(pFree_InternalOffset + 1);

        if (!pFree_InternalAddress)
        {
            MessageBox(0, L"Invalid Free_Internal address, exiting...", L"Error", MB_ICONERROR);
            ExitProcess(EXIT_FAILURE);
        }

        Free_Internal = reinterpret_cast<fFree_Internal>(pFree_InternalAddress);

        auto pGetObjectName_InternalAddress = Util::FindPattern
        (
            "\x8B\x4C\x24\x08\x83\xEC\x08\x56\x8B\x74\x24\x10\x85\xC9\x75\x4E",
            "xxxxxxxxxxxxxxxx"
        );

        if (!pGetObjectName_InternalAddress)
        {
            MessageBox(0, L"Invalid GetObjectName_Internal address, exiting...", L"Error", MB_ICONERROR);
            ExitProcess(EXIT_FAILURE);
        }

        GetObjectName_Internal = reinterpret_cast<fGetObjectName_Internal>(pGetObjectName_InternalAddress);

        MH_Initialize();

        auto pProcessEventAddress = Util::BaseAddress() + 0x9A9E70;/*Util::FindPattern
        (
            "\x55\x8B\xEC\x81\xEC\x00\x00\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xFC\x56\x8B\xF1\x8B\x4D\x0C\x57\x8B\x7D\x08\x89\x75\x8C",
            "xxxxx????x????xxxxxxxxxxxxxxxxxx"
        );*/

        MH_CreateHook(reinterpret_cast<PVOID*>(pProcessEventAddress), ProcessEventHook, reinterpret_cast<PVOID*>(&ProcessEvent));
        MH_EnableHook(reinterpret_cast<PVOID*>(pProcessEventAddress));

        /*for (int i = 0; i < GlobalObjects->ObjObjects.NumElements; i++)
        {
            FUObjectItem* what = &GlobalObjects->ObjObjects.Objects[i];
            if (what && what->Object)
            {
                wprintf(L"%s\n", GetObjectFullName(what->Object).c_str());
            }
        }*/
    }

    return TRUE;
}

