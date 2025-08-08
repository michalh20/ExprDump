#include "pch.h"
#include "Windows.h"
#include <thread>
#include <fstream> 
#include <Psapi.h> 

struct ExprFuncArg {
    int type;
    const char* name;
    char pad[0x18];
    const char* returnTypeName;
};

struct ExpressionFunction {
    int argCount;
    int exprCodeEnum;
    int* funcAddress;
    char pad[0x8];
    char* cmdName;
    char* comment;
    char* file;
    char unknown[0x8];
    ExprFuncArg args[12];
    int returnType; // 0x278 if equal to 0x81(129 in dec) use info from 0x290 or 0x2A0
    char pad2[0x20];
    char* returnTypeStr; // 0x2A0
    //0x2A8 - expr type
    //0x290 -  ?
    //0x2A0 - return type in text ?
};

struct ExprFuncDescContainer {
    ExpressionFunction** funcs;
};

uintptr_t moduleBase = (uintptr_t)GetModuleHandle(L"GameClient.exe");

uintptr_t FindAOBSignature(const BYTE* signature, const char* mask, size_t sigLength) {
    MODULEINFO moduleInfo = { 0 };
    GetModuleInformation(GetCurrentProcess(), (HMODULE)moduleBase, &moduleInfo, sizeof(MODULEINFO));

    uintptr_t baseAddress = (uintptr_t)moduleInfo.lpBaseOfDll;
    const uintptr_t endAddress = baseAddress + moduleInfo.SizeOfImage;

    for (uintptr_t address = baseAddress; address < endAddress - sigLength; address++) {
        bool found = true;

        for (size_t i = 0; i < sigLength; i++) {
            if (mask[i] != '?' && signature[i] != *(BYTE*)(address + i)) {
                found = false;
                break;
            }
        }

        if (found) {
            return address;
        }
    }

    return 0;
}

const BYTE exprGetAllFuncsSignature[] = { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x4C, 0x8D, 0x0D, 0x00, 0x00, 0x00, 0x00 };
const char exprGetAllFuncsMask[] = "xxxxxxxxx????";
uintptr_t exprGetAllFuncsAddress = FindAOBSignature(exprGetAllFuncsSignature, exprGetAllFuncsMask, sizeof(exprGetAllFuncsSignature));

typedef ExprFuncDescContainer* (__stdcall* setupexprGetAllFuncs)();
setupexprGetAllFuncs exprGetAllFuncs = reinterpret_cast<setupexprGetAllFuncs>(exprGetAllFuncsAddress);

std::string GetTypeNameFromId(int typeId)
{
    switch (typeId) {
    case 0:     return "void";
    case 2:     return "int";
    case 4:     return "float";
    case 5:     return "intarray";
    case 6:     return "floatarray";
    case 7:     return "Vec3";
    case 8:     return "Vec4";
    case 9:     return "Mat4";
    case 10:    return "Quat";
    case 11:    return "char*";
    case 12:    return "multivalarray";
    case 128:   return "entityarray";
    case 129:   return "object"; 
    case 130:   return "MultiVal";
    case 10249: return "loc";
    default:    return "undefined";
    }
}


DWORD WINAPI MainThread(HMODULE hModule) {
    ExprFuncDescContainer* container = exprGetAllFuncs();
    if (container)
    {
        void* adjustedAddress = (void*)((uintptr_t)container->funcs - 0x10); // size is 10 bytes above
        int funcCount = *reinterpret_cast<int*>(adjustedAddress);
        if (funcCount > 0) {
            std::ofstream File("Expressions.txt");
            if (File.is_open()) {
                for (int i = 0; i < funcCount; i++)
                {
                    ExpressionFunction* func_desc = container->funcs[i];
                    if (func_desc) {
                        std::string returnString;
                        switch (func_desc->returnType) {
                        case 0:
                            returnString = "void";
                            break;
                        case 2:
                            returnString = "int";
                            break;
                        case 4:
                            returnString = "float";
                            break;
                        case 5:
                            returnString = "intarray";
                            break;
                        case 6:
                            returnString = "floatarray";
                            break;
                        case 7:
                            returnString = "Vec3";
                            break;
                        case 8:
                            returnString = "Vec4";
                            break;
                        case 9:
                            returnString = "Mat4";
                            break;
                        case 10:
                            returnString = "Quat";
                            break;
                        case 11:
                            returnString = "char*";
                            break;
                        case 12:
                            returnString = "multivalarray";
                            break;
                        case 128:
                            returnString = "entityarray";
                            break;
                        case 129:
                            returnString = func_desc->returnTypeStr;
                            break;
                        case 130:
                            returnString = "MultiVal";
                            break;
                        case 10249:
                            returnString = "loc";
                            break;
                        default:
                            returnString = "undefined";
                            break;
                        }
                        if (func_desc->argCount == 0) {
                            File << "[" << func_desc->funcAddress << "] "
                                << returnString << " "
                                << func_desc->cmdName << "()\n";
                        }
                        else {
                            std::string arguments;
                            for (int i = 0; i < func_desc->argCount; i++) {
                                int argType = func_desc->args[i].type;
                                const char* name = func_desc->args[i].name;
                                if (argType == 129) {
                                    arguments += func_desc->args[i].returnTypeName;
                                }
                                else {
                                    arguments += GetTypeNameFromId(argType);
                                }
                                arguments += " ";
                                arguments += name;

                                if (i < func_desc->argCount - 1) {
                                    arguments += ", ";
                                }
                            }
                            File << "[" << func_desc->funcAddress << "] "
                                << returnString << " "
                                << func_desc->cmdName << "(" << arguments << ")\n";
                        }
                    }
                }
                File.close();
            }
        }
    }
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CloseHandle(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, nullptr));
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

