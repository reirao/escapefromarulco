#include "OS0_FieldTutorial.h"

BOOLEAN OS0FieldTutorial::notify(OS0FieldTutorialEvent const event) noexcept
{
	const OS0FieldTutorialStage before = stage_;
	switch (event)
	{
		case OS0FieldTutorialEvent::BEGIN:
			if (stage_ == OS0FieldTutorialStage::DISABLED)
				stage_ = OS0FieldTutorialStage::ACQUIRE_CONTAINER;
			break;
		case OS0FieldTutorialEvent::CONTAINER_ASSIGNED:
			if (stage_ == OS0FieldTutorialStage::ACQUIRE_CONTAINER)
				stage_ = OS0FieldTutorialStage::HOVER_CONTAINER;
			break;
		case OS0FieldTutorialEvent::CONTAINER_HOVERED:
			if (stage_ == OS0FieldTutorialStage::HOVER_CONTAINER)
				stage_ = OS0FieldTutorialStage::OPEN_ACTIONS;
			break;
		case OS0FieldTutorialEvent::ACTIONS_OPENED:
			if (stage_ == OS0FieldTutorialStage::HOVER_CONTAINER ||
				stage_ == OS0FieldTutorialStage::OPEN_ACTIONS)
				stage_ = OS0FieldTutorialStage::SELECT_CONTENTS;
			break;
		case OS0FieldTutorialEvent::CONTENTS_SELECTED:
			if (stage_ == OS0FieldTutorialStage::SELECT_CONTENTS)
				stage_ = OS0FieldTutorialStage::APPROACH_CONTAINER;
			break;
		case OS0FieldTutorialEvent::APPROACH_STARTED:
			if (stage_ == OS0FieldTutorialStage::SELECT_CONTENTS ||
				stage_ == OS0FieldTutorialStage::APPROACH_CONTAINER)
				stage_ = OS0FieldTutorialStage::APPROACH_CONTAINER;
			break;
		case OS0FieldTutorialEvent::APPROACH_ABORTED:
			if (stage_ == OS0FieldTutorialStage::APPROACH_CONTAINER)
				stage_ = OS0FieldTutorialStage::SELECT_CONTENTS;
			break;
		case OS0FieldTutorialEvent::CONTENTS_OPENED:
			if (stage_ == OS0FieldTutorialStage::HOVER_CONTAINER ||
				stage_ == OS0FieldTutorialStage::OPEN_ACTIONS ||
				stage_ == OS0FieldTutorialStage::SELECT_CONTENTS ||
				stage_ == OS0FieldTutorialStage::APPROACH_CONTAINER)
				stage_ = OS0FieldTutorialStage::LOOT_CONTAINER;
			break;
		case OS0FieldTutorialEvent::ITEM_TAKEN:
			if (stage_ == OS0FieldTutorialStage::LOOT_CONTAINER)
				stage_ = OS0FieldTutorialStage::COMPLETE;
			break;
		case OS0FieldTutorialEvent::TARGET_LOST:
			if (!completed() && active())
				stage_ = OS0FieldTutorialStage::ACQUIRE_CONTAINER;
			break;
		case OS0FieldTutorialEvent::DISMISS:
			stage_ = OS0FieldTutorialStage::DISABLED;
			break;
	}
	return before != stage_;
}

void OS0FieldTutorial::reset(BOOLEAN const alreadyCompleted) noexcept
{
	stage_ = alreadyCompleted ? OS0FieldTutorialStage::DISABLED :
		OS0FieldTutorialStage::ACQUIRE_CONTAINER;
}

const char* OS0FieldTutorial::heading() const noexcept
{
	switch (stage_)
	{
		case OS0FieldTutorialStage::DISABLED: return "";
		case OS0FieldTutorialStage::ACQUIRE_CONTAINER:
			return "FIELD TEST / FINDING CONTAINER";
		case OS0FieldTutorialStage::HOVER_CONTAINER:
			return "1 / PERCEIVE THE WORLD";
		case OS0FieldTutorialStage::OPEN_ACTIONS:
			return "2 / OPEN OBJECT ACTIONS";
		case OS0FieldTutorialStage::SELECT_CONTENTS:
			return "3 / CHOOSE A RELATION";
		case OS0FieldTutorialStage::APPROACH_CONTAINER:
			return "4 / PHYSICAL APPROACH";
		case OS0FieldTutorialStage::LOOT_CONTAINER:
			return "5 / REAL CONTENTS";
		case OS0FieldTutorialStage::COMPLETE:
			return "FIELD TEST COMPLETE";
	}
	return "";
}

const char* OS0FieldTutorial::instruction() const noexcept
{
	switch (stage_)
	{
		case OS0FieldTutorialStage::DISABLED: return "";
		case OS0FieldTutorialStage::ACQUIRE_CONTAINER:
			return "Scanning this sector for a usable crate...";
		case OS0FieldTutorialStage::HOVER_CONTAINER:
			return "Move the pointer onto the red-marked crate.";
		case OS0FieldTutorialStage::OPEN_ACTIONS:
			return "Press F (or RMB) while the crate is under the pointer.";
		case OS0FieldTutorialStage::SELECT_CONTENTS:
			return "Click CONTENTS. Distance is solved by the action itself.";
		case OS0FieldTutorialStage::APPROACH_CONTAINER:
			return "The operator is approaching. WASD cancels this order.";
		case OS0FieldTutorialStage::LOOT_CONTAINER:
			return "Double-click one item, or drag it to a body slot.";
		case OS0FieldTutorialStage::COMPLETE:
			return "Hover -> relation -> approach -> contents -> inventory verified.";
	}
	return "";
}
