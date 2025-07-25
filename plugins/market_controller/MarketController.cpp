﻿#include "PCH.hpp"

#include "MarketController.hpp"
#include "API/FLHook/ResourceManager.hpp"
#include "API/Utils/Random.hpp"

namespace Plugins
{
    void MarketControllerPlugin::ProcessNpcDrops()
    {

        for (auto& item : dropMap)
        {
            uint shipId = item.first;
            auto iobj = FLHook::GetObjInspect(shipId);

            if (!iobj)
            {
                continue;
            }

            CShip* cship = reinterpret_cast<CShip*>(iobj->cobj);

            if (cship->hitPoints <= 1.0f)
            {
                continue;
            }

            float totalCargoToJettison = item.second;
            if (totalCargoToJettison <= 0.0f)
            {
                continue;
            }

            std::unordered_map<uint, float>* overrideMap = nullptr;
            auto volumeOverrideIter = cargoVolumeOverrideMap.find(cship->shiparch()->shipClass);
            if (volumeOverrideIter != cargoVolumeOverrideMap.end())
            {
                overrideMap = &volumeOverrideIter->second;
            }

            CEquipTraverser tr((uint)EquipmentClass::Cargo);
            CEquipManager& eqManager = cship->equipManager;

            CECargo* cargo = nullptr;
            while (cargo = reinterpret_cast<CECargo*>(eqManager.Traverse(tr)))
            {
                float volume = cargo->archetype->volume;

                if (overrideMap)
                {
                    auto overrideIter = overrideMap->find(cargo->archetype->archId.GetValue());
                    if (overrideIter != overrideMap->end())
                    {
                        volume = overrideIter->second;
                    }
                }

                if (volume == 0.0f)
                {
                    continue;
                }
                bool flag = false;
                pub::IsCommodity(cargo->archetype->archId.GetValue(), flag);
                if (!flag)
                {
                    continue;
                }

                float amountToJettison = std::min(static_cast<float>(cargo->count), ceilf(totalCargoToJettison / volume));
                iobj->jettison_cargo(cargo->SubObjId, static_cast<ushort>(amountToJettison), 0);

                totalCargoToJettison -= amountToJettison * volume;
                if (totalCargoToJettison <= 0.0f)
                {
                    break;
                }
            }
        }

        dropMap.clear();
    }

    MarketControllerPlugin::MarketControllerPlugin(const PluginInfo& info) : Plugin(info)
    {
        AddTimer([this] { ProcessNpcDrops(); }, 1000);
    }

    void MarketControllerPlugin::LoadGameData()
    {
        colGrpCargoMap.clear();
        unstableJumpObjMap.clear();

        INI_Reader ini;

        char szCurDir[MAX_PATH];
        GetCurrentDirectoryA(sizeof(szCurDir), szCurDir);
        std::string currDir = std::string(szCurDir);
        std::string scFreelancerIniFile = currDir + R"(\freelancer.ini)";

        std::string gameDir = currDir.substr(0, currDir.length() - 4);
        gameDir += std::string(R"(\DATA\)");

        if (!ini.open(scFreelancerIniFile.c_str(), false))
        {
            return;
        }

        std::vector<std::string> shipFiles;
        std::vector<std::string> equipFiles;
        std::vector<std::string> solarFiles;

        while (ini.read_header())
        {
            if (!ini.is_header("Data"))
            {
                continue;
            }
            while (ini.read_value())
            {
                if (ini.is_value("ships"))
                {
                    shipFiles.emplace_back(ini.get_value_string());
                }
                else if (ini.is_value("equipment"))
                {
                    equipFiles.emplace_back(ini.get_value_string());
                }
                else if (ini.is_value("solar"))
                {
                    solarFiles.emplace_back(ini.get_value_string());
                }
            }
        }

        ini.close();

        for (std::string& shipFile : shipFiles)
        {
            shipFile = gameDir + shipFile;
            if (!ini.open(shipFile.c_str(), false))
            {
                continue;
            }

            uint currNickname = 0;
            ushort currSID = 3;
            float totalColGrpCapacity = 0.0f;
            while (ini.read_header())
            {
                if (ini.is_header("Ship"))
                {
                    while (ini.read_value())
                    {
                        if (ini.is_value("nickname"))
                        {
                            currNickname = CreateID(ini.get_value_string());
                            currSID = 3;
                            totalColGrpCapacity = 0.0f;
                            break;
                        }
                    }
                }
                else if (ini.is_header("CollisionGroup"))
                {
                    currSID++;
                    while (ini.read_value())
                    {
                        if (ini.is_value("cargo_capacity"))
                        {
                            totalColGrpCapacity += ini.get_value_float(0);
                            colGrpCargoMap[currNickname].first = totalColGrpCapacity;
                            colGrpCargoMap[currNickname].second[currSID] = ini.get_value_float(0);
                            break;
                        }
                    }
                }
            }
            ini.close();
        }

        for (std::string& equipFile : equipFiles)
        {
            equipFile = gameDir + equipFile;

            if (!ini.open(equipFile.c_str(), false))
            {
                continue;
            }

            uint currNickname = 0;

            while (ini.read_header())
            {
                LootData ld;
                bool hasValue = false;
                while (ini.read_value())
                {
                    if (ini.is_value("nickname"))
                    {
                        currNickname = CreateID(ini.get_value_string());
                    }
                    else if (ini.is_value("max_drop_npc"))
                    {
                        ld.maxDropNPC = ini.get_value_int(0);
                        hasValue = true;
                    }
                    else if (ini.is_value("max_drop_player"))
                    {
                        ld.maxDropPlayer = ini.get_value_int(0);
                        hasValue = true;
                    }
                    else if (ini.is_value("drop_chance_npc_unmounted"))
                    {
                        ld.dropChanceUnmounted = ini.get_value_float(0);
                        hasValue = true;
                    }
                    else if (ini.is_value("drop_chance_npc_mounted"))
                    {
                        ld.dropChanceMounted = ini.get_value_float(0);
                        hasValue = true;
                    }
                    else if (ini.is_value("volume_class_override"))
                    {
                        cargoVolumeOverrideMap[ini.get_value_int(0)][currNickname] = ini.get_value_float(1);
                    }
                }
                if (hasValue)
                {
                    lootData[currNickname] = ld;
                }
            }
            ini.close();
        }

        for (std::string& solarFile : solarFiles)
        {
            solarFile = gameDir + solarFile;

            if (!ini.open(solarFile.c_str(), false))
            {
                continue;
            }

            uint currNickname = 0;

            while (ini.read_header())
            {
                if (!ini.is_header("Solar"))
                {
                    continue;
                }
                while (ini.read_value())
                {
                    if (ini.is_value("nickname"))
                    {
                        currNickname = CreateID(ini.get_value_string());
                    }
                    else if (ini.is_value("cargo_limit"))
                    {
                        unstableJumpObjMap[currNickname] = ini.get_value_float(0);
                        break;
                    }
                }
            }

            ini.close();
        }
    }

    bool MarketControllerPlugin::OnLoadSettings()
    {
        if (!LoadSettingsCL())
        {
            return false;
        }

        LoadJsonWithValidation(Config, config, "config/market_controller.json");

        if (config.clootUnseenDespawnRange)
        {
            float** CLOOT_UNSEEN_RADIUS = (float**)0x6D64420;
            **CLOOT_UNSEEN_RADIUS = config.clootUnseenDespawnRange;
        }

        LoadGameData();

        return true;
    }

    void MarketControllerPlugin::OnAcceptTrade(ClientId client, bool newTradeAcceptState)
    {

        auto tradeOffer = client.GetTradeOffer().Unwrap();
        if (newTradeAcceptState && tradeOffer && tradeOffer->equipOffer.equip.size() + Players[tradeOffer->targetClient].equipAndCargo.equip.size() >= 127)
        {
            client.MessageErr(L"ERR Target player holds too many items, trade cannot proceed");
            returnCode = ReturnCode::SkipAll;
        }
    }

    bool MarketControllerPlugin::OnSystemSwitchOutPacket(ClientId client, FLPACKET_SYSTEM_SWITCH_OUT& packet)
    {
        ClientId jumpClient = ShipId(packet.shipId).GetPlayer().Unwrap();
        if (client != jumpClient)
        {
            return true;
        }
        uint spaceArchId;
        pub::SpaceObj::GetSolarArchetypeID(packet.jumpObjectId, spaceArchId);
        auto jumpCargoLimit = unstableJumpObjMap.find(spaceArchId);
        if (jumpCargoLimit == unstableJumpObjMap.end())
        {
            return true;
        }
        if (EquipDescCommodityVolume(jumpClient) <= jumpCargoLimit->second)
        {
            return true;
        }

        std::vector<std::pair<ushort, int>> itemsToRemove;
        for (auto& eq : Players[jumpClient.GetValue()].equipAndCargo.equip)
        {
            bool isCommodity;
            pub::IsCommodity(eq.archId.GetValue(), isCommodity);
            if (isCommodity)
            {
                itemsToRemove.push_back({ eq.id, eq.count });
            }
        }

        for (auto& item : itemsToRemove)
        {
            jumpClient.RemoveCargo(item.first, item.second);
        }
        return true;
    }

    std::optional<DOCK_HOST_RESPONSE> MarketControllerPlugin::OnDockCall(const ShipId& shipId, const ObjectId& spaceId, int dockPortIndex,
                                                                         DOCK_HOST_RESPONSE response)
    {
        ClientId client = ShipId(shipId).GetPlayer().Unwrap();
        if (!client)
        {
            return {};
        }

        if ((response == DOCK_HOST_RESPONSE::ProceedDock || response == DOCK_HOST_RESPONSE::Dock) && dockPortIndex!= -1)
        {
            uint solarArchetypeID;
            pub::SpaceObj::GetSolarArchetypeID(spaceId.GetId().Unwrap(), solarArchetypeID);

            auto unstableJumpObjInfo = unstableJumpObjMap.find(solarArchetypeID);
            if (unstableJumpObjInfo != unstableJumpObjMap.end() && EquipDescCommodityVolume(client) > unstableJumpObjInfo->second)
            {
                returnCode = ReturnCode::SkipPlugins;
                client.MessageErr(std::format(L"ERR This jumphole is unstable, can't take more than {} cargo through!", unstableJumpObjInfo->second));
                return {DOCK_HOST_RESPONSE::AccessDenied};
            }
        }

        return {};
    }

    void MarketControllerPlugin::OnLogin(ClientId client, const SLoginInfo& li)
    {
        // TODO: Send price overrides to the client
    }

    void MarketControllerPlugin::OnGfGoodSell(ClientId client, const SGFGoodSellInfo& info)
    {
        // TODO: Add monitoring for certain item sales, previously handled in MF

        const GoodInfo* gi = GoodList_get()->find_by_id(info.archId.GetValue());
        if (gi->type != GoodType::Commodity)
        {
            return;
        }

        BaseData* bd = BaseDataList_get()->get_base_data(client.GetCurrentBase().Unwrap().GetValue());
        auto marketData = bd->marketMap.find(info.archId);
        if (marketData == bd->marketMap.end())
        {
            return;
        }
        int sellPrice = marketData->second.min;
        int currPrice = static_cast<int>(marketData->second.price);
        client.AddCash(info.count * (sellPrice - currPrice));
    }

    void MarketControllerPlugin::OnGfGoodBuy(ClientId client, const SGFGoodBuyInfo& info)
    {
        if (!GFGoodBuyCL(info, client))
        {
            returnCode = ReturnCode::SkipFunctionCall;
        }
    }

    float MarketControllerPlugin::EquipDescCommodityVolume(ClientId clientId, Id shipArch)
    {
        Archetype::Ship* ship = Archetype::GetShip(shipArch.GetValue());
        if (!ship)
        {
            return 0.0f;
        }

        auto cargoVolumeOverrideIter = cargoVolumeOverrideMap.find(ship->shipClass);
        std::unordered_map<uint, float>* cargoOverrideMap = nullptr;
        if (cargoVolumeOverrideIter != cargoVolumeOverrideMap.end())
        {
            cargoOverrideMap = &cargoVolumeOverrideIter->second;
        }

        float cargoUsed = 0.0f;
        for (auto& eq : Players[clientId.GetValue()].equipAndCargo.equip)
        {
            if (eq.mounted)
            {
                continue;
            }
            if (cargoOverrideMap)
            {
                auto cargoData = cargoOverrideMap->find(eq.archId.GetValue());
                if (cargoData != cargoOverrideMap->end())
                {
                    cargoUsed += eq.count * cargoData->second;
                    continue;
                }
            }
            auto archData = Archetype::GetEquipment(eq.archId.GetValue());
            cargoUsed += eq.count * archData->volume;
        }

        return cargoUsed;
    }

    float MarketControllerPlugin::EquipDescCommodityVolume(ClientId clientId)
    {
        return EquipDescCommodityVolume(clientId, clientId.GetShipArch().Unwrap().GetId());
    }

    void MarketControllerPlugin::OnGfGoodBuyAfter(ClientId client, const SGFGoodBuyInfo& info)
    {
        const GoodInfo* gi = GoodList_get()->find_by_id(info.goodId.GetValue());

        if (!gi || gi->type != GoodType::Ship)
        {
            return;
        }
        const GoodInfo* shipGoodInfo = GoodList_get()->find_by_id(gi->hullGoodId);

        auto shipArch = Archetype::GetShip(shipGoodInfo->shipGoodId);
        float usedCargoSpace = EquipDescCommodityVolume(client, shipArch->archId);
        float missingCargoSpace = usedCargoSpace - shipArch->holdSize;
        if (missingCargoSpace > 0.0f)
        {
            client.MessageErr(L"WARNING: Your new ship cannot fit all the cargo it currently holds. Sell it or you will be kicked upon undocking.");
            client.MessageErr(std::format(L"Cargo hold is exceeded by {} units", missingCargoSpace));
        }
    }

    void MarketControllerPlugin::OnRequestChangeCash(ClientId client, int newCash)
    {
        if (!ReqChangeCashCL(client))
        {
            returnCode = ReturnCode::SkipAll;
        }
    }

    void MarketControllerPlugin::OnRequestAddItem(ClientId client, GoodId& goodId, std::wstring_view hardpoint, int count, float status, bool mounted)
    {
        if (!ReqAddItemCL(client))
        {
            returnCode = ReturnCode::SkipAll;
        }
    }

    void MarketControllerPlugin::OnShipDestroy(Ship* ship, DamageList* dmgList, ShipId killerId)
    {
        if (!config.enableCustomLootDrops)
        {
            return;
        }

        CShip* cship = reinterpret_cast<CShip*>(ship->cobj);
        if (!cship->ownerPlayer)
        {
            auto npcKillerClient = FLHook::GetResourceManager()->GetLastAttackingPlayer(ship->cship()->id.GetValue());
            if (!npcKillerClient.has_value())
            {
                cship->clear_equip_and_cargo();
                return;
            }
            int repId = npcKillerClient.value().GetReputation().Unwrap().GetValue();
            uint targetAffiliation;
            float attitude;
            Reputation::Vibe::GetAffiliation(cship->repVibe, targetAffiliation, false);
            pub::Reputation::GetGroupFeelingsTowards(repId, targetAffiliation, attitude);

            if (attitude > config.maximumRepForLoot)
            {
                cship->clear_equip_and_cargo();
                return;
            }
        }

        CEquipTraverser tr((uint)EquipmentClass::Cargo);
        CECargo* cargo = nullptr;
        while (cargo = reinterpret_cast<CECargo*>(cship->equipManager.Traverse(tr)))
        {
            auto lootIter = lootData.find(cargo->archetype->archId.GetValue());

            const LootData& ld = lootIter == lootData.end() ? LootData() : lootIter->second;

            uint amountToDrop;
            if (cship->ownerPlayer)
            {
                amountToDrop = ld.maxDropPlayer;
            }
            else
            {
                amountToDrop = ld.maxDropNPC;
            }

            if (ld.dropChanceUnmounted < 1.0f)
            {
                float roll = static_cast<float>(rand()) / RAND_MAX;
                if (roll > ld.dropChanceUnmounted)
                {
                    continue;
                }
            }

            if (!amountToDrop)
            {
                continue;
            }

            Vector dropPos = cship->position;
            Vector randomVector = RandomVector(Random::UniformFloat(20.f,80.f));
            dropPos += (glm::vec<3, float, glm::packed_highp>)randomVector;

            ResourceManager::CreateLootSimple(
                cship->system, dropPos, cargo->archetype->archId, std::min(cargo->count, amountToDrop), ShipId(cship->id.GetValue()), false);
        }

        cship->clear_equip_and_cargo();
    }

    void MarketControllerPlugin::OnShipColGrpDestroy(Ship* ship, CArchGroup* colGrp, DamageEntry::SubObjFate, DamageList*)
    {
        CShip* cship = ship->cship();
        const auto shipColGrpInfo = colGrpCargoMap.find(cship->archetype->archId.GetValue());
        if (shipColGrpInfo == colGrpCargoMap.end())
        {
            return;
        }

        const auto colGrpInfo = shipColGrpInfo->second.second.find(colGrp->colGrp->id);
        if (colGrpInfo == shipColGrpInfo->second.second.end())
        {
            return;
        }

        float cargoRemaining = cship->get_cargo_hold_remaining();

        if (cargoRemaining > colGrpInfo->second)
        {
            return;
        }

        uint id = ship->get_id();
        auto dropIter = dropMap.find(id);
        if (dropIter == dropMap.end())
        {
            dropMap[id] = colGrpInfo->second - cargoRemaining;
        }
        else
        {
            dropIter->second += colGrpInfo->second - cargoRemaining;
        }
    }

    void MarketControllerPlugin::OnShipDropAllCargo(Ship* ship, const char* hardPoint, DamageList* dmgList)
    {
        if (config.enableCustomLootDrops)
        {
            returnCode = ReturnCode::SkipFunctionCall;
        }
    }

    Task MarketControllerPlugin::AdminCmdReloadPrices(const ClientId client)
    {
        OnLoadSettings();
        co_return TaskStatus::Finished;
    }
} // namespace Plugins

using namespace Plugins;

DefaultDllMain();

const PluginInfo Info(L"MarketController", L"market_cntl", PluginMajorVersion::V05, PluginMinorVersion::V00);
SetupPlugin(MarketControllerPlugin, Info);
