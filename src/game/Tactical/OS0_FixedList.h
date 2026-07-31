#pragma once

#include <array>
#include <cassert>
#include <cstddef>

// Small, allocation-free sequence for bounded tactical/UI results. OS//0 uses
// these lists in per-frame paths where the engine already defines a strict
// upper bound and heap-backed vectors only add allocator traffic.
template<typename T, std::size_t Capacity>
class OS0FixedList
{
public:
	using value_type = T;
	using iterator = value_type*;
	using const_iterator = value_type const*;
	static constexpr std::size_t CAPACITY = Capacity;

	bool empty() const noexcept { return m_size == 0; }
	std::size_t size() const noexcept { return m_size; }
	iterator begin() noexcept { return m_entries.data(); }
	iterator end() noexcept { return begin() + m_size; }
	const_iterator begin() const noexcept { return m_entries.data(); }
	const_iterator end() const noexcept { return begin() + m_size; }
	value_type& operator[](std::size_t index) noexcept
	{
		assert(index < m_size);
		return m_entries[index];
	}
	value_type const& operator[](std::size_t index) const noexcept
	{
		assert(index < m_size);
		return m_entries[index];
	}
	void push_back(value_type const& entry) noexcept
	{
		assert(m_size < Capacity);
		if (m_size < Capacity) m_entries[m_size++] = entry;
	}

private:
	std::array<value_type, Capacity> m_entries{};
	std::size_t m_size = 0;
};
