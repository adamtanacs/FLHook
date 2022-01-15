// Functions that allow plugins to write directly to character file and append information there.

#include "global.h"

typedef int(__cdecl *_GetFLName)(char *szBuf, const wchar_t *wszStr);
bool CharacterHook::bPatched = false;
CharacterHook* CharacterHook::Instance = nullptr;

static PlayerData *CurrPlayer;
int CharacterHook::HkCbUpdateFile(char *fileName, wchar_t *saveTime, int b) {
    // Call the original save charfile function
    int retv;
    __asm {
        pushad
        mov ecx, [CurrPlayer]
        push b
        push saveTime
        push fileName
        mov eax, 0x6d4ccd0
        call eax
        mov retv, eax
        popad
    }

    // Readd the flhook section.
    if (retv) {
        uint client = CurrPlayer->iOnlineID;

         // init variables
        char szDataPath[MAX_PATH];
        GetUserDataPath(szDataPath);

        std::string path = std::string(szDataPath) + R"(\Accts\MultiPlayer\)" + GetAccountDir(client) + "\\" + fileName;
        FILE *file;
        fopen_s(&file, path.c_str(), "a");
        if (file) {
            fprintf(file, "[flhook]\n");
            for (auto &i : clients[client].lines)
                fprintf(file, "%s = %s\n", i.first.c_str(), i.second.c_str());
            fclose(file);
        }
    }

    return retv;
}

std::string CharacterHook::GetAccountDir(uint client) {
    static _GetFLName GetFLName = reinterpret_cast<_GetFLName>(reinterpret_cast<char *>(hModServer) + 0x66370);
    char dirname[1024];
    GetFLName(dirname, Players[client].Account->wszAccID);
    return dirname;
}

std::string CharacterHook::GetCharFilename(const std::wstring &charName) {
    static _GetFLName GetFLName = reinterpret_cast<_GetFLName>(reinterpret_cast<char *>(hModServer) + 0x66370);
    char filename[1024];
    GetFLName(filename, charName.c_str());
    return filename;
}

void CharacterHook::LoadSettings() {
    clients.clear();
    struct PlayerData *pPD = nullptr;
    while ((pPD = Players.traverse_active(pPD))) {
        const uint client = pPD->iOnlineID;
        std::wstring charName = reinterpret_cast<const wchar_t *>(Players.GetActiveCharacterName(client));

        std::string filename = GetCharFilename(charName) + ".fl";
        CHARACTER_ID charid;
        strcpy_s(charid.szCharFilename, filename.c_str());
        CharacterSelect(charid, client);
    }
}

void CharacterHook::Disconnect(uint clientId, EFLConnection p2) {
    clients.erase(clientId);
}

CharacterHook::CharacterHook() {
    if (bPatched)
        return;

    Init();
}

CharacterHook::~CharacterHook() {
    if (Instance != this)
        return;

    if (bPatched) {
        bPatched = false;
        {
            BYTE patch[] = {0xE8, 0x84, 0x07, 0x00, 0x00};
            WriteProcMem((char *)hModServer + 0x6c547, patch, 5);
        }

        {
            BYTE patch[] = {0xE8, 0xFE, 0x2, 0x00, 0x00};
            WriteProcMem((char *)hModServer + 0x6c9cd, patch, 5);
        }
    }
}

CharacterHook *CharacterHook::Get() {
    return Instance;
}

void CharacterHook::ClearClientInfo(uint id) {
    clients.erase(id);
}

void CharacterHook::CharacterSelect(const CHARACTER_ID &charId, uint client) {
    char szDataPath[MAX_PATH];
    GetUserDataPath(szDataPath);
    std::string path = std::string(szDataPath) + R"(\Accts\MultiPlayer\)" + GetAccountDir(client) + "\\" + charId.szCharFilename;

    clients[client].charFileName = charId.szCharFilename;
    clients[client].lines.clear();

    // Read the flhook section so that we can rewrite after the save so that it
    // isn't lost
    INI_Reader ini;
    if (ini.open(path.c_str(), false)) {
        while (ini.read_header()) {
            if (ini.is_header("flhook")) {
                std::wstring tag;
                while (ini.read_value()) {
                    clients[client].lines[ini.get_name_ptr()] =
                        ini.get_value_string();
                }
            }
        }
        ini.close();
    }
}

std::string CharacterHook::IniGetS(uint client, const std::string &name) {
    if (clients.find(client) == clients.end())
        return "";

    if (!clients[client].charFileName.length())
        return "";

    if (clients[client].lines.find(name) == clients[client].lines.end())
        return "";

    return clients[client].lines[name];
}

std::wstring CharacterHook::IniGetWS(uint client, const std::string &name) {
    std::string svalue = IniGetS(client, name);

    std::wstring value;
    long lHiByte;
    long lLoByte;
    while (sscanf_s(svalue.c_str(), "%02X%02X", &lHiByte, &lLoByte) == 2) {
        svalue = svalue.substr(4);
        wchar_t wChar = (wchar_t)((lHiByte << 8) | lLoByte);
        value.append(1, wChar);
    }
    return value;
}

uint CharacterHook::IniGetI(uint client, const std::string &name) {
    std::string svalue = IniGetS(client, name);
    return strtoul(svalue.c_str(), 0, 10);
}

bool CharacterHook::IniGetB(uint client, const std::string &name) {
    std::string svalue = IniGetS(client, name);
    if (svalue == "yes")
        return true;
    return false;
}

float CharacterHook::IniGetF(uint client, const std::string &name) {
    std::string svalue = IniGetS(client, name);
    return (float)atof(svalue.c_str());
}

void CharacterHook::IniSetS(uint client, const std::string &name, const std::string &value) {
    clients[client].lines[name] = value;
}

void CharacterHook::IniSetWS(uint client, const std::string &name, const std::wstring &value) {
    std::string svalue = "";
    for (uint i = 0; (i < value.length()); i++) {
        char cHiByte = value[i] >> 8;
        char cLoByte = value[i] & 0xFF;
        char szBuf[8];
        sprintf_s(szBuf, "%02X%02X", ((uint)cHiByte) & 0xFF,
                  ((uint)cLoByte) & 0xFF);
        svalue += szBuf;
    }

    IniSetS(client, name, svalue);
}

void CharacterHook::IniSetI(uint client, const std::string &name, uint value) {
    char svalue[100];
    sprintf_s(svalue, "%u", value);
    IniSetS(client, name, svalue);
}

void CharacterHook::IniSetB(uint client, const std::string &name, bool value) {
    std::string svalue = value ? "yes" : "no";
    IniSetS(client, name, svalue);
}

void CharacterHook::IniSetF(uint client, const std::string &name, float value) {
    char svalue[100];
    sprintf_s(svalue, "%0.02f", value);
    IniSetS(client, name, svalue);
}

void CharacterHook::IniSetS(const std::wstring &charName, const std::string &name, const std::string &value) {
    // If the player is online then update the in memory cache.
    std::string charfilename = GetCharFilename(charName) + ".fl";
    for (auto &i : clients) {
        if (i.second.charFileName == charfilename) {
            IniSetS(i.first, name, value);
            return;
        }
    }

    // Otherwise write directly to the character file if it exists.
    st6::wstring flStr((ushort *)charName.c_str());
    CAccount *acc = Players.FindAccountFromCharacterName(flStr);
    if (acc) {
        char szDataPath[MAX_PATH];
        GetUserDataPath(szDataPath);
        std::string path = std::string(szDataPath) + R"(\Accts\MultiPlayer\)";
        std::string charpath = path + GetCharFilename(acc->wszAccID) + "\\" + charfilename;
        WritePrivateProfileString("flhook", name.c_str(), value.c_str(), charpath.c_str());
    }
}

void CharacterHook::IniSetWS(const std::wstring &charName, const std::string &name, const std::wstring &value) {
    std::string svalue = "";
    for (uint i = 0; (i < value.length()); i++) {
        char cHiByte = value[i] >> 8;
        char cLoByte = value[i] & 0xFF;
        char szBuf[8];
        sprintf_s(szBuf, "%02X%02X", ((uint)cHiByte) & 0xFF,
                  ((uint)cLoByte) & 0xFF);
        svalue += szBuf;
    }

    IniSetS(charName, name, svalue);
}
    

void CharacterHook::IniSetI(const std::wstring &charName, const std::string &name, uint value) {
    char svalue[100];
    sprintf_s(svalue, "%u", value);
    IniSetS(charName, name, svalue);
}

void CharacterHook::IniSetB(const std::wstring &charName, const std::string &name, bool value) {
    const std::string svalue = value ? "yes" : "no";
    IniSetS(charName, name, svalue);
}

void CharacterHook::IniSetF(const std::wstring &charName, const std::string &name, float value) {
    char svalue[100];
    sprintf_s(svalue, "%0.02f", value);
    IniSetS(charName, name, svalue);
}

__declspec(naked) void HkCbUpdateFileNaked() {
    __asm {
        mov CurrPlayer, ecx
        jmp CharacterHook::HkCbUpdateFile
    }
}

void CharacterHook::Init() {
    Instance = this;
    bPatched = true;
    PatchCallAddr((char *)hModServer, 0x6c547, (char *)HkCbUpdateFileNaked);
    PatchCallAddr((char *)hModServer, 0x6c9cd, (char *)HkCbUpdateFileNaked);
}