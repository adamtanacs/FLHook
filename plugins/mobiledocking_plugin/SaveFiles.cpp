#include "Main.h"


void LoadDockInfo(uint client) {
    CLIENT_DATA &cd = clients[client];

    // How many docking modules do we have?
    cd.iDockingModules = 0;
    cd.mapDockedShips.clear();

    for (auto item = Players[client].equipDescList.equip.begin();
         item != Players[client].equipDescList.equip.end(); item++) {
        if (item->bMounted && item->iArchID == 0xB85AB480) {
            cd.iDockingModules++;
        }
    }

    // Load docked ships until we run out of docking module space.
    uint count = CharacterHook::Get()->IniGetI(client, "dock.docked_ships_count");
    for (uint i = 1;
         i <= count && cd.mapDockedShips.size() <= cd.iDockingModules; i++) {
        char key[100];
        sprintf_s(key, "dock.docked_ship.%u", i);
        std::wstring charname = CharacterHook::Get()->IniGetWS(client, key);
        if (charname.length()) {
            if (HkGetAccountByCharname(charname)) {
                cd.mapDockedShips[charname] = charname;
            }
        }
    }

    cd.wscDockedWithCharname =
        CharacterHook::Get()->IniGetWS(client, "dock.docked_with_charname");
    if (cd.wscDockedWithCharname.length())
        cd.mobile_docked = true;

    cd.iLastBaseID = CharacterHook::Get()->IniGetI(client, "dock.last_base");
    cd.iCarrierSystem = CharacterHook::Get()->IniGetI(client, "dock.carrier_system");
    cd.vCarrierLocation.x = CharacterHook::Get()->IniGetF(client, "dock.carrier_pos.x");
    cd.vCarrierLocation.y = CharacterHook::Get()->IniGetF(client, "dock.carrier_pos.y");
    cd.vCarrierLocation.z = CharacterHook::Get()->IniGetF(client, "dock.carrier_pos.z");

    Vector vRot;
    vRot.x = CharacterHook::Get()->IniGetF(client, "dock.carrier_rot.x");
    vRot.y = CharacterHook::Get()->IniGetF(client, "dock.carrier_rot.y");
    vRot.z = CharacterHook::Get()->IniGetF(client, "dock.carrier_rot.z");
    cd.mCarrierLocation = EulerMatrix(vRot);
}

void SaveDockInfo(uint client) {
    CLIENT_DATA &cd = clients[client];

    if (cd.mobile_docked) {
        CharacterHook::Get()->IniSetWS(client, "dock.docked_with_charname",
                          cd.wscDockedWithCharname);
        CharacterHook::Get()->IniSetI(client, "dock.last_base", cd.iLastBaseID);
        CharacterHook::Get()->IniSetI(client, "dock.carrier_system", cd.iCarrierSystem);
        CharacterHook::Get()->IniSetF(client, "dock.carrier_pos.x", cd.vCarrierLocation.x);
        CharacterHook::Get()->IniSetF(client, "dock.carrier_pos.y", cd.vCarrierLocation.y);
        CharacterHook::Get()->IniSetF(client, "dock.carrier_pos.z", cd.vCarrierLocation.z);

        Vector vRot = MatrixToEuler(cd.mCarrierLocation);
        CharacterHook::Get()->IniSetF(client, "dock.carrier_rot.x", vRot.x);
        CharacterHook::Get()->IniSetF(client, "dock.carrier_rot.y", vRot.y);
        CharacterHook::Get()->IniSetF(client, "dock.carrier_rot.z", vRot.z);
    } else {
        CharacterHook::Get()->IniSetWS(client, "dock.docked_with_charname", L"");
        CharacterHook::Get()->IniSetI(client, "dock.last_base", 0);
        CharacterHook::Get()->IniSetI(client, "dock.carrier_system", 0);
        CharacterHook::Get()->IniSetI(client, "dock.carrier_pos.x", 0);
        CharacterHook::Get()->IniSetI(client, "dock.carrier_pos.y", 0);
        CharacterHook::Get()->IniSetI(client, "dock.carrier_pos.z", 0);
        CharacterHook::Get()->IniSetI(client, "dock.carrier_rot.x", 0);
        CharacterHook::Get()->IniSetI(client, "dock.carrier_rot.y", 0);
        CharacterHook::Get()->IniSetI(client, "dock.carrier_rot.z", 0);
    }

    if (cd.mapDockedShips.size() > 0) {
        int index = 1;
        for (map<std::wstring, std::wstring>::iterator i =
                 cd.mapDockedShips.begin();
             i != cd.mapDockedShips.end(); ++i, ++index) {
            char key[100];
            sprintf_s(key, "dock.docked_ship.%u", index);
            CharacterHook::Get()->IniSetWS(client, key, i->second);
        }
        CharacterHook::Get()->IniSetI(client, "dock.docked_ships_count",
                         cd.mapDockedShips.size());
    } else {
        CharacterHook::Get()->IniSetI(client, "dock.docked_ships_count", 0);
    }
}

void UpdateDockInfo(const std::wstring &wscCharname, uint iSystem, Vector pos,
                    Matrix rot) {
    CharacterHook::Get()->IniSetI(wscCharname, "dock.carrier_system", iSystem);
    CharacterHook::Get()->IniSetF(wscCharname, "dock.carrier_pos.x", pos.x);
    CharacterHook::Get()->IniSetF(wscCharname, "dock.carrier_pos.y", pos.y);
    CharacterHook::Get()->IniSetF(wscCharname, "dock.carrier_pos.z", pos.z);

    Vector vRot = MatrixToEuler(rot);

    CharacterHook::Get()->IniSetF(wscCharname, "dock.carrier_rot.x", vRot.x);
    CharacterHook::Get()->IniSetF(wscCharname, "dock.carrier_rot.y", vRot.y);
    CharacterHook::Get()->IniSetF(wscCharname, "dock.carrier_rot.z", vRot.z);
}
