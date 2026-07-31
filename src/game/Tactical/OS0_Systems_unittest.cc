#include "gtest/gtest.h"

#include "OS0_ActionRegistry.h"
#include "OS0_AssetCatalogService.h"
#include "OS0_AssetDamageSystem.h"
#include "OS0_CarrySystem.h"
#include "OS0_CoverOrderSystem.h"
#include "OS0_CreatorModel.h"
#include "OS0_FieldTutorial.h"
#include "OS0_ItemRelations.h"
#include "OS0_ItemTransferController.h"
#include "OS0_RealtimeEditor.h"
#include "OS0_SectorEconomySystem.h"
#include "OS0_TacticalSession.h"
#include "OS0_UIRuntime.h"
#include "OS0_ViewportInput.h"
#include "OS0_WindowManager.h"
#include "OS0_WorldInteractionSystem.h"
#include "SaveLoadGameStates.h"

TEST(OS0ItemTransferPolicyTest, StaysSilentUntilAnAccessibleTargetExists)
{
	ItemTransferPolicyInput input;
	input.carryingItem = TRUE;
	input.allowed.fill(TRUE);
	EXPECT_TRUE(ResolveItemTransferPolicy(input).actions.empty());

	input.targetAvailable = TRUE;
	EXPECT_TRUE(ResolveItemTransferPolicy(input).actions.empty());

	input.targetAccessible = TRUE;
	EXPECT_EQ(ResolveItemTransferPolicy(input).actions.size(), 5u);
}

TEST(OS0ItemTransferPolicyTest, ExposesOnlyValidRelationsAndSelectsSemanticDefault)
{
	ItemTransferPolicyInput armour;
	armour.carryingItem = TRUE;
	armour.targetAvailable = TRUE;
	armour.targetAccessible = TRUE;
	armour.allowed[static_cast<size_t>(ItemTransferIntent::BODY)] = TRUE;
	armour.allowed[static_cast<size_t>(ItemTransferIntent::PACK)] = TRUE;
	armour.allowed[static_cast<size_t>(ItemTransferIntent::DROP)] = TRUE;

	ItemTransferPolicyDecision const decision = ResolveItemTransferPolicy(armour);
	ASSERT_EQ(decision.actions.size(), 3u);
	EXPECT_TRUE(decision.allows(ItemTransferIntent::BODY));
	EXPECT_FALSE(decision.allows(ItemTransferIntent::PRIMARY_HAND));
	ASSERT_TRUE(decision.hasPreferred);
	EXPECT_EQ(decision.preferred, ItemTransferIntent::BODY);
	EXPECT_FALSE(decision.safeToApplyAutomatically);

	armour.allowed[static_cast<size_t>(ItemTransferIntent::BODY)] = FALSE;
	ItemTransferPolicyDecision const packOnly = ResolveItemTransferPolicy(armour);
	ASSERT_TRUE(packOnly.hasPreferred);
	EXPECT_EQ(packOnly.preferred, ItemTransferIntent::PACK);
	EXPECT_TRUE(packOnly.safeToApplyAutomatically);
}

TEST(OS0ItemTransferControllerTest, OneGestureHasOneSourceAndOneReleaseTarget)
{
	OS0ItemTransferController transfers;
	EXPECT_TRUE(transfers.beginSourcePress(OS0ItemTransferSurface::LOOT,
		42, 100, 100));
	EXPECT_FALSE(transfers.beginSourcePress(OS0ItemTransferSurface::INVENTORY,
		7, 100, 100));
	EXPECT_FALSE(transfers.dragThresholdReached(OS0ItemTransferSurface::LOOT,
		42, 103, 103));
	EXPECT_TRUE(transfers.dragThresholdReached(OS0ItemTransferSurface::LOOT,
		42, 104, 100));
	EXPECT_TRUE(transfers.markItemHeld(OS0ItemTransferSurface::LOOT, 42));

	EXPECT_EQ(transfers.claimRelease(OS0ItemTransferSurface::INVENTORY),
		OS0ItemReleaseClaim::ITEM);
	EXPECT_EQ(transfers.claimRelease(OS0ItemTransferSurface::WORLD),
		OS0ItemReleaseClaim::NONE);
	transfers.completeItemRelease(FALSE);
	EXPECT_EQ(transfers.phase(), OS0ItemTransferPhase::IDLE);
	EXPECT_TRUE(transfers.consumeHandledRelease());
	EXPECT_FALSE(transfers.consumeHandledRelease());
}

TEST(OS0ItemTransferControllerTest, HeldItemCanSurviveAContextChoice)
{
	OS0ItemTransferController transfers;
	transfers.adoptExternalHeldItem();
	EXPECT_TRUE(transfers.beginHeldGesture());
	EXPECT_EQ(transfers.claimRelease(OS0ItemTransferSurface::WORLD),
		OS0ItemReleaseClaim::ITEM);
	transfers.completeItemRelease(TRUE);
	EXPECT_TRUE(transfers.itemHeld());
	EXPECT_FALSE(transfers.ownsPhysicalGesture());

	EXPECT_TRUE(transfers.beginHeldGesture());
	EXPECT_EQ(transfers.claimRelease(OS0ItemTransferSurface::RELATION),
		OS0ItemReleaseClaim::ITEM);
	transfers.completeItemRelease(FALSE);
	EXPECT_FALSE(transfers.itemHeld());
}

TEST(OS0ItemTransferControllerTest, AClickNeverBecomesALateDrag)
{
	OS0ItemTransferController transfers;
	ASSERT_TRUE(transfers.beginSourcePress(OS0ItemTransferSurface::INVENTORY,
		11, 25, 25));
	EXPECT_EQ(transfers.claimRelease(OS0ItemTransferSurface::INVENTORY),
		OS0ItemReleaseClaim::SOURCE_CLICK);
	EXPECT_FALSE(transfers.markItemHeld(OS0ItemTransferSurface::INVENTORY, 11));
	EXPECT_TRUE(transfers.releaseWasHandled());
}

TEST(OS0ItemTransferControllerTest, PointerCreatedOnButtonUpConsumesThatRelease)
{
	OS0ItemTransferController transfers;
	transfers.adoptExternalHeldItemAfterHandledRelease();
	EXPECT_TRUE(transfers.itemHeld());
	EXPECT_FALSE(transfers.ownsPhysicalGesture());
	EXPECT_TRUE(transfers.consumeHandledRelease());
	EXPECT_TRUE(transfers.beginHeldGesture());
	EXPECT_EQ(transfers.claimRelease(OS0ItemTransferSurface::WORLD),
		OS0ItemReleaseClaim::ITEM);
}

TEST(OS0ItemTransferControllerTest, DoubleClickSuppressesTheActualUpNotTheDown)
{
	OS0ItemTransferController transfers;
	ASSERT_TRUE(transfers.beginSourcePress(OS0ItemTransferSurface::LOOT,
		21, 10, 10));
	transfers.cancelGestureAndConsumeRelease();
	EXPECT_TRUE(transfers.waitingForSuppressedRelease());
	EXPECT_FALSE(transfers.consumeHandledRelease());
	EXPECT_TRUE(transfers.consumeSuppressedRelease());
	EXPECT_FALSE(transfers.waitingForSuppressedRelease());
	EXPECT_TRUE(transfers.consumeHandledRelease());
}

TEST(OS0ItemTransferControllerTest, NewPhysicalDownEndsThePreviousReleaseLatch)
{
	OS0ItemTransferController transfers;
	transfers.adoptExternalHeldItemAfterHandledRelease();
	ASSERT_TRUE(transfers.releaseWasHandled());
	transfers.observePrimaryDown();
	EXPECT_FALSE(transfers.releaseWasHandled());
	EXPECT_TRUE(transfers.beginHeldGesture());
}

TEST(OS0ItemTransferControllerTest, FocusLossCannotLeaveAGestureCaptured)
{
	OS0ItemTransferController transfers;
	transfers.adoptExternalHeldItem();
	ASSERT_TRUE(transfers.beginHeldGesture());
	EXPECT_FALSE(transfers.recoverLostRelease(TRUE, TRUE));
	EXPECT_TRUE(transfers.ownsPhysicalGesture());
	EXPECT_TRUE(transfers.recoverLostRelease(FALSE, TRUE));
	EXPECT_FALSE(transfers.ownsPhysicalGesture());
	EXPECT_TRUE(transfers.itemHeld());
	EXPECT_TRUE(transfers.consumeHandledRelease());

	ASSERT_TRUE(transfers.beginHeldGesture());
	transfers.cancelGestureAndConsumeRelease();
	EXPECT_TRUE(transfers.recoverLostRelease(FALSE, FALSE));
	EXPECT_FALSE(transfers.waitingForSuppressedRelease());
}

TEST(OS0CoverOrderSystemTest, KeepsIndependentOrdersPerSoldier)
{
	OS0CoverOrderSystem system;
	system.issue({ 1, 101, 0, OS0CoverStance::CROUCH });
	system.issue({ 2, 202, 1, OS0CoverStance::PRONE });

	ASSERT_EQ(system.orders().size(), 2u);
	ASSERT_NE(system.find(1), nullptr);
	ASSERT_NE(system.find(2), nullptr);
	EXPECT_EQ(system.find(1)->destination, 101);
	EXPECT_EQ(system.find(2)->destination, 202);

	system.issue({ 1, 303, 0, OS0CoverStance::AUTO });
	EXPECT_EQ(system.orders().size(), 2u);
	EXPECT_EQ(system.find(1)->destination, 303);
	EXPECT_TRUE(system.cancel(1));
	EXPECT_EQ(system.find(1), nullptr);
	EXPECT_NE(system.find(2), nullptr);
}

TEST(OS0AssetDamageSystemTest, SeparatesSectorsAndLevelsAndMovesDamage)
{
	OS0AssetDamageSystem system;
	OS0AssetKey const ground{ 9, 1, 0, 1000, 0, 50 };
	OS0AssetKey const roof{ 9, 1, 0, 1000, 1, 50 };
	OS0AssetKey const moved{ 9, 1, 0, 1001, 0, 50 };

	OS0AssetDamageResult const hit = system.apply(ground, 100, 30);
	EXPECT_TRUE(hit.handled);
	EXPECT_FALSE(hit.destroyed);
	EXPECT_EQ(system.durability(ground, 100), 70);
	EXPECT_EQ(system.durability(roof, 100), 100);

	system.move(ground, moved);
	EXPECT_EQ(system.durability(ground, 100), 100);
	EXPECT_EQ(system.durability(moved, 100), 70);
	EXPECT_TRUE(system.apply(moved, 100, 80).destroyed);
}

TEST(OS0ActionRegistryTest, OneDeterministicResolverDrivesCursorSurfaces)
{
	OS0ActionFacts facts;
	facts.hasTarget = TRUE;
	facts.hostileTarget = TRUE;
	facts.armed = TRUE;

	OS0InteractionContext context;
	context.cursor = facts;
	OS0ResolvedActionList const first = ResolveOS0InteractionActions(context);
	OS0ResolvedActionList const second = ResolveOS0InteractionActions(context);
	ASSERT_EQ(first.size(), 3u);
	ASSERT_EQ(first.size(), second.size());
	for (std::size_t i = 0; i < first.size(); ++i)
	{
		EXPECT_EQ(first[i].action, second[i].action);
		EXPECT_EQ(first[i].enabled, second[i].enabled);
		EXPECT_EQ(first[i].approach, second[i].approach);
	}
	EXPECT_EQ(first[0].action, ContextAction::ATTACK);
	EXPECT_EQ(first[1].action, ContextAction::INSPECT);
	EXPECT_EQ(first[2].action, ContextAction::MOVE);

	for (UINT8 value = 0; value < static_cast<UINT8>(ContextAction::COUNT);
		++value)
	{
		ContextAction const action = static_cast<ContextAction>(value);
		ContextActionDescriptor const& descriptor =
			GetContextActionDescriptor(action);
		EXPECT_EQ(descriptor.action, action);
		ASSERT_NE(descriptor.name, nullptr);
		ASSERT_NE(descriptor.explanation, nullptr);
		EXPECT_NE(descriptor.name[0], '\0');
		EXPECT_NE(descriptor.explanation[0], '\0');
		EXPECT_NE(descriptor.icon, OS0UIIcon::COUNT);
		EXPECT_LT(descriptor.category, ActionCategory::COUNT);
		EXPECT_EQ(ContextActionName(action), descriptor.name);
		EXPECT_EQ(ContextActionExplanation(action), descriptor.explanation);
	}

	for (UINT8 value = 0; value < static_cast<UINT8>(ActionCategory::COUNT);
		++value)
	{
		ActionCategory const category = static_cast<ActionCategory>(value);
		ActionCategoryDescriptor const& descriptor =
			GetActionCategoryDescriptor(category);
		EXPECT_EQ(descriptor.category, category);
		ASSERT_NE(descriptor.name, nullptr);
		ASSERT_NE(descriptor.explanation, nullptr);
		EXPECT_NE(descriptor.name[0], '\0');
		EXPECT_NE(descriptor.explanation[0], '\0');
		EXPECT_NE(descriptor.icon, OS0UIIcon::COUNT);
		EXPECT_EQ(ActionCategoryName(category), descriptor.name);
	}

	EXPECT_STREQ(ActionCategoryName(ActionCategory::DEBUG), "GOD");
	EXPECT_EQ(ContextActionCategory(ContextAction::PREVIOUS_SQUAD),
		ActionCategory::GROUP);
	EXPECT_EQ(ContextActionCategory(ContextAction::NEXT_SQUAD),
		ActionCategory::GROUP);
	EXPECT_EQ(ContextActionCategory(ContextAction::TEAM), ActionCategory::GROUP);
	EXPECT_EQ(ContextActionCategory(ContextAction::END_TURN),
		ActionCategory::GROUP);
	EXPECT_EQ(ContextActionCategory(ContextAction::GOD_ASSETS),
		ActionCategory::DEBUG);
	EXPECT_EQ(ContextActionCategory(ContextAction::GOD_EDITOR),
		ActionCategory::DEBUG);
	EXPECT_EQ(ContextActionCategory(ContextAction::GOD_ICONS),
		ActionCategory::DEBUG);
	EXPECT_EQ(ContextActionCategory(ContextAction::GOD_TOOLS),
		ActionCategory::DEBUG);
	EXPECT_EQ(ContextActionCategory(ContextAction::GOD_REVIVE),
		ActionCategory::DEBUG);
}

TEST(OS0ActionRegistryTest, EnvironmentCapabilitiesDriveEveryObjectSurface)
{
	OS0EnvironmentActionFacts facts;
	facts.hasAsset = TRUE;
	facts.near = TRUE;
	facts.moveCandidate = TRUE;
	facts.canMove = TRUE;
	facts.canThrow = FALSE;
	facts.salvageable = TRUE;
	facts.canSalvage = FALSE;
	facts.debugCatalog = TRUE;

	OS0InteractionContext context;
	context.hasEnvironment = TRUE;
	context.environment = facts;
	context.environment.actorAvailable = TRUE;
	context.target.kind = OS0InteractionTargetKind::WORLD_ASSET;
	OS0ResolvedActionList const actions =
		ResolveOS0InteractionActions(context);
	auto enabled = [&](ContextAction action)
	{
		auto const found = std::find_if(actions.begin(), actions.end(),
			[action](auto const& entry) { return entry.action == action; });
		return found != actions.end() && found->enabled;
	};
	EXPECT_TRUE(enabled(ContextAction::CARRY));
	EXPECT_TRUE(enabled(ContextAction::PUSH));
	EXPECT_TRUE(enabled(ContextAction::PULL));
	EXPECT_FALSE(enabled(ContextAction::THROW));
	EXPECT_FALSE(enabled(ContextAction::SALVAGE));
	EXPECT_TRUE(OS0IsManipulationAction(ContextAction::THROW));
}

TEST(OS0ActionRegistryTest, MaximumEnvironmentRelationFitsInlineStorage)
{
	OS0InteractionContext context;
	context.hasEnvironment = TRUE;
	context.target.kind = OS0InteractionTargetKind::WORLD_ASSET;
	context.environment.actorAvailable = TRUE;
	context.environment.hasAsset = TRUE;
	context.environment.hasItems = TRUE;
	context.environment.openable = TRUE;
	context.environment.terrain = TRUE;
	context.environment.near = TRUE;
	context.environment.moveCandidate = TRUE;
	context.environment.canMove = TRUE;
	context.environment.canThrow = TRUE;
	context.environment.salvageable = TRUE;
	context.environment.canSalvage = TRUE;
	context.environment.diggableSurface = TRUE;
	context.environment.canDig = TRUE;
	context.environment.buildable = TRUE;
	context.environment.debugCatalog = TRUE;

	OS0ResolvedActionList const actions =
		ResolveOS0InteractionActions(context);
	EXPECT_EQ(actions.size(), OS0ResolvedActionList::CAPACITY);
	EXPECT_NE(FindOS0ResolvedAction(actions, ContextAction::CONTENTS), nullptr);
	EXPECT_NE(FindOS0ResolvedAction(actions, ContextAction::CATALOG), nullptr);
	EXPECT_NE(FindOS0ResolvedAction(actions, ContextAction::MOVE), nullptr);
}

TEST(OS0ActionRegistryTest, RelationalResolverBindsTargetAndPlansApproach)
{
	OS0InteractionContext context;
	context.hasEnvironment = TRUE;
	context.target = { OS0InteractionTargetKind::WORLD_ASSET, -1, 1234, 0,
		77, -1 };
	context.environment.actorAvailable = TRUE;
	context.environment.hasAsset = TRUE;
	context.environment.openable = TRUE;
	context.environment.near = FALSE;

	OS0ResolvedActionList const actions =
		ResolveOS0InteractionActions(context);
	OS0ResolvedAction const* const contents =
		FindOS0ResolvedAction(actions, ContextAction::CONTENTS);
	ASSERT_NE(contents, nullptr);
	EXPECT_TRUE(contents->enabled);
	EXPECT_EQ(contents->approach, OS0ActionApproach::MOVE_TO_RANGE);
	EXPECT_EQ(contents->binding, context.target);
	ASSERT_NE(PrimaryOS0InteractionAction(actions), nullptr);
	EXPECT_EQ(PrimaryOS0InteractionAction(actions)->action,
		ContextAction::CONTENTS);
}

TEST(OS0ActionRegistryTest, RelationalResolverExplainsBlockedToolAction)
{
	OS0InteractionContext context;
	context.hasEnvironment = TRUE;
	context.target = { OS0InteractionTargetKind::TERRAIN, -1, 1234, 0,
		0xffff, -1 };
	context.environment.actorAvailable = TRUE;
	context.environment.terrain = TRUE;
	context.environment.diggableSurface = TRUE;
	context.environment.canDig = FALSE;

	OS0ResolvedActionList const actions =
		ResolveOS0InteractionActions(context);
	OS0ResolvedAction const* const dig =
		FindOS0ResolvedAction(actions, ContextAction::DIG);
	ASSERT_NE(dig, nullptr);
	EXPECT_FALSE(dig->enabled);
	EXPECT_EQ(dig->approach, OS0ActionApproach::IMPOSSIBLE);
	EXPECT_EQ(dig->blockReason, OS0ActionBlockReason::MISSING_TOOL);
	EXPECT_STREQ(OS0ActionBlockReasonName(dig->blockReason), "MISSING TOOL");
}

TEST(OS0ActionRegistryTest, HostileArmedRelationDefaultsToBoundAttack)
{
	OS0InteractionContext context;
	context.target = { OS0InteractionTargetKind::ACTOR, 9, 456, 0,
		0xffff, -1 };
	context.cursor.hasTarget = TRUE;
	context.cursor.hostileTarget = TRUE;
	context.cursor.armed = TRUE;

	OS0ResolvedActionList const actions =
		ResolveOS0InteractionActions(context);
	OS0ResolvedAction const* const primary =
		PrimaryOS0InteractionAction(actions);
	ASSERT_NE(primary, nullptr);
	EXPECT_EQ(primary->action, ContextAction::ATTACK);
	EXPECT_EQ(primary->binding, context.target);
}

TEST(OS0ViewportGestureStateTest, ConsumesOnlyMatchedAndHandledReleases)
{
	OS0ViewportGestureState gestures;
	EXPECT_FALSE(gestures.releaseRight());
	gestures.armRight();
	EXPECT_TRUE(gestures.releaseRight());
	EXPECT_FALSE(gestures.releaseRight());

	EXPECT_FALSE(gestures.releaseMiddle());
	gestures.armMiddle();
	EXPECT_TRUE(gestures.releaseMiddle());
	EXPECT_FALSE(gestures.releaseMiddle());

	gestures.beginPrimary();
	gestures.markHeldItemReleaseHandled();
	EXPECT_TRUE(gestures.consumeHeldItemRelease());
	EXPECT_FALSE(gestures.consumeHeldItemRelease());
	gestures.markHeldItemReleaseHandled();
	gestures.beginPrimary();
	EXPECT_FALSE(gestures.consumeHeldItemRelease());

	gestures.beginPrimary(TRUE);
	EXPECT_TRUE(gestures.ownsPrimary());
	EXPECT_TRUE(gestures.consumePrimaryGesture());
	EXPECT_TRUE(gestures.releasePrimary());
	EXPECT_FALSE(gestures.ownsPrimary());
	EXPECT_TRUE(gestures.consumePrimaryGesture());
	EXPECT_FALSE(gestures.consumePrimaryGesture());
	EXPECT_FALSE(gestures.releasePrimary());
}

TEST(OS0CreatorModelTest, OwnsValidatedIdentityStatsAndTraitSelection)
{
	OS0CreatorModel model;
	EXPECT_TRUE(model.callsign().empty());
	for (char32_t character : U"Reirao<> Escape from Arulco")
		model.appendCallsign(character);
	EXPECT_EQ(model.callsign(), "Reirao Escape fr");
	EXPECT_TRUE(model.backspaceCallsign());
	EXPECT_EQ(model.callsign(), "Reirao Escape f");

	const std::array<INT8, OS0CreatorModel::STAT_COUNT> expectedStats{{
		85, 85, 85, 85, 85, 35, 85, 35, 35, 35
	}};
	EXPECT_EQ(model.stats(), expectedStats);
	EXPECT_EQ(model.points(), 0);
	EXPECT_FALSE(model.adjustStat(0, 1));
	EXPECT_TRUE(model.adjustStat(0, -1));
	EXPECT_EQ(model.stats()[0], 80);
	EXPECT_EQ(model.points(), 5);
	for (int i = 0; i < 20; ++i) model.adjustStat(0, -1);
	EXPECT_EQ(model.stats()[0], OS0CreatorModel::STAT_MIN);

	EXPECT_EQ(model.bodyType(), REGMALE);
	EXPECT_TRUE(model.selectBodyType(REGFEMALE));
	EXPECT_EQ(model.bodyType(), REGFEMALE);
	EXPECT_FALSE(model.selectBodyType(COW));

	EXPECT_TRUE(model.toggleTrait(STEALTHY));
	EXPECT_TRUE(model.toggleTrait(NIGHTOPS));
	EXPECT_EQ(model.traits()[0], STEALTHY);
	EXPECT_EQ(model.traits()[1], NIGHTOPS);
	EXPECT_TRUE(model.toggleTrait(AUTO_WEAPS));
	EXPECT_EQ(model.traits()[0], NIGHTOPS);
	EXPECT_EQ(model.traits()[1], AUTO_WEAPS);
	EXPECT_TRUE(model.toggleTrait(NIGHTOPS));
	EXPECT_EQ(model.traits()[0], AUTO_WEAPS);
	EXPECT_EQ(model.traits()[1], NO_SKILLTRAIT);
}

TEST(OS0UIRuntimeTest, CreatorHasOneLinearFlowAndClosesBeforeGameplay)
{
	OS0UIRuntime ui;
	EXPECT_TRUE(ui.creatorActive());
	EXPECT_EQ(ui.creatorStage(), OS0CreatorStage::WELCOME);
	EXPECT_TRUE(ui.visible(OS0UIPanel::INVENTORY));

	EXPECT_FALSE(ui.advanceCreator());
	EXPECT_EQ(ui.creatorStage(), OS0CreatorStage::IDENTITY);
	EXPECT_FALSE(ui.advanceCreator());
	EXPECT_EQ(ui.creatorStage(), OS0CreatorStage::ATTRIBUTES);
	EXPECT_FALSE(ui.advanceCreator());
	EXPECT_EQ(ui.creatorStage(), OS0CreatorStage::TRAITS);
	EXPECT_FALSE(ui.advanceCreator());
	EXPECT_EQ(ui.creatorStage(), OS0CreatorStage::CONTROLS);
	EXPECT_TRUE(ui.advanceCreator());
	EXPECT_FALSE(ui.creatorActive());
	EXPECT_FALSE(ui.visible(OS0UIPanel::INVENTORY));
	EXPECT_EQ(ui.creatorStage(), OS0CreatorStage::COMPLETE);
}

TEST(OS0UIRuntimeTest, PanelsAreIndependentAndDockMappingIsStable)
{
	OS0UIRuntime ui;
	ui.enterCampaign(TRUE);
	ui.show(OS0UIPanel::INVENTORY);
	ui.show(OS0UIPanel::CONTEXT);
	ui.hideTransientWorldPanels();
	EXPECT_TRUE(ui.visible(OS0UIPanel::INVENTORY));
	EXPECT_FALSE(ui.visible(OS0UIPanel::CONTEXT));
	EXPECT_EQ(OS0CommandForDockSlot(0), OS0UICommand::CHARACTER);
	EXPECT_EQ(GetOS0UICommandDescriptor(OS0UICommand::TACTICAL).intent,
		OS0UICommandIntent::TOGGLE_COMBAT_MODE);
	EXPECT_EQ(GetOS0UICommandDescriptor(OS0UICommand::CHARACTER).intent,
		OS0UICommandIntent::OPEN_CHARACTER_HUB);
	EXPECT_EQ(OS0CommandForDockSlot(7), OS0UICommand::SANDBOX);
	EXPECT_EQ(OS0CommandForDockSlot(8), OS0UICommand::COUNT);
}

TEST(OS0UIRegistryTest, CommandsWindowsAndArtworkHaveCompleteSemanticMetadata)
{
	for (UINT8 value = 0; value < static_cast<UINT8>(OS0UICommand::COUNT);
		++value)
	{
		OS0UICommand const command = static_cast<OS0UICommand>(value);
		OS0UICommandDescriptor const& descriptor =
			GetOS0UICommandDescriptor(command);
		EXPECT_EQ(descriptor.command, command);
		EXPECT_NE(descriptor.label, nullptr);
		EXPECT_NE(descriptor.tooltip, nullptr);
		EXPECT_NE(descriptor.icon, OS0UIIcon::COUNT);
	}

	OS0UIRuntime ui;
	ui.enterCampaign(TRUE);
	for (UINT8 value = 0; value < static_cast<UINT8>(OS0UIWindow::COUNT);
		++value)
	{
		OS0UIWindow const window = static_cast<OS0UIWindow>(value);
		OS0UIWindowDescriptor const& descriptor =
			GetOS0UIWindowDescriptor(window);
		EXPECT_EQ(descriptor.window, window);
		EXPECT_NE(descriptor.persistenceKey, nullptr);
		EXPECT_EQ(OS0UIWindowFromPersistenceKey(descriptor.persistenceKey), window);
		EXPECT_EQ(ui.window(window).visible, descriptor.defaultVisible);
	}
	EXPECT_EQ(OS0UIWindowFromPersistenceKey("obsolete-window"),
		OS0UIWindow::COUNT);

	for (UINT8 value = 0; value < static_cast<UINT8>(OS0UIIcon::COUNT);
		++value)
	{
		OS0UIIcon const icon = static_cast<OS0UIIcon>(value);
		OS0UIIconDescriptor const& descriptor = GetOS0UIIconDescriptor(icon);
		EXPECT_EQ(descriptor.icon, icon);
		EXPECT_NE(descriptor.label, nullptr);
		EXPECT_LT(descriptor.frame,
			GetOS0UIAssetSheetDescriptor(descriptor.sheet).minimumFrames);
	}
}

TEST(OS0UIRuntimeTest, OwnsPanelGeometryDragAndVisibilityTogether)
{
	OS0UIRuntime ui;
	ui.enterCampaign(TRUE);
	OS0UIWindowState& inventory = ui.panel(OS0UIPanel::INVENTORY);
	inventory.x = 123;
	inventory.y = 45;
	inventory.dragging = TRUE;
	ui.show(OS0UIPanel::INVENTORY);
	EXPECT_EQ(ui.panel(OS0UIPanel::INVENTORY).x, 123);
	EXPECT_EQ(ui.panel(OS0UIPanel::INVENTORY).y, 45);
	EXPECT_TRUE(ui.panel(OS0UIPanel::INVENTORY).dragging);
	EXPECT_TRUE(ui.visible(OS0UIPanel::INVENTORY));
}

TEST(OS0WindowManagerTest, TemplatesShareDragClampAndZOrder)
{
	OS0WindowManager windows;
	windows.setWorkspace({ 0, 0, 320, 200 });
	ASSERT_TRUE(windows.registerTemplate({ 1, "inventory", "Inventory",
		OS0UIIcon::HAND, OS0WindowPresentation::FLOATING,
		{ 20, 30, 140, 90 }, 80, 50,
		OS0_WINDOW_MOVABLE | OS0_WINDOW_BLOCKS_WORLD_INPUT |
		OS0_WINDOW_PERSIST_POSITION, TRUE, 1 }));
	ASSERT_TRUE(windows.registerTemplate({ 2, "editor", "World editor",
		OS0UIIcon::TOOLKIT, OS0WindowPresentation::FLOATING,
		{ 170, 10, 120, 150 }, 90, 80,
		OS0_WINDOW_MOVABLE | OS0_WINDOW_BLOCKS_WORLD_INPUT |
		OS0_WINDOW_PERSIST_POSITION, TRUE, 2 }));

	EXPECT_EQ(windows.hitTest(180, 40), 2);
	ASSERT_TRUE(windows.beginDrag(1, 25, 35));
	EXPECT_TRUE(windows.dragTo(400, 300));
	EXPECT_TRUE(windows.endDrag());
	EXPECT_EQ(windows.bounds(1).x, 180);
	EXPECT_EQ(windows.bounds(1).y, 110);
	EXPECT_EQ(windows.hitTest(200, 120), 1);
	EXPECT_TRUE(windows.blocksWorldInputAt(200, 120));
}

TEST(OS0WindowManagerTest, SuspensionDoesNotDestroyRequestedVisibility)
{
	OS0WindowManager windows;
	ASSERT_TRUE(windows.registerTemplate({ 3, "inspector", "Inspector",
		OS0UIIcon::EXAMINE, OS0WindowPresentation::FLOATING,
		{ 10, 10, 100, 80 }, 40, 30,
		OS0_WINDOW_MOVABLE | OS0_WINDOW_COLLAPSE_DURING_AIM,
		TRUE, 1 }));
	EXPECT_TRUE(windows.requestedVisible(3));
	EXPECT_TRUE(windows.visible(3));
	windows.setSuspended(OS0WindowSuspendReason::AIM, TRUE);
	EXPECT_TRUE(windows.requestedVisible(3));
	EXPECT_FALSE(windows.visible(3));
	windows.setSuspended(OS0WindowSuspendReason::MODAL, TRUE);
	windows.setSuspended(OS0WindowSuspendReason::AIM, FALSE);
	EXPECT_FALSE(windows.visible(3));
	EXPECT_TRUE(windows.requestedVisible(3));
	windows.setSuspended(OS0WindowSuspendReason::MODAL, FALSE);
	EXPECT_TRUE(windows.visible(3));
}

TEST(OS0WindowManagerTest, RadialsUseTheirAnchorAndModalsBlockWorkspace)
{
	OS0WindowManager windows;
	windows.setWorkspace({ 0, 0, 640, 440 });
	ASSERT_TRUE(windows.registerTemplate({ 5, "context", "Context",
		OS0UIIcon::TARGET, OS0WindowPresentation::RADIAL,
		{ 320, 200, 184, 184 }, 80, 80,
		OS0_WINDOW_BLOCKS_WORLD_INPUT, TRUE, 10 }));
	EXPECT_EQ(windows.hitTest(235, 115), 5);
	EXPECT_EQ(windows.hitTest(220, 100), OS0_INVALID_WINDOW);

	ASSERT_TRUE(windows.registerTemplate({ 6, "modal", "Modal",
		OS0UIIcon::LOOK, OS0WindowPresentation::MODAL,
		{ 250, 170, 140, 100 }, 80, 60,
		OS0_WINDOW_BLOCKS_WORLD_INPUT, TRUE, 20 }));
	EXPECT_TRUE(windows.blocksWorldInputAt(5, 5));
}

TEST(OS0WindowManagerTest, LayoutRoundTripScalesReusableTemplates)
{
	OS0WindowManager source;
	source.setWorkspace({ 0, 0, 640, 442 });
	ASSERT_TRUE(source.registerTemplate({ 4, "sector", "Sector",
		OS0UIIcon::LOOK, OS0WindowPresentation::FLOATING,
		{ 100, 80, 200, 100 }, 80, 40,
		OS0_WINDOW_PERSIST_POSITION, FALSE, 1 }));
	source.setBounds(4, { 320, 200, 200, 100 });
	ST::string const serialized = source.serializeLayout(640, 480);

	OS0WindowManager restored;
	restored.setWorkspace({ 0, 0, 1280, 884 });
	ASSERT_TRUE(restored.registerTemplate({ 4, "sector", "Sector",
		OS0UIIcon::LOOK, OS0WindowPresentation::FLOATING,
		{ 0, 0, 200, 100 }, 80, 40,
		OS0_WINDOW_PERSIST_POSITION, FALSE, 1 }));
	ASSERT_TRUE(restored.restoreLayout(serialized, 1280, 960));
	EXPECT_EQ(restored.bounds(4).x, 640);
	EXPECT_EQ(restored.bounds(4).y, 400);
	EXPECT_EQ(restored.bounds(4).w, 400);
	EXPECT_EQ(restored.bounds(4).h, 200);
}

TEST(OS0RealtimeEditorSessionTest, QueuesTypedCommandsAndFlagsWorldSwap)
{
	OS0RealtimeEditorSession editor;
	EXPECT_EQ(editor.pendingCount(), 0u);
	EXPECT_FALSE(editor.hasPendingWorldSwap());
	EXPECT_FALSE(editor.willInvalidateWorldPointers());

	OS0EditorTilePlacement tile;
	tile.gridNo = 101;
	tile.tileIndex = 23;
	auto const tileCommand = editor.queuePlaceTile(tile);
	EXPECT_NE(tileCommand, 0u);
	EXPECT_EQ(editor.pendingCount(), 1u);
	EXPECT_FALSE(editor.willInvalidateWorldPointers());

	OS0EditorBlankMapRequest blank;
	blank.tileset = 2;
	auto const blankCommand = editor.queueNewBlankMap(blank);
	EXPECT_GT(blankCommand, tileCommand);
	EXPECT_EQ(editor.pendingCount(), 2u);
	EXPECT_TRUE(editor.hasPendingWorldSwap());
	EXPECT_TRUE(editor.willInvalidateWorldPointers());

	OS0EditorSaveRequest save;
	save.name = "queue-test";
	auto const saveCommand = editor.queueSave(save);
	EXPECT_GT(saveCommand, blankCommand);
	EXPECT_EQ(editor.pendingCount(), 3u);
	EXPECT_TRUE(editor.willInvalidateWorldPointers());

	OS0EditorLoadRequest load;
	load.name = "queue-test";
	auto const loadCommand = editor.queueLoad(load);
	EXPECT_GT(loadCommand, saveCommand);
	EXPECT_EQ(editor.pendingCount(), 4u);
	EXPECT_TRUE(editor.hasPendingWorldSwap());
}

TEST(OS0UILayoutTest, FloatingMultitoolDoesNotReserveWorldSpace)
{
	OS0UILayout layout;
	layout.configure(1280, 720, 720);
	EXPECT_EQ(layout.worldBottom(), 720);
	EXPECT_EQ(layout.workspaceBottom(), 720);
	EXPECT_EQ(layout.dock().y, 682);
	EXPECT_EQ(layout.dock().h, 38);
	EXPECT_EQ(layout.command(OS0UICommand::TACTICAL).x, 0);
	EXPECT_EQ(layout.command(OS0UICommand::SANDBOX).x, 1137);
	OS0UIRect const clamped = layout.clampWindow({ 1200, 690, 420, 184 });
	EXPECT_EQ(clamped.x, 860);
	EXPECT_EQ(clamped.y, 536);
}

TEST(OS0SectorEconomySystemTest, MigratesLegacyOnceAndClampsResources)
{
	OS0SectorEconomySystem economy;
	OS0SectorKey const sector{ 9, 1, 0 };
	UINT32 const legacy = (7u << 8) | (12u << 13) | (31u << 18) |
		(4u << 23) | 0x10000000u | 0x40000000u;
	economy.migrateLegacy(sector, legacy);

	EXPECT_EQ(economy.resource(sector, OS0ResourceKind::TIMBER), 7);
	EXPECT_EQ(economy.resource(sector, OS0ResourceKind::STONE), 12);
	EXPECT_EQ(economy.resource(sector, OS0ResourceKind::SCRAP), 31);
	EXPECT_EQ(economy.resource(sector, OS0ResourceKind::SOIL), 4);
	EXPECT_TRUE(economy.hasUpgrade(sector, OS0_SECTOR_UPGRADE_SHELTER));
	EXPECT_FALSE(economy.hasUpgrade(sector, OS0_SECTOR_UPGRADE_WORKSHOP));
	EXPECT_TRUE(economy.hasUpgrade(sector, OS0_SECTOR_UPGRADE_DEPOT));

	economy.setResource(sector, OS0ResourceKind::TIMBER, 65000);
	economy.addResource(sector, OS0ResourceKind::TIMBER, 65000);
	EXPECT_EQ(economy.resource(sector, OS0ResourceKind::TIMBER),
		OS0_SECTOR_RESOURCE_MAX);
	economy.migrateLegacy(sector, 0);
	EXPECT_EQ(economy.resource(sector, OS0ResourceKind::TIMBER),
		OS0_SECTOR_RESOURCE_MAX);
}

TEST(OS0WorldInteractionSystemTest, DepositsAndBuildsAtomically)
{
	OS0SectorEconomySystem economy;
	OS0SectorKey const sector{ 9, 1, 0 };
	EXPECT_FALSE(OS0DepositResources(economy, sector,
		OS0ResourceKind::COUNT, 1));
	EXPECT_FALSE(OS0DepositResources(economy, sector,
		OS0ResourceKind::TIMBER, 0));
	ASSERT_TRUE(OS0DepositResources(economy, sector,
		OS0ResourceKind::TIMBER, 8));
	ASSERT_TRUE(OS0DepositResources(economy, sector,
		OS0ResourceKind::STONE, 4));
	ASSERT_TRUE(OS0DepositResources(economy, sector,
		OS0ResourceKind::SOIL, 4));
	EXPECT_TRUE(OS0CanBuildSectorUpgrade(economy, sector, 0));
	ASSERT_TRUE(OS0BuildSectorUpgrade(economy, sector, 0));
	EXPECT_EQ(economy.resource(sector, OS0ResourceKind::TIMBER), 0);
	EXPECT_EQ(economy.resource(sector, OS0ResourceKind::STONE), 0);
	EXPECT_EQ(economy.resource(sector, OS0ResourceKind::SOIL), 0);
	EXPECT_TRUE(economy.hasUpgrade(sector, OS0_SECTOR_UPGRADE_SHELTER));
	EXPECT_FALSE(OS0BuildSectorUpgrade(economy, sector, 0));

	economy.setResource(sector, OS0ResourceKind::SCRAP,
		OS0_SECTOR_RESOURCE_MAX);
	EXPECT_FALSE(OS0DepositResources(economy, sector,
		OS0ResourceKind::SCRAP, 1));
}

TEST(OS0AssetCatalogServiceTest, LaterOverrideReplacesBuiltInRecord)
{
	std::vector<OS0AssetCatalogRecord> records;
	OS0MergeAssetCatalogText(
		"1 100 1 1 1 2 3 0 BUILTIN DOOR\n", records);
	OS0MergeAssetCatalogText(
		"1 100 3 2 2 4 5 1 USER CRATE\n", records);

	ASSERT_EQ(records.size(), 1u);
	EXPECT_EQ(records[0].category, OS0AssetCategory::CONTAINER);
	EXPECT_EQ(records[0].material, OS0AssetMaterial::STONE);
	EXPECT_EQ(records[0].role, OS0AssetRole::STORAGE);
	EXPECT_EQ(records[0].width, 4);
	EXPECT_EQ(records[0].height, 5);
	EXPECT_TRUE(records[0].buildable);
	EXPECT_EQ(records[0].label, "USER CRATE");
}

TEST(OS0CarryStateTest, ValidatesBeginWalkAndCancelLifecycle)
{
	OS0CarryState carry;
	EXPECT_FALSE(carry.begin(NOWHERE, 0, 100, 1));
	EXPECT_FALSE(carry.active());
	EXPECT_FALSE(carry.begin(1000, 2, 100, 1));
	EXPECT_FALSE(carry.begin(1000, 0, 100, NOBODY));

	ASSERT_TRUE(carry.begin(1000, 0, 100, 1));
	EXPECT_TRUE(carry.pending());
	EXPECT_FALSE(carry.beginWalk(1000, 0, 1001));
	EXPECT_FALSE(carry.beginWalk(NOWHERE, 0, 1001));
	ASSERT_TRUE(carry.beginWalk(1002, 0, 1001));
	EXPECT_TRUE(carry.walking());
	EXPECT_EQ(OS0ValidateCarryContinuation(carry, {}),
		OS0CarryCancelReason::NONE);
	OS0CarryContinuationFacts dead;
	dead.carrierAlive = FALSE;
	EXPECT_EQ(OS0ValidateCarryContinuation(carry, dead),
		OS0CarryCancelReason::CARRIER_DEAD);
	OS0CarryContinuationFacts sectorChange;
	sectorChange.sameSector = FALSE;
	EXPECT_EQ(OS0ValidateCarryContinuation(carry, sectorChange),
		OS0CarryCancelReason::SECTOR_CHANGED);
	OS0CarryContinuationFacts pathFailure;
	pathFailure.pathValid = FALSE;
	EXPECT_EQ(OS0ValidateCarryContinuation(carry, pathFailure),
		OS0CarryCancelReason::PATH_FAILED);
	OS0CarryContinuationFacts objectChanged;
	objectChanged.objectAvailable = FALSE;
	EXPECT_EQ(OS0ValidateCarryContinuation(carry, objectChanged),
		OS0CarryCancelReason::OBJECT_CHANGED);
	carry.reset();
	EXPECT_FALSE(carry.active());
	EXPECT_EQ(carry.source, NOWHERE);
}

TEST(OS0CarryStateTest, RetainsSelectedPhysicalHandlingMode)
{
	OS0CarryState carry;
	ASSERT_TRUE(carry.begin(1000, 0, 50, 1, OS0CarryMode::PULL));
	EXPECT_EQ(carry.mode, OS0CarryMode::PULL);
	ASSERT_TRUE(carry.beginWalk(1001, 0, 999));
	EXPECT_EQ(carry.mode, OS0CarryMode::PULL);
	carry.reset();
	EXPECT_EQ(carry.mode, OS0CarryMode::CARRY);
}

TEST(OS0CarryStateTest, PersistentGrabSurvivesPhysicalStepSelection)
{
	OS0CarryState carry;
	ASSERT_TRUE(carry.begin(1000, 0, 50, 1, OS0CarryMode::GRAB));
	EXPECT_TRUE(carry.persistentGrab);
	EXPECT_EQ(carry.mode, OS0CarryMode::GRAB);
	carry.mode = OS0CarryMode::PUSH;
	ASSERT_TRUE(carry.beginWalk(1001, 0, 1000));
	EXPECT_TRUE(carry.persistentGrab);
	EXPECT_EQ(carry.mode, OS0CarryMode::PUSH);
	carry.reset();
	EXPECT_FALSE(carry.persistentGrab);
}

TEST(OS0TacticalSessionTest, PersistsSimulationAndClearsOnlyTransientState)
{
	OS0TacticalSession source;
	source.state().creatorCompleted = TRUE;
	source.state().fieldTutorialCompleted = TRUE;
	OS0AssetKey const key{ 9, 1, 0, 1000, 1, 50 };
	OS0SectorKey const sector{ 9, 1, 0 };
	source.state().assetDamage.apply(key, 160, 25);
	source.state().sectorEconomy.setResource(sector,
		OS0ResourceKind::SCRAP, 777);
	source.state().sectorEconomy.addUpgrade(sector,
		OS0_SECTOR_UPGRADE_WORKSHOP);
	source.state().coverOrders.issue({ 1, 1001, 0,
		OS0CoverStance::CROUCH });
	ASSERT_TRUE(source.state().carry.begin(1000, 0, 50, 1));
	source.state().cursor.action = ContextAction::ATTACK;

	source.endTacticalSector();
	EXPECT_TRUE(source.state().coverOrders.orders().empty());
	EXPECT_FALSE(source.state().carry.active());
	EXPECT_EQ(source.state().cursor.action, ContextAction::MOVE);
	EXPECT_EQ(source.state().assetDamage.durability(key, 160), 135);

	SavedGameStates states;
	source.storePersistentState(states);
	SavedGameStates roundTrip;
	roundTrip.Deserialize(states.Serialize());
	OS0TacticalSession loaded;
	loaded.state().coverOrders.issue({ 9, 2222, 0,
		OS0CoverStance::PRONE });
	ASSERT_TRUE(loaded.state().carry.begin(2222, 0, 55, 1));
	loaded.state().cursor.action = ContextAction::ATTACK;
	loaded.state().pendingVisualEvents.push_back({ 2222,
		OS0AssetMaterial::STONE, 1 });
	loaded.state().pendingDiagnostics.push_back("stale diagnostic");
	loaded.loadPersistentState(roundTrip);

	EXPECT_TRUE(loaded.state().creatorCompleted);
	EXPECT_TRUE(loaded.state().fieldTutorialCompleted);
	EXPECT_TRUE(loaded.state().coverOrders.orders().empty());
	EXPECT_FALSE(loaded.state().carry.active());
	EXPECT_EQ(loaded.state().cursor.action, ContextAction::MOVE);
	EXPECT_TRUE(loaded.state().pendingVisualEvents.empty());
	EXPECT_TRUE(loaded.state().pendingDiagnostics.empty());
	EXPECT_EQ(loaded.state().assetDamage.durability(key, 160), 135);
	EXPECT_EQ(loaded.state().sectorEconomy.resource(sector,
		OS0ResourceKind::SCRAP), 777);
	EXPECT_TRUE(loaded.state().sectorEconomy.hasUpgrade(sector,
		OS0_SECTOR_UPGRADE_WORKSHOP));
}

TEST(OS0FieldTutorialTest, VerifiesOneRelationalContainerFlow)
{
	OS0FieldTutorial tutorial;
	EXPECT_TRUE(tutorial.notify(OS0FieldTutorialEvent::BEGIN));
	EXPECT_EQ(tutorial.stage(), OS0FieldTutorialStage::ACQUIRE_CONTAINER);
	EXPECT_TRUE(tutorial.notify(OS0FieldTutorialEvent::CONTAINER_ASSIGNED));
	EXPECT_EQ(tutorial.stage(), OS0FieldTutorialStage::HOVER_CONTAINER);
	EXPECT_TRUE(tutorial.notify(OS0FieldTutorialEvent::CONTAINER_HOVERED));
	EXPECT_EQ(tutorial.stage(), OS0FieldTutorialStage::OPEN_ACTIONS);
	EXPECT_TRUE(tutorial.notify(OS0FieldTutorialEvent::ACTIONS_OPENED));
	EXPECT_EQ(tutorial.stage(), OS0FieldTutorialStage::SELECT_CONTENTS);
	EXPECT_TRUE(tutorial.notify(OS0FieldTutorialEvent::CONTENTS_SELECTED));
	EXPECT_EQ(tutorial.stage(), OS0FieldTutorialStage::APPROACH_CONTAINER);
	EXPECT_FALSE(tutorial.notify(OS0FieldTutorialEvent::APPROACH_STARTED));
	EXPECT_TRUE(tutorial.notify(OS0FieldTutorialEvent::CONTENTS_OPENED));
	EXPECT_EQ(tutorial.stage(), OS0FieldTutorialStage::LOOT_CONTAINER);
	EXPECT_TRUE(tutorial.notify(OS0FieldTutorialEvent::ITEM_TAKEN));
	EXPECT_TRUE(tutorial.completed());
}

TEST(OS0TacticalSessionTest, RejectsMalformedOrFuturePersistentState)
{
	SavedGameStates malformed;
	malformed.Set("escape-from-arulco:state-version", 2);
	malformed.SetVector("escape-from-arulco:asset-damage-v1",
		std::vector<int32_t>{ 9, 1, 0 });
	OS0TacticalSession loaded;
	loaded.loadPersistentState(malformed);
	EXPECT_TRUE(loaded.state().creatorCompleted);
	EXPECT_TRUE(loaded.state().assetDamage.records().empty());

	SavedGameStates future;
	future.Set("escape-from-arulco:state-version", 999);
	future.SetVector("escape-from-arulco:sector-economy-v1",
		std::vector<int32_t>{ 9, 1, 0, 1, 2, 3, 4, 0, 1 });
	loaded.loadPersistentState(future);
	EXPECT_TRUE(loaded.state().sectorEconomy.records().empty());
}

TEST(OS0RealtimeEditorRecipeTest, BrushResolverClipsWithoutRowWrapping)
{
	std::vector<INT16> const corner = OS0ResolveEditorBrushGridNos(0, 1);
	EXPECT_EQ(corner, (std::vector<INT16>{ 0, 1, WORLD_COLS,
		WORLD_COLS + 1 }));

	INT16 const center = static_cast<INT16>(WORLD_COLS + 1);
	std::vector<INT16> const square = OS0ResolveEditorBrushGridNos(center, 1);
	ASSERT_EQ(square.size(), 9u);
	EXPECT_EQ(square.front(), 0);
	EXPECT_EQ(square.back(), static_cast<INT16>(WORLD_COLS * 2 + 2));
	EXPECT_TRUE(OS0ResolveEditorBrushGridNos(-1, 2).empty());
	EXPECT_TRUE(OS0ResolveEditorBrushGridNos(WORLD_MAX, 2).empty());
}

TEST(OS0RealtimeEditorRecipeTest, RoadGuardRejectsUnsafeLegacyOffsets)
{
	INT16 const firstSafe = static_cast<INT16>(5 * WORLD_COLS + 5);
	EXPECT_TRUE(OS0EditorRoadMacroFitsWorld(firstSafe, 0));
	EXPECT_TRUE(OS0EditorRoadMacroFitsWorld(firstSafe, 31));
	EXPECT_FALSE(OS0EditorRoadMacroFitsWorld(firstSafe, 32));
	EXPECT_FALSE(OS0EditorRoadMacroFitsWorld(
		static_cast<INT16>(4 * WORLD_COLS + 5), 0));
	EXPECT_FALSE(OS0EditorRoadMacroFitsWorld(
		static_cast<INT16>(5 * WORLD_COLS + 4), 0));
	EXPECT_FALSE(OS0EditorRoadMacroFitsWorld(
		static_cast<INT16>((WORLD_ROWS - 1) * WORLD_COLS + 5), 0));
}
