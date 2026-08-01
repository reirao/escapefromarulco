#pragma once

#include "Types.h"

enum class OS0FieldTutorialStage : UINT8
{
	DISABLED,
	ACQUIRE_CONTAINER,
	HOVER_CONTAINER,
	OPEN_ACTIONS,
	SELECT_CONTENTS,
	APPROACH_CONTAINER,
	LOOT_CONTAINER,
	COMPLETE
};

enum class OS0FieldTutorialEvent : UINT8
{
	BEGIN,
	CONTAINER_ASSIGNED,
	CONTAINER_HOVERED,
	ACTIONS_OPENED,
	CONTENTS_SELECTED,
	APPROACH_STARTED,
	APPROACH_ABORTED,
	CONTENTS_OPENED,
	ITEM_TAKEN,
	TARGET_LOST,
	DISMISS
};

// Pure, monotonic tutorial model. Engine callbacks report facts; they never set
// presentation stages directly. This keeps F, RMB, approach and loot paths
// interchangeable while still proving that they reach the same action chain.
class OS0FieldTutorial
{
public:
	BOOLEAN notify(OS0FieldTutorialEvent event) noexcept;
	void reset(BOOLEAN alreadyCompleted = FALSE) noexcept;

	OS0FieldTutorialStage stage() const noexcept { return stage_; }
	BOOLEAN active() const noexcept
	{
		return stage_ != OS0FieldTutorialStage::DISABLED;
	}
	BOOLEAN completed() const noexcept
	{
		return stage_ == OS0FieldTutorialStage::COMPLETE;
	}
	const char* heading() const noexcept;
	const char* instruction() const noexcept;

private:
	OS0FieldTutorialStage stage_ = OS0FieldTutorialStage::DISABLED;
};
