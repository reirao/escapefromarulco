#pragma once

#include "JA2Types.h"
#include "Soldier_Profile_Type.h"

#include <array>
#include <cstddef>
#include <string_theory/string>

// Pure character-creation data. The tactical renderer displays this model and
// applies the accepted result to JA2's soldier/profile only once at each stage.
class OS0CreatorModel
{
public:
	static constexpr size_t STAT_COUNT = 10;
	static constexpr size_t TRAIT_COUNT = 2;
	static constexpr INT8 STAT_MIN = 35;
	static constexpr INT8 STAT_MAX = 85;
	static constexpr INT8 STAT_STEP = 5;
	static constexpr size_t CALLSIGN_LIMIT = 16;

	OS0CreatorModel();

	void reset();
	ST::string const& callsign() const noexcept { return callsign_; }
	BOOLEAN appendCallsign(char32_t character);
	BOOLEAN backspaceCallsign();

	INT16 points() const noexcept { return points_; }
	std::array<INT8, STAT_COUNT> const& stats() const noexcept { return stats_; }
	BOOLEAN adjustStat(size_t index, INT8 direction) noexcept;

	std::array<SkillTrait, TRAIT_COUNT> const& traits() const noexcept
	{
		return traits_;
	}
	BOOLEAN toggleTrait(SkillTrait trait) noexcept;

private:
	ST::string callsign_;
	INT16 points_ = 100;
	std::array<INT8, STAT_COUNT> stats_{};
	std::array<SkillTrait, TRAIT_COUNT> traits_{};
};
