#include "CraftingOrders.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "WorldSession.h"
#include <limits>
#include <memory>

namespace
{
    bool TableExists(DatabaseType& db, char const* table)
    {
        std::unique_ptr<QueryResult> result(db.PQuery("SHOW TABLES LIKE '%s'", table));
        return bool(result);
    }
}

bool CraftingOrders::HasPlayerRecipe(Player* player, uint32 professionId, uint32 spellId) const
{
    if (!player || !player->GetSession())
        return false;
    if (!TableExists(CharacterDatabase, "crafting_order_recipes"))
        return false;

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT 1 FROM crafting_order_recipes WHERE accountId = %u AND professionId = %u AND spellId = %u",
        player->GetSession()->GetAccountId(), professionId, spellId));
    return bool(result);
}

bool CraftingOrders::AddPlayerRecipe(Player* player, uint32 professionId, uint32 spellId)
{
    if (!player || !player->GetSession())
        return false;
    if (!TableExists(CharacterDatabase, "crafting_order_recipes"))
        return false;

    if (!CharacterDatabase.DirectPExecute(
        "INSERT IGNORE INTO crafting_order_recipes (accountId, professionId, spellId) VALUES (%u, %u, %u)",
        player->GetSession()->GetAccountId(), professionId, spellId))
        return false;
    return HasPlayerRecipe(player, professionId, spellId);
}

uint32 CraftingOrders::GetCooldownRemaining(Player* player, uint32 spellId) const
{
    if (!player || !player->GetSession() || !sCraftingOrdersConfig.EnforceCooldowns())
        return 0;
    if (!TableExists(CharacterDatabase, "crafting_order_cooldowns"))
        return 0;

    uint64 const now = uint64(time(nullptr));
    std::unique_ptr<QueryResult> result;
    if (sCraftingOrdersConfig.AccountWideCooldowns())
    {
        result.reset(CharacterDatabase.PQuery(
            "SELECT cooldownEndTime FROM crafting_order_cooldowns "
            "WHERE scopeType = %u AND scopeId = %u AND spellId = %u AND cooldownEndTime > " UI64FMTD,
            uint32(CraftingOrdersDomain::COOLDOWN_SCOPE_ACCOUNT),
            player->GetSession()->GetAccountId(), spellId, now));
    }
    else
    {
        result.reset(CharacterDatabase.PQuery(
            "SELECT cooldownEndTime FROM crafting_order_cooldowns "
            "WHERE scopeType = %u AND scopeId = %u AND spellId = %u AND cooldownEndTime > " UI64FMTD,
            uint32(CraftingOrdersDomain::COOLDOWN_SCOPE_CHARACTER),
            player->GetGUIDLow(), spellId, now));
    }

    if (!result)
        return 0;

    uint64 const cooldownEndTime = result->Fetch()[0].GetUInt64();
    if (cooldownEndTime <= now)
        return 0;

    uint64 const remaining = cooldownEndTime - now;
    return remaining > uint64(std::numeric_limits<uint32>::max())
        ? std::numeric_limits<uint32>::max()
        : uint32(remaining);
}

bool CraftingOrders::IsOnCooldown(Player* player, uint32 spellId) const
{
    return GetCooldownRemaining(player, spellId) > 0;
}

void CraftingOrders::SetCooldown(Player* player, uint32 spellId, uint32 cooldownSecs)
{
    if (!player || !player->GetSession() || !cooldownSecs)
        return;

    uint64 const endTime = uint64(time(nullptr)) + uint64(cooldownSecs);
    uint32 scopeType = sCraftingOrdersConfig.AccountWideCooldowns()
        ? uint32(CraftingOrdersDomain::COOLDOWN_SCOPE_ACCOUNT)
        : uint32(CraftingOrdersDomain::COOLDOWN_SCOPE_CHARACTER);
    uint32 scopeId = sCraftingOrdersConfig.AccountWideCooldowns()
        ? player->GetSession()->GetAccountId()
        : player->GetGUIDLow();

    CharacterDatabase.DirectPExecute(
        "REPLACE INTO crafting_order_cooldowns (scopeType, scopeId, spellId, cooldownEndTime) "
        "VALUES (%u, %u, %u, " UI64FMTD ")",
        scopeType, scopeId, spellId, endTime);
}

void CraftingOrders::CleanupExpiredCooldowns()
{
    if (!TableExists(CharacterDatabase, "crafting_order_cooldowns"))
        return;

    uint64 const now = uint64(time(nullptr));
    uint64 const maxEnd = now + (30ull * 24ull * 60ull * 60ull);
    CharacterDatabase.PExecute(
        "DELETE FROM crafting_order_cooldowns WHERE cooldownEndTime <= " UI64FMTD " OR cooldownEndTime > " UI64FMTD,
        now, maxEnd);
}
