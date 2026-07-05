#include "pch.h"
#include "shader_inspect.h"

HMODULE g_hCurrentModule = nullptr;

#define REQUIRE_HLSL2021 1


struct IDxcBlob : public IUnknown {
    virtual LPVOID  STDMETHODCALLTYPE GetBufferPointer(void) = 0;
    virtual SIZE_T  STDMETHODCALLTYPE GetBufferSize(void) = 0;
};

struct IDxcBlobEncoding : public IDxcBlob {
    virtual HRESULT STDMETHODCALLTYPE GetEncoding(_Out_ BOOL* pKnown, _Out_ UINT32* pCodePage) = 0;
};

struct IDxcBlobUtf16 : public IDxcBlobEncoding {
    virtual LPCWSTR STDMETHODCALLTYPE GetStringPointer(void) = 0;
    virtual SIZE_T  STDMETHODCALLTYPE GetStringLength(void) = 0;
};

struct IDxcBlobUtf8 : public IDxcBlobEncoding {
    virtual LPCSTR  STDMETHODCALLTYPE GetStringPointer(void) = 0;
    virtual SIZE_T  STDMETHODCALLTYPE GetStringLength(void) = 0;
};

struct IDxcIncludeHandler : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE LoadSource(_In_z_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource) = 0;
};

typedef struct DxcBuffer {
    LPCVOID Ptr;
    SIZE_T  Size;
    UINT    Encoding;
} DxcText;

struct DxcDefine {
    LPCWSTR Name;
    _Maybenull_ LPCWSTR Value;
};

struct IDxcOperationResult : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetStatus(_Out_ HRESULT* pStatus) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetResult(_COM_Outptr_result_maybenull_ IDxcBlob** ppResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetErrorBuffer(_COM_Outptr_result_maybenull_ IDxcBlobEncoding** ppErrors) = 0;
};

struct IDxcCompiler : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Compile(
        _In_ IDxcBlob* pSource, _In_opt_z_ LPCWSTR pSourceName,
        _In_opt_z_ LPCWSTR pEntryPoint, _In_z_ LPCWSTR pTargetProfile,
        _In_opt_count_(argCount) LPCWSTR* pArguments, _In_ UINT32 argCount,
        _In_count_(defineCount) const DxcDefine* pDefines, _In_ UINT32 defineCount,
        _In_opt_ IDxcIncludeHandler* pIncludeHandler,
        _COM_Outptr_ IDxcOperationResult** ppResult) = 0;

    virtual HRESULT STDMETHODCALLTYPE Preprocess(
        _In_ IDxcBlob* pSource, _In_opt_z_ LPCWSTR pSourceName,
        _In_opt_count_(argCount) LPCWSTR* pArguments, _In_ UINT32 argCount,
        _In_count_(defineCount) const DxcDefine* pDefines, _In_ UINT32 defineCount,
        _In_opt_ IDxcIncludeHandler* pIncludeHandler,
        _COM_Outptr_ IDxcOperationResult** ppResult) = 0;

    virtual HRESULT STDMETHODCALLTYPE Disassemble(
        _In_ IDxcBlob* pSource, _COM_Outptr_ IDxcBlobEncoding** ppDisassembly) = 0;
};

struct IDxcCompiler2 : public IDxcCompiler {
    virtual HRESULT STDMETHODCALLTYPE CompileWithDebug(
        _In_ IDxcBlob* pSource, _In_opt_z_ LPCWSTR pSourceName,
        _In_opt_z_ LPCWSTR pEntryPoint, _In_z_ LPCWSTR pTargetProfile,
        _In_opt_count_(argCount) LPCWSTR* pArguments, _In_ UINT32 argCount,
        _In_count_(defineCount) const DxcDefine* pDefines, _In_ UINT32 defineCount,
        _In_opt_ IDxcIncludeHandler* pIncludeHandler,
        _COM_Outptr_ IDxcOperationResult** ppResult,
        _Outptr_opt_result_z_ LPWSTR* ppDebugBlobName,
        _COM_Outptr_opt_ IDxcBlob** ppDebugBlob) = 0;
};

struct IDxcCompiler3 : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Compile(
        _In_ const DxcBuffer* pSource,
        _In_opt_count_(argCount) LPCWSTR* pArguments, _In_ UINT32 argCount,
        _In_opt_ IDxcIncludeHandler* pIncludeHandler,
        _In_ REFIID riid, _Out_ LPVOID* ppResult) = 0;

    virtual HRESULT STDMETHODCALLTYPE Disassemble(
        _In_ const DxcBuffer* pObject,
        _In_ REFIID riid, _Out_ LPVOID* ppResult) = 0;
};

static const CLSID CLSID_DxcCompiler = {
    0x73e22d93, 0xe6ce, 0x47f3, {0xb5, 0xbf, 0xf0, 0x66, 0x4f, 0x39, 0xc1, 0xb0}
};
static const IID IID_IDxcCompiler = {
    0x8c210bf3, 0x011f, 0x4422, {0x8d, 0x70, 0x6f, 0x9a, 0xcb, 0x8d, 0xb6, 0x17}
};
static const IID IID_IDxcCompiler2 = {
    0xA005A9D9, 0xB8BB, 0x4594, {0xB5, 0xC9, 0x0E, 0x63, 0x3B, 0xEC, 0x4D, 0x37}
};
static const IID IID_IDxcCompiler3 = {
    0x228B4687, 0x5A6A, 0x4730, {0x90, 0x0C, 0x97, 0x02, 0xB2, 0x20, 0x3F, 0x54}
};

typedef HRESULT(__stdcall* PFN_DxcCreateInstance)(REFCLSID rclsid, REFIID riid, LPVOID* ppv);
typedef HRESULT(__stdcall* PFN_DxcCreateInstance2)(IMalloc* pMalloc, REFCLSID rclsid, REFIID riid, LPVOID* ppv);

static HMODULE                  g_hOrigDll = nullptr;
static PFN_DxcCreateInstance    g_OrigDxcCreateInstance = nullptr;
static PFN_DxcCreateInstance2   g_OrigDxcCreateInstance2 = nullptr;

static bool LoadOriginalDLL()
{
    if (g_hOrigDll) return true;

    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW(g_hCurrentModule, dllPath, MAX_PATH);
    std::wstring dir(dllPath);
    dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);
    std::wstring origPath = dir + L"dxcompiler_orig.dll";

    g_hOrigDll = LoadLibraryW(origPath.c_str());
    if (!g_hOrigDll)
    {
        OutputDebugStringW((L"[dxcompiler] ERROR: Failed to load dxcompiler_orig.dll from: " + origPath + L"\n").c_str());
        return false;
    }

    g_OrigDxcCreateInstance  = (PFN_DxcCreateInstance) GetProcAddress(g_hOrigDll, "DxcCreateInstance");
    g_OrigDxcCreateInstance2 = (PFN_DxcCreateInstance2)GetProcAddress(g_hOrigDll, "DxcCreateInstance2");

    if (!g_OrigDxcCreateInstance || !g_OrigDxcCreateInstance2)
    {
        OutputDebugStringW(L"[dxcompiler] ERROR: Failed to get DxcCreateInstance proc from dxcompiler_orig.dll\n");
        return false;
    }

    OutputDebugStringW(L"[dxcompiler] Loaded dxcompiler_orig.dll successfully\n");

    ShaderInspector::Instance().Initialize(g_hOrigDll);

    return true;
}


typedef HRESULT(STDMETHODCALLTYPE* PFN_Compiler_Compile)(
    IDxcCompiler* self,
    IDxcBlob* pSource, LPCWSTR pSourceName,
    LPCWSTR pEntryPoint, LPCWSTR pTargetProfile,
    LPCWSTR* pArguments, UINT32 argCount,
    const DxcDefine* pDefines, UINT32 defineCount,
    IDxcIncludeHandler* pIncludeHandler,
    IDxcOperationResult** ppResult);

typedef HRESULT(STDMETHODCALLTYPE* PFN_Compiler3_Compile)(
    IDxcCompiler3* self,
    const DxcBuffer* pSource,
    LPCWSTR* pArguments, UINT32 argCount,
    IDxcIncludeHandler* pIncludeHandler,
    REFIID riid, LPVOID* ppResult);

static std::mutex g_HookMutex;
static bool       g_bHooksInstalled = false;

static PFN_Compiler_Compile   g_OrigCompiler_Compile = nullptr;
static PFN_Compiler3_Compile  g_OrigCompiler3_Compile = nullptr;

static std::vector<IUnknown*> g_HookedObjects;
static std::recursive_mutex g_HookedObjectsMutex;

#ifdef DEBUG_UNITY_INPUT

static void EnsureDumpDirectory(const std::wstring& dir)
{
    std::wstring path;
    for (size_t i = 0; i < dir.size(); ++i)
    {
        if (dir[i] == L'\\' || dir[i] == L'/')
        {
            if (!path.empty())
                CreateDirectoryW(path.c_str(), nullptr);
        }
        path += dir[i];
    }
    if (!path.empty())
        CreateDirectoryW(path.c_str(), nullptr);
}

static std::wstring GetDumpBaseDir()
{
    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW(g_hCurrentModule, dllPath, MAX_PATH);
    std::wstring dir(dllPath);
    dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);
    dir += L"ShaderDump\\";
    return dir;
}

static void WriteSourceFile(const std::wstring& filepath, const void* data, size_t len)
{
    std::ofstream ofs(filepath, std::ios::binary);
    if (ofs.is_open()) { ofs.write((const char*)data, len); ofs.close(); }
}

static void WriteArgsFile(const std::wstring& filepath, const std::wstring& content)
{
    std::ofstream ofs(filepath, std::ios::binary);
    if (ofs.is_open())
    {
        std::string utf8(content.begin(), content.end());
        ofs.write(utf8.c_str(), utf8.size());
        ofs.close();
    }
}

static void DumpCompilerCompile(
    IDxcBlob* pSource, LPCWSTR pSourceName,
    LPCWSTR pEntryPoint, LPCWSTR pTargetProfile,
    LPCWSTR* pArguments, UINT32 argCount,
    const DxcDefine* pDefines, UINT32 defineCount)
{
    DWORD pid = GetCurrentProcessId();
    SYSTEMTIME st; GetLocalTime(&st);

    wchar_t folder[128];
    swprintf_s(folder, L"PID_%u", pid);

    wchar_t timestamp[64];
    swprintf_s(timestamp, L"%04d%02d%02d_%02d%02d%02d_%03d",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    std::wstring baseDir = GetDumpBaseDir() + folder + L"\\";
    std::wstring srcName = (pSourceName && pSourceName[0]) ? pSourceName : L"unknown";
    for (auto& c : srcName) { if (c == L'\\' || c == L'/' || c == L':') c = L'_'; }

    std::wstring fileBase = baseDir + srcName + L"_" + timestamp;

    if (pSource)
    {
        void* pData = pSource->GetBufferPointer();
        SIZE_T size = pSource->GetBufferSize();
        if (pData && size > 0)
        {
            std::wstring srcFile = fileBase + L".hlsl";
            EnsureDumpDirectory(baseDir);
            WriteSourceFile(srcFile, pData, (size_t)size);
        }
    }

    std::wstringstream args;
    if (pEntryPoint && pEntryPoint[0])  args << L"-E " << pEntryPoint << L"\n";
    if (pTargetProfile && pTargetProfile[0]) args << L"-T " << pTargetProfile << L"\n";
    for (UINT32 i = 0; i < defineCount && pDefines; ++i)
    {
        args << L"-D " << pDefines[i].Name;
        if (pDefines[i].Value && pDefines[i].Value[0])
            args << L"=" << pDefines[i].Value;
        args << L"\n";
    }
    for (UINT32 i = 0; i < argCount && pArguments; ++i)
    {
        if (pArguments[i])
            args << pArguments[i] << L"\n";
    }

    std::wstring argsFile = fileBase + L"_args.txt";
    WriteArgsFile(argsFile, args.str());
    OutputDebugStringW((L"[dxcompiler] IDxcCompiler::Compile dumped: " + fileBase + L"\n").c_str());
}

static void DumpCompiler3Compile(
    const DxcBuffer* pSource,
    LPCWSTR* pArguments, UINT32 argCount,
    LPCWSTR pEntryPoint, LPCWSTR pTargetProfile,
    const DxcDefine* pDefines, UINT32 defineCount)
{
    DWORD pid = GetCurrentProcessId();
    SYSTEMTIME st; GetLocalTime(&st);

    wchar_t folder[128];
    swprintf_s(folder, L"PID_%u", pid);

    wchar_t timestamp[64];
    swprintf_s(timestamp, L"%04d%02d%02d_%02d%02d%02d_%03d",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    std::wstring baseDir = GetDumpBaseDir() + folder + L"\\";
    std::wstring srcName = L"unknown";
    for (UINT32 i = 0; i < argCount && pArguments; ++i)
    {
        if (pArguments[i] && wcscmp(pArguments[i], L"-E") == 0 && i + 1 < argCount)
            pEntryPoint = pArguments[i + 1];
        if (pArguments[i] && wcscmp(pArguments[i], L"-T") == 0 && i + 1 < argCount)
            pTargetProfile = pArguments[i + 1];
    }
    if (pEntryPoint && pEntryPoint[0])
        srcName = pEntryPoint;
    for (auto& c : srcName) { if (c == L'\\' || c == L'/' || c == L':') c = L'_'; }

    std::wstring fileBase = baseDir + srcName + L"_" + timestamp;

    if (pSource && pSource->Ptr && pSource->Size > 0)
    {
        std::wstring srcFile = fileBase + L".hlsl";
        EnsureDumpDirectory(baseDir);
        WriteSourceFile(srcFile, pSource->Ptr, (size_t)pSource->Size);
    }

    std::wstringstream args;
    if (pEntryPoint && pEntryPoint[0])  args << L"-E " << pEntryPoint << L"\n";
    if (pTargetProfile && pTargetProfile[0]) args << L"-T " << pTargetProfile << L"\n";
    for (UINT32 i = 0; i < argCount && pArguments; ++i)
    {
        if (pArguments[i])
            args << pArguments[i] << L"\n";
    }

    std::wstring argsFile = fileBase + L"_args.txt";
    WriteArgsFile(argsFile, args.str());
    OutputDebugStringW((L"[dxcompiler] IDxcCompiler3::Compile dumped: " + fileBase + L"\n").c_str());
}

#endif // DEBUG_UNITY_INPUT

static HRESULT STDMETHODCALLTYPE Hooked_Compiler_Compile(
    IDxcCompiler* self,
    IDxcBlob* pSource, LPCWSTR pSourceName,
    LPCWSTR pEntryPoint, LPCWSTR pTargetProfile,
    LPCWSTR* pArguments, UINT32 argCount,
    const DxcDefine* pDefines, UINT32 defineCount,
    IDxcIncludeHandler* pIncludeHandler,
    IDxcOperationResult** ppResult)
{
    std::wstring newProfile;
    SMVersion currentSM = SMVersion::FromTargetProfile(pTargetProfile);

    if (pSource)
    {
        void*  pData = pSource->GetBufferPointer();
        SIZE_T size  = pSource->GetBufferSize();

        SMVersion requiredSM = ShaderInspector::Instance().Inspect(pData, (size_t)size, 0);

        if (ShaderInspector::Instance().NeedsUpgrade(currentSM, requiredSM))
        {
            newProfile = requiredSM.ToTargetProfile(pTargetProfile);
            pTargetProfile = newProfile.c_str();

            char dbg[256];
            sprintf_s(dbg, "[dxcompiler] IDxcCompiler SM upgrade: %d.%d -> %d.%d (%ls)\n",
                currentSM.major, currentSM.minor,
                requiredSM.major, requiredSM.minor, newProfile.c_str());
            OutputDebugStringA(dbg);
        }
    }

#ifdef DEBUG_UNITY_INPUT
    DumpCompilerCompile(pSource, pSourceName, pEntryPoint, pTargetProfile,
        pArguments, argCount, pDefines, defineCount);
#endif

    return g_OrigCompiler_Compile(self, pSource, pSourceName,
        pEntryPoint, pTargetProfile, pArguments, argCount,
        pDefines, defineCount, pIncludeHandler, ppResult);
}

static HRESULT STDMETHODCALLTYPE Hooked_Compiler3_Compile(
    IDxcCompiler3* self,
    const DxcBuffer* pSource,
    LPCWSTR* pArguments, UINT32 argCount,
    IDxcIncludeHandler* pIncludeHandler,
    REFIID riid, LPVOID* ppResult)
{
    std::wstring newTarget;
    std::vector<LPCWSTR> modifiedArgs;

    LPCWSTR* finalArgs    = const_cast<LPCWSTR*>(pArguments);
    UINT32   finalArgCount = argCount;

    LPCWSTR currentTarget = nullptr;
    UINT32  targetIndex   = 0;
    for (UINT32 i = 0; i + 1 < argCount && pArguments; ++i)
    {
        if (pArguments[i] && wcscmp(pArguments[i], L"-T") == 0)
        {
            currentTarget = pArguments[i + 1];
            targetIndex   = i + 1;
            break;
        }
    }

    SMVersion requiredSM = ShaderInspector::Instance().Inspect(
        pSource->Ptr, (size_t)pSource->Size, pSource->Encoding);

    if (currentTarget && requiredSM.IsValid())
    {
        SMVersion currentSM = SMVersion::FromTargetProfile(currentTarget);
        SMVersion finalSM = currentSM;

        modifiedArgs.assign(pArguments, pArguments + argCount);
        bool argsModified = false;

        bool isCompute = (wcsncmp(currentTarget, L"cs_", 3) == 0);

        if (currentSM.major < 6 && requiredSM.major >= 6)
        {
            if (isCompute)
            {
                newTarget = requiredSM.ToTargetProfile(currentTarget);
                modifiedArgs[targetIndex] = newTarget.c_str();
                finalSM = requiredSM;

                modifiedArgs.push_back(L"-D");
                modifiedArgs.push_back(L"UNITY_SM6_SELECT(fallback,...)=(__VA_ARGS__)");

                argsModified = true;

                char dbg[256];
                sprintf_s(dbg, "[dxcompiler] Compute SM upgrade: %d.%d -> %d.%d (%ls -> %ls)\n",
                    currentSM.major, currentSM.minor, requiredSM.major, requiredSM.minor,
                    currentTarget, newTarget.c_str());
                OutputDebugStringA(dbg);
            }
            else
            {
                modifiedArgs.push_back(L"-D");
                modifiedArgs.push_back(L"UNITY_SM6_SELECT(fallback,...)=(fallback)");

                argsModified = true;

                char dbg[256];
                sprintf_s(dbg, "[dxcompiler] Graphics DX11 Fallback injected for SM %d.%d requirement (%ls)\n",
                    requiredSM.major, requiredSM.minor, currentTarget);
                OutputDebugStringA(dbg);
            }
        }
        else
        {
            modifiedArgs.push_back(L"-D");
            modifiedArgs.push_back(L"UNITY_SM6_SELECT(fallback,...)=(__VA_ARGS__)");
            argsModified = true;

            if (ShaderInspector::Instance().NeedsUpgrade(currentSM, requiredSM))
            {
                newTarget = requiredSM.ToTargetProfile(currentTarget);
                modifiedArgs[targetIndex] = newTarget.c_str();
                finalSM = requiredSM;

                char dbg[256];
                sprintf_s(dbg, "[dxcompiler] Minor SM upgrade: %d.%d -> %d.%d (%ls -> %ls)\n",
                    currentSM.major, currentSM.minor, requiredSM.major, requiredSM.minor,
                    currentTarget, newTarget.c_str());
                OutputDebugStringA(dbg);
            }
        }

        if (finalSM.major == 6 && finalSM.minor >= 2)
        {
            modifiedArgs.push_back(L"-enable-16bit-types");
            argsModified = true;
#if REQUIRE_HLSL2021
            modifiedArgs.push_back(L"-HV");
            modifiedArgs.push_back(L"2021");

            char dbg[256];
            sprintf_s(dbg, "[dxcompiler] SM 6.2+ Detected. Injected 16-bit types and HLSL 2021\n");
            OutputDebugStringA(dbg);
#else
            char dbg[256];
            sprintf_s(dbg, "[dxcompiler] SM 6.2+ Detected. Injected 16-bit types\n");
            OutputDebugStringA(dbg);
#endif
        }

        if (argsModified)
        {
            finalArgs = modifiedArgs.data();
            finalArgCount = (UINT32)modifiedArgs.size();
        }
    }

#ifdef DEBUG_UNITY_INPUT
    DumpCompiler3Compile(pSource, finalArgs, finalArgCount,
        nullptr, nullptr, nullptr, 0);
#endif

    return g_OrigCompiler3_Compile(self, pSource, finalArgs, finalArgCount,
        pIncludeHandler, riid, ppResult);
}


static bool IsAlreadyHooked(IUnknown* pObj)
{
    std::lock_guard<std::recursive_mutex> lock(g_HookedObjectsMutex);
    return std::find(g_HookedObjects.begin(), g_HookedObjects.end(), pObj) != g_HookedObjects.end();
}

static void MarkHooked(IUnknown* pObj)
{
    std::lock_guard<std::recursive_mutex> lock(g_HookedObjectsMutex);
    if (!IsAlreadyHooked(pObj))
        g_HookedObjects.push_back(pObj);
}

static void TryInstallVTableHooks(IUnknown* pUnknown)
{
    std::lock_guard<std::mutex> lock(g_HookMutex);

    void** vtable = *(void***)pUnknown;

    {
        IDxcCompiler3* pCompiler3 = nullptr;
        HRESULT hr = pUnknown->QueryInterface(IID_IDxcCompiler3, (void**)&pCompiler3);
        if (SUCCEEDED(hr) && pCompiler3)
        {
            if (IsAlreadyHooked(pCompiler3))
            {
                pCompiler3->Release();
                OutputDebugStringW(L"[dxcompiler] IDxcCompiler3 already hooked, skip\n");
                return;
            }

            void* targetFunc = vtable[3];
            if (!g_OrigCompiler3_Compile)
            {
                MH_STATUS mh = MH_CreateHook(targetFunc, (LPVOID)&Hooked_Compiler3_Compile, (LPVOID*)&g_OrigCompiler3_Compile);
                if (mh != MH_OK)
                {
                    char dbg[256];
                    sprintf_s(dbg, "[dxcompiler] MH_CreateHook Compiler3::Compile failed: %s\n", MH_StatusToString(mh));
                    OutputDebugStringA(dbg);
                    pCompiler3->Release();
                    return;
                }
                MH_EnableHook(targetFunc);
            }

            MarkHooked(pCompiler3);
            pCompiler3->Release();
            OutputDebugStringW(L"[dxcompiler] IDxcCompiler3::Compile HOOKED\n");
            return;
        }
    }

    {
        IDxcCompiler2* pCompiler2 = nullptr;
        HRESULT hr = pUnknown->QueryInterface(IID_IDxcCompiler2, (void**)&pCompiler2);
        if (SUCCEEDED(hr) && pCompiler2)
        {
            if (IsAlreadyHooked(pCompiler2))
            {
                pCompiler2->Release();
                OutputDebugStringW(L"[dxcompiler] IDxcCompiler2 already hooked, skip\n");
                return;
            }

            void* targetFunc = vtable[3];
            if (!g_OrigCompiler_Compile)
            {
                MH_STATUS mh = MH_CreateHook(targetFunc, (LPVOID)&Hooked_Compiler_Compile, (LPVOID*)&g_OrigCompiler_Compile);
                if (mh != MH_OK)
                {
                    char dbg[256];
                    sprintf_s(dbg, "[dxcompiler] MH_CreateHook Compiler::Compile failed: %s\n", MH_StatusToString(mh));
                    OutputDebugStringA(dbg);
                    pCompiler2->Release();
                    return;
                }
                MH_EnableHook(targetFunc);
            }

            MarkHooked(pCompiler2);
            pCompiler2->Release();
            OutputDebugStringW(L"[dxcompiler] IDxcCompiler2::Compile HOOKED\n");
            return;
        }
    }

    {
        IDxcCompiler* pCompiler = nullptr;
        HRESULT hr = pUnknown->QueryInterface(IID_IDxcCompiler, (void**)&pCompiler);
        if (SUCCEEDED(hr) && pCompiler)
        {
            if (IsAlreadyHooked(pCompiler))
            {
                pCompiler->Release();
                OutputDebugStringW(L"[dxcompiler] IDxcCompiler already hooked, skip\n");
                return;
            }

            void* targetFunc = vtable[3];
            if (!g_OrigCompiler_Compile)
            {
                MH_STATUS mh = MH_CreateHook(targetFunc, (LPVOID)&Hooked_Compiler_Compile, (LPVOID*)&g_OrigCompiler_Compile);
                if (mh != MH_OK)
                {
                    char dbg[256];
                    sprintf_s(dbg, "[dxcompiler] MH_CreateHook Compiler::Compile failed: %s\n", MH_StatusToString(mh));
                    OutputDebugStringA(dbg);
                    pCompiler->Release();
                    return;
                }
                MH_EnableHook(targetFunc);
            }

            MarkHooked(pCompiler);
            pCompiler->Release();
            OutputDebugStringW(L"[dxcompiler] IDxcCompiler::Compile HOOKED\n");
            return;
        }
    }

    OutputDebugStringW(L"[dxcompiler] Object is not a known compiler interface\n");
}

extern "C" __declspec(dllexport) HRESULT __stdcall DxcCreateInstance(
    _In_ REFCLSID  rclsid,
    _In_ REFIID    riid,
    _Out_ LPVOID*  ppv)
{
    if (!LoadOriginalDLL())
        return E_FAIL;

    HRESULT hr = g_OrigDxcCreateInstance(rclsid, riid, ppv);

    if (SUCCEEDED(hr) && *ppv)
    {
        if (IsEqualGUID(rclsid, CLSID_DxcCompiler))
        {
            TryInstallVTableHooks((IUnknown*)*ppv);
        }
    }

    return hr;
}

extern "C" __declspec(dllexport) HRESULT __stdcall DxcCreateInstance2(
    _In_ IMalloc*  pMalloc,
    _In_ REFCLSID  rclsid,
    _In_ REFIID    riid,
    _Out_ LPVOID*  ppv)
{
    if (!LoadOriginalDLL())
        return E_FAIL;

    HRESULT hr = g_OrigDxcCreateInstance2(pMalloc, rclsid, riid, ppv);

    if (SUCCEEDED(hr) && *ppv)
    {
        if (IsEqualGUID(rclsid, CLSID_DxcCompiler))
        {
            TryInstallVTableHooks((IUnknown*)*ppv);
        }
    }

    return hr;
}
