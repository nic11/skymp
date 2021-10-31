#include "TES5DamageFormula.h"

#include "HitData.h"
#include "MpActor.h"
#include "WorldState.h"
#include <espm.h>

namespace {

bool IsUnarmedAttack(const uint32_t sourceFormId)
{
  return sourceFormId == 0x1f4;
}

class TES5DamageFormulaImpl
{
public:
  TES5DamageFormulaImpl(const MpActor& aggressor_, const MpActor& target_,
                        const HitData& hitData_);

  float CalculateDamage() const;

private:
  const MpActor& aggressor;
  const MpActor& target;
  const HitData& hitData;
  WorldState* espmProvider;

private:
  float GetBaseWeaponDamage() const;
  float CalcWeaponRating() const;
  float CalcArmorRatingComponent(
    const Inventory::Entry& opponentEquipmentEntry) const;
  float CalcOpponentArmorRating() const;
};

TES5DamageFormulaImpl::TES5DamageFormulaImpl(const MpActor& aggressor_,
                                             const MpActor& target_,
                                             const HitData& hitData_)
  : aggressor(aggressor_)
  , target(target_)
  , hitData(hitData_)
  , espmProvider(aggressor.GetParent())
{
}

float TES5DamageFormulaImpl::GetBaseWeaponDamage() const
{
  auto weapData = espm::GetData<espm::WEAP>(hitData.source, espmProvider);
  if (!weapData.weapData) {
    throw std::runtime_error(
      fmt::format("no weapData for {:#x}", hitData.source));
  }
  return weapData.weapData->damage;
}

float TES5DamageFormulaImpl::CalcWeaponRating() const
{
  // TODO(#xyz): take other components into account
  return GetBaseWeaponDamage();
}

float TES5DamageFormulaImpl::CalcArmorRatingComponent(
  const Inventory::Entry& opponentEquipmentEntry) const
{
  if (opponentEquipmentEntry.extra.worn != Inventory::Worn::None &&
      espm::GetRecordType(opponentEquipmentEntry.baseId, espmProvider) ==
        espm::ARMO::type) {
    auto armorData =
      espm::GetData<espm::ARMO>(opponentEquipmentEntry.baseId, espmProvider);
    // TODO(#xyz): take other components into account
    return static_cast<float>(armorData.baseRatingX100) / 100;
  }
  return 0;
}

float TES5DamageFormulaImpl::CalcOpponentArmorRating() const
{
  // TODO(#xyz): OpponentArmorRating is 1 if your character is successfully
  // sneaking and has the Master Sneak perk (C) UESP Wiki
  // ^ at least for Oblivion
  float combinedArmorRating = 0;
  for (const auto& entry : target.GetEquipment().inv.entries) {
    combinedArmorRating += CalcArmorRatingComponent(entry);
  }
  return combinedArmorRating;
}

float TES5DamageFormulaImpl::CalculateDamage() const
{
  if (IsUnarmedAttack(hitData.source)) {
    uint32_t raceId = aggressor.GetRaceId();
    return espm::GetData<espm::RACE>(raceId, espmProvider).unarmedDamage;
  }

  float incomingDamage = CalcWeaponRating();
  float maxArmorRating =
    espm::GetData<espm::GMST>(espm::GMST::kFArmorRating, espmProvider).value;
  float minReceivedDamage = incomingDamage * (1 - 0.01 * maxArmorRating);

  // TODO(#xyz): take other components into account
  float damage =
    std::max<float>(minReceivedDamage,
                    incomingDamage / (CalcOpponentArmorRating() * 0.12 + 1));
  if (hitData.isPowerAttack) {
    damage *= 2;
  }
  if (hitData.isHitBlocked) {
    // TODO(#xyz): implement correct block formula
    damage *= 0.1;
  }
  return damage;
}

}

float TES5DamageFormula::CalculateDamage(const MpActor& aggressor,
                                         const MpActor& target,
                                         const HitData& hitData) const
{
  return TES5DamageFormulaImpl(aggressor, target, hitData).CalculateDamage();
}
