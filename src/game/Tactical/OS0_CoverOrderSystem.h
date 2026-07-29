#pragma once

#include "Soldier_Control.h"

#include <vector>

enum class OS0CoverStance : UINT8
{
	AUTO,
	CROUCH,
	PRONE
};

struct OS0CoverOrder
{
	SoldierID soldier;
	GridNo destination;
	UINT8 level;
	OS0CoverStance desiredStance;
};

class OS0CoverOrderSystem
{
public:
	void issue(OS0CoverOrder order);
	BOOLEAN cancel(SoldierID soldier);
	void clear();
	OS0CoverOrder const* find(SoldierID soldier) const;
	std::vector<OS0CoverOrder> const& orders() const noexcept;

private:
	std::vector<OS0CoverOrder> orders_;
};
