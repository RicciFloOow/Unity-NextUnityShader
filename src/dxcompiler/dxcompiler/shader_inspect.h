#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

struct SMVersion {
    int major;
    int minor;

    SMVersion(int mj = 0, int mn = 0) : major(mj), minor(mn) {}

    bool operator>(const SMVersion& other) const {
        return (major > other.major) || (major == other.major && minor > other.minor);
    }
    bool operator>=(const SMVersion& other) const {
        return (major > other.major) || (major == other.major && minor >= other.minor);
    }
    bool operator==(const SMVersion& other) const {
        return major == other.major && minor == other.minor;
    }

    bool IsValid() const { return major > 0; }

    static SMVersion FromTargetProfile(const wchar_t* profile);

    std::wstring ToTargetProfile(const wchar_t* baseProfile) const;
};

struct WhitelistEntry {
    std::wstring name;
    SMVersion     minSM;
};

class ACAutomaton {
public:
    ACAutomaton() { Clear(); }

    void Build(const std::vector<WhitelistEntry>& patterns);

    void Search(const wchar_t* text, size_t len, std::vector<int>& outPatternIndices) const;

    // 清空重建
    void Clear();

private:
    struct Node {
        int fail = 0;
        int output = -1;
        std::unordered_map<wchar_t, int> next;
    };

    std::vector<Node> m_nodes;
    int               m_patternCount = 0;

    static bool IsIdentifierEnd(wchar_t c);
};

class ShaderInspector {
public:
    static ShaderInspector& Instance();

    void Initialize(HMODULE hOrigDll);

    SMVersion Inspect(const void* sourceData, size_t size, UINT encoding) const;

    bool NeedsUpgrade(SMVersion currentTarget, SMVersion required) const {
        return required.IsValid() && required > currentTarget;
    }

private:
    ShaderInspector() = default;

    ACAutomaton m_ac;
    std::vector<WhitelistEntry> m_whitelist;
    std::vector<WhitelistEntry> m_activeWhitelist;
    bool m_initialized = false;

    void BuildBuiltinWhitelist();

    void FilterByDxcCapability(HMODULE hOrigDll);

    static std::wstring ToWideString(const void* data, size_t size, UINT encoding);

    static std::wstring StripCommentsAndStrings(const std::wstring& source);
};
