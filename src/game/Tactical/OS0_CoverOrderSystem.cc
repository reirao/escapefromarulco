#include "OS0_CoverOrderSystem.h"

#include <algorithm>

void OS0CoverOrderSystem::issue(OS0CoverOrder order)
{
	auto const existing = std::find_if(orders_.begin(), orders_.end(),
		[&](OS0CoverOrder const& value) { return value.soldier == order.soldier; });
	if (existing == orders_.end()) orders_.push_back(order);
	else *existing = order;
}

BOOLEAN OS0CoverOrderSystem::cancel(SoldierID soldier)
{
	auto const oldSize = orders_.size();
	orders_.erase(std::remove_if(orders_.begin(), orders_.end(),
		[&](OS0CoverOrder const& value) { return value.soldier == soldier; }),
		orders_.end());
	return orders_.size() != oldSize;
}

void OS0CoverOrderSystem::clear()
{
	orders_.clear();
}

OS0CoverOrder const* OS0CoverOrderSystem::find(SoldierID soldier) const
{
	auto const found = std::find_if(orders_.begin(), orders_.end(),
		[&](OS0CoverOrder const& value) { return value.soldier == soldier; });
	return found == orders_.end() ? nullptr : &*found;
}

std::vector<OS0CoverOrder> const& OS0CoverOrderSystem::orders() const noexcept
{
	return orders_;
}
