#include "pch.h"
#include "shader_inspect.h"

#define DXC_CP_ACP 0

SMVersion SMVersion::FromTargetProfile(const wchar_t* profile)
{
    if (!profile) return SMVersion();
    
    const wchar_t* p = profile;
    while (*p && !(iswdigit(*p) && p > profile && *(p - 1) == L'_'))
        ++p;
    if (!*p) return SMVersion();
    int mj = *p - L'0'; ++p;
    if (*p == L'_') ++p;
    if (!iswdigit(*p)) return SMVersion();
    int mn = *p - L'0';
    return SMVersion(mj, mn);
}

std::wstring SMVersion::ToTargetProfile(const wchar_t* baseProfile) const
{
    if (!baseProfile) return L"";

    std::wstring prefix;
    const wchar_t* p = baseProfile;

    while (*p && !(iswdigit(*p) && p > baseProfile && *(p - 1) == L'_'))
    {
        prefix += *p;
        ++p;
    }
    if (prefix.empty() || prefix.back() != L'_')
        prefix += L'_';

    wchar_t result[32];
    swprintf_s(result, L"%s%d_%d", prefix.c_str(), major, minor);
    return result;
}

void ACAutomaton::Clear()
{
    m_nodes.clear();
    m_nodes.push_back(Node());
    m_patternCount = 0;
}

void ACAutomaton::Build(const std::vector<WhitelistEntry>& patterns)
{
    Clear();
    m_patternCount = (int)patterns.size();

    for (int pi = 0; pi < m_patternCount; ++pi)
    {
        const std::wstring& name = patterns[pi].name;
        int cur = 0;
        for (wchar_t ch : name)
        {
            auto it = m_nodes[cur].next.find(ch);
            if (it == m_nodes[cur].next.end())
            {
                int nextIdx = (int)m_nodes.size();
                m_nodes[cur].next[ch] = nextIdx;
                m_nodes.push_back(Node());
                cur = nextIdx;
            }
            else
            {
                cur = it->second;
            }
        }
        m_nodes[cur].output = pi;
    }

    std::vector<int> queue;
    queue.reserve(m_nodes.size());

    for (auto& kv : m_nodes[0].next)
    {
        m_nodes[kv.second].fail = 0;
        queue.push_back(kv.second);
    }

    for (size_t front = 0; front < queue.size(); ++front)
    {
        int cur = queue[front];

        for (auto& kv : m_nodes[cur].next)
        {
            wchar_t ch = kv.first;
            int child = kv.second;

            int f = m_nodes[cur].fail;
            while (f > 0)
            {
                auto it = m_nodes[f].next.find(ch);
                if (it != m_nodes[f].next.end()) break;
                f = m_nodes[f].fail;
            }
            auto it = m_nodes[f].next.find(ch);
            m_nodes[child].fail = (it != m_nodes[f].next.end()) ? it->second : 0;

            queue.push_back(child);
        }
    }
}

bool ACAutomaton::IsIdentifierEnd(wchar_t c)
{
    return !(iswalnum(c) || c == L'_');
}

void ACAutomaton::Search(const wchar_t* text, size_t len, std::vector<int>& outPatternIndices) const
{
    outPatternIndices.clear();
    if (m_nodes.empty() || !text || len == 0) return;

    std::vector<bool> found(m_patternCount, false);

    int cur = 0;
    for (size_t i = 0; i < len; ++i)
    {
        wchar_t ch = text[i];

        while (cur > 0)
        {
            auto it = m_nodes[cur].next.find(ch);
            if (it != m_nodes[cur].next.end()) break;
            cur = m_nodes[cur].fail;
        }
        auto it = m_nodes[cur].next.find(ch);
        cur = (it != m_nodes[cur].next.end()) ? it->second : 0;

        for (int node = cur; node > 0; node = m_nodes[node].fail)
        {
            int pi = m_nodes[node].output;
            if (pi >= 0 && !found[pi])
            {
                size_t after = i + 1;
                if (after < len)
                {
                    wchar_t nc = text[after];
                    if (nc == L'(')
                    {
                        found[pi] = true;
                    }
                    else if (nc == L' ' && after + 1 < len && text[after + 1] == L'(')
                    {
                        found[pi] = true;
                    }
                }
            }
        }
    }

    for (int i = 0; i < m_patternCount; ++i)
    {
        if (found[i])
            outPatternIndices.push_back(i);
    }
}

std::wstring ShaderInspector::StripCommentsAndStrings(const std::wstring& source)
{
    std::wstring result;
    result.reserve(source.size());
    size_t i = 0;
    size_t n = source.size();

    while (i < n)
    {
        if (i + 1 < n && source[i] == L'/' && source[i + 1] == L'/')
        {
            i += 2;
            while (i < n && source[i] != L'\n' && source[i] != L'\r')
            {
                result += L' ';
                ++i;
            }
            if (i < n && source[i] == L'\r') { result += source[i]; ++i; }
            if (i < n && source[i] == L'\n') { result += source[i]; ++i; }
            continue;
        }

        if (i + 1 < n && source[i] == L'/' && source[i + 1] == L'*')
        {
            i += 2;
            result += L' '; result += L' ';
            while (i + 1 < n && !(source[i] == L'*' && source[i + 1] == L'/'))
            {
                result += (source[i] == L'\n' || source[i] == L'\r') ? source[i] : L' ';
                ++i;
            }
            if (i + 1 < n) { result += L' '; result += L' '; i += 2; }
            continue;
        }

        if (source[i] == L'"')
        {
            result += L' ';
            ++i;
            while (i < n)
            {
                if (source[i] == L'\\' && i + 1 < n)
                {
                    result += L' '; result += L' '; i += 2;
                }
                else if (source[i] == L'"')
                {
                    result += L' ';
                    ++i;
                    break;
                }
                else
                {
                    result += L' ';
                    ++i;
                }
            }
            continue;
        }

        result += source[i];
        ++i;
    }

    return result;
}

std::wstring ShaderInspector::ToWideString(const void* data, size_t size, UINT encoding)
{
    if (!data || size == 0) return L"";

    if (encoding == 0 || encoding == 1200 || encoding == DXC_CP_ACP)
    {
        size_t wlen = size / sizeof(wchar_t);
        return std::wstring((const wchar_t*)data, wlen);
    }

    if (encoding == 65001)
    {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)data, (int)size, nullptr, 0);
        if (wlen <= 0) return L"";
        std::wstring result(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, (LPCCH)data, (int)size, &result[0], wlen);

        if (!result.empty() && result.back() == L'\0')
            result.pop_back();
        return result;
    }

    return L"";
}

// ============================================================================
// 内置白名单
// ============================================================================
void ShaderInspector::BuildBuiltinWhitelist()
{
    m_whitelist.clear();

    auto add = [this](const wchar_t* name, int major, int minor) {
        m_whitelist.push_back({ name, SMVersion(major, minor) });
    };

    add(L"Require16bitType", 6, 2);

    //ref: https://github.com/microsoft/DirectXShaderCompiler/wiki/Shader-Model-6.4
    add(L"dot4add_u8packed", 6, 4);
    add(L"dot4add_i8packed", 6, 4);
    add(L"dot2add",          6, 4);

    //ref: https://microsoft.github.io/DirectX-Specs/d3d/HLSL_ShaderModel6_5.html#new-wave-intrinsics
    add(L"WaveMatch",                6, 5);
    add(L"WaveMultiPrefixSum",       6, 5);
    add(L"WaveMultiPrefixProduct",   6, 5);
    add(L"WaveMultiPrefixBitAnd",    6, 5);
    add(L"WaveMultiPrefixBitOr",     6, 5);
    add(L"WaveMultiPrefixBitXor",    6, 5);
    add(L"WaveMultiPrefixCountBits", 6, 5);

    //ref: https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_Int64_and_Float_Atomics.html
    //注意, 这里必须用我们自己重新封装的Lib(或者自己封装一下), 防止与SM5.0的冲突了
    add(L"NUS_Require_SM66_Atomics", 6, 6);

    //ref: https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_DynamicResources.html
    add(L"NUS_Require_SM66_Bindless", 6, 6);

    //ref: https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_Pack_Unpack_Intrinsics.html
    add(L"pack4x8",     6, 6);
    add(L"pack4x8un",   6, 6);
    add(L"pack4x8sn",   6, 6);
    add(L"unpack4x8",   6, 6);
    add(L"unpack4x8un", 6, 6);
    add(L"unpack4x8sn", 6, 6);

    //ref: https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_7_QuadAny_QuadAll.html
    add(L"QuadAny", 6, 7);
    add(L"QuadAll", 6, 7);

    //@TODO: 用上SM6.9的估计得等几年了, 挺馋的
    //ref: https://github.com/microsoft/hlsl-specs/blob/main/proposals/0026-hlsl-long-vector-type.md
}

struct IDxcVersionInfo : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetVersion(_Out_ UINT32* pMajor, _Out_ UINT32* pMinor) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFlags(_Out_ UINT32* pFlags) = 0;
};

void ShaderInspector::FilterByDxcCapability(HMODULE hOrigDll)
{
    if (!hOrigDll) { m_activeWhitelist = m_whitelist; return; }

    typedef HRESULT(__stdcall* PFN_DxcCreate)(REFCLSID, REFIID, LPVOID*);
    auto pfnCreate = (PFN_DxcCreate)GetProcAddress(hOrigDll, "DxcCreateInstance");
    if (!pfnCreate) { m_activeWhitelist = m_whitelist; return; }

    CLSID CLSID_DxcCompiler = {
        0x73e22d93, 0xe6ce, 0x47f3, {0xb5, 0xbf, 0xf0, 0x66, 0x4f, 0x39, 0xc1, 0xb0}
    };
    IID IID_IDxcVersionInfo = {
        0xb04f5b50, 0x2059, 0x4f12, {0xa8, 0xff, 0xa1, 0xe0, 0xcd, 0xe1, 0xcc, 0x7e}
    };

    IUnknown* pCompiler = nullptr;
    HRESULT hr = pfnCreate(CLSID_DxcCompiler, __uuidof(IUnknown), (LPVOID*)&pCompiler);
    if (FAILED(hr) || !pCompiler) { m_activeWhitelist = m_whitelist; return; }

    IDxcVersionInfo* pVer = nullptr;
    hr = pCompiler->QueryInterface(IID_IDxcVersionInfo, (LPVOID*)&pVer);
    pCompiler->Release();

    int dxcMajor = 1, dxcMinor = 6;
    if (SUCCEEDED(hr) && pVer)
    {
        UINT32 maj = 0, min = 0;
        if (SUCCEEDED(pVer->GetVersion(&maj, &min)) && maj > 0)
        {
            dxcMajor = (int)maj;
            dxcMinor = (int)min;
        }
        pVer->Release();
    }

    int maxSMMinor = dxcMinor;
    if (dxcMajor < 1 || (dxcMajor == 1 && dxcMinor < 4))
        maxSMMinor = 5;
    SMVersion maxSM(6, maxSMMinor);

    m_activeWhitelist.clear();
    for (const auto& entry : m_whitelist)
    {
        if (!(entry.minSM > maxSM))
            m_activeWhitelist.push_back(entry);
    }

    wchar_t dbg[256];
    swprintf_s(dbg, L"[dxcompiler] DXC v%d.%d → max SM 6.%d, whitelist: %zu/%zu entries active\n",
        dxcMajor, dxcMinor, maxSMMinor, m_activeWhitelist.size(), m_whitelist.size());
    OutputDebugStringW(dbg);
}


ShaderInspector& ShaderInspector::Instance()
{
    static ShaderInspector inst;
    return inst;
}

void ShaderInspector::Initialize(HMODULE hOrigDll)
{
    if (m_initialized) return;

    BuildBuiltinWhitelist();
    FilterByDxcCapability(hOrigDll);

    if (!m_activeWhitelist.empty())
    {
        m_ac.Build(m_activeWhitelist);
        wchar_t dbg[128];
        swprintf_s(dbg, L"[dxcompiler] AC automaton built with %zu patterns\n", m_activeWhitelist.size());
        OutputDebugStringW(dbg);
    }
    else
    {
        OutputDebugStringW(L"[dxcompiler] WARNING: No active whitelist patterns after filtering!\n");
    }

    m_initialized = true;
}

SMVersion ShaderInspector::Inspect(const void* sourceData, size_t size, UINT encoding) const
{
    if (!m_initialized || m_activeWhitelist.empty() || !sourceData || size == 0)
        return SMVersion();

    std::wstring wideSrc = ToWideString(sourceData, size, encoding);
    if (wideSrc.empty()) return SMVersion();

    std::wstring clean = StripCommentsAndStrings(wideSrc);

    std::vector<int> hits;
    m_ac.Search(clean.c_str(), clean.size(), hits);

    SMVersion maxSM;
    for (int idx : hits)
    {
        if (idx >= 0 && idx < (int)m_activeWhitelist.size())
        {
            const SMVersion& v = m_activeWhitelist[idx].minSM;
            if (v > maxSM) maxSM = v;
        }
    }

    return maxSM;
}
