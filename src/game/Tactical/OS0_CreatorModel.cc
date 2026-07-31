#include "OS0_CreatorModel.h"

#include <algorithm>

OS0CreatorModel::OS0CreatorModel()
{
	reset();
}

void OS0CreatorModel::reset()
{
	callsign_.clear();
	// Balanced survival preset requested for the first playable build. It uses
	// exactly the same 650-point budget as the old 55 + 100 unspent setup.
	stats_ = {{ 85, 85, 85, 85, 85, 35, 85, 35, 35, 35 }};
	points_ = 0;
	traits_.fill(NO_SKILLTRAIT);
	bodyType_ = REGMALE;
}

BOOLEAN OS0CreatorModel::appendCallsign(char32_t character)
{
	if (callsign_.to_utf32().size() >= CALLSIGN_LIMIT || character < U' ' ||
		character == U'<' || character == U'>') return FALSE;
	callsign_ += character;
	return TRUE;
}

BOOLEAN OS0CreatorModel::backspaceCallsign()
{
	ST::utf32_buffer const characters = callsign_.to_utf32();
	if (characters.empty()) return FALSE;
	callsign_ = ST::string::from_utf32(characters.data(), characters.size() - 1);
	return TRUE;
}

BOOLEAN OS0CreatorModel::adjustStat(size_t index, INT8 direction) noexcept
{
	if (index >= stats_.size() || direction == 0) return FALSE;
	INT8& value = stats_[index];
	if (direction > 0)
	{
		if (points_ < STAT_STEP || value > STAT_MAX - STAT_STEP) return FALSE;
		value += STAT_STEP;
		points_ -= STAT_STEP;
	}
	else
	{
		if (value < STAT_MIN + STAT_STEP) return FALSE;
		value -= STAT_STEP;
		points_ += STAT_STEP;
	}
	return TRUE;
}

BOOLEAN OS0CreatorModel::selectBodyType(
	SoldierBodyType const bodyType) noexcept
{
	if (bodyType != REGMALE && bodyType != BIGMALE &&
		bodyType != STOCKYMALE && bodyType != REGFEMALE)
		return FALSE;
	if (bodyType_ == bodyType) return FALSE;
	bodyType_ = bodyType;
	return TRUE;
}

BOOLEAN OS0CreatorModel::toggleTrait(SkillTrait trait) noexcept
{
	if (trait == NO_SKILLTRAIT) return FALSE;
	for (SkillTrait& selected : traits_)
	{
		if (selected != trait) continue;
		selected = NO_SKILLTRAIT;
		if (traits_[0] == NO_SKILLTRAIT && traits_[1] != NO_SKILLTRAIT)
			std::swap(traits_[0], traits_[1]);
		return TRUE;
	}
	for (SkillTrait& selected : traits_)
	{
		if (selected != NO_SKILLTRAIT) continue;
		selected = trait;
		return TRUE;
	}
	traits_[0] = traits_[1];
	traits_[1] = trait;
	return TRUE;
}
