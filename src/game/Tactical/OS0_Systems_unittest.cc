#include "gtest/gtest.h"

#include "OS0_ActionRegistry.h"
#include "OS0_AssetCatalogService.h"
#include "OS0_AssetDamageSystem.h"
#include "OS0_CarrySystem.h"
#include "OS0_CoverOrderSystem.h"
#include "OS0_CreatorModel.h"
#include "OS0_SectorEconomySystem.h"
#include "OS0_TacticalSession.h"
#include "OS0_UIRuntime.h"
#include "OS0_ViewportInput.h"
#include "OS0_WorldInteractionSystem.h"
#include "SaveLoadGameStates.h"

#include <vector>

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

	std::vector<ContextAction> const first = ResolveOS0CursorActions(facts);
	std::vector<ContextAction> const second = ResolveOS0CursorActions(facts);
	ASSERT_EQ(first, second);
	ASSERT_EQ(first.size(), 3u);
	EXPECT_EQ(first[0], ContextAction::ATTACK);
	EXPECT_EQ(first[1], ContextAction::INSPECT);
	EXPECT_EQ(first[2], ContextAction::MOVE);

	for (UINT8 value = 0; value < static_cast<UINT8>(ContextAction::COUNT);
		++value)
	{
		ContextAction const action = static_cast<ContextAction>(value);
		EXPECT_EQ(GetContextActionDescriptor(action).action, action);
		EXPECT_NE(ContextActionName(action), nullptr);
		EXPECT_NE(ContextActionExplanation(action), nullptr);
	}
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

	EXPECT_EQ(model.stats()[0], 55);
	EXPECT_EQ(model.points(), 100);
	EXPECT_TRUE(model.adjustStat(0, 1));
	EXPECT_EQ(model.stats()[0], 60);
	EXPECT_EQ(model.points(), 95);
	for (int i = 0; i < 20; ++i) model.adjustStat(0, -1);
	EXPECT_EQ(model.stats()[0], OS0CreatorModel::STAT_MIN);

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
	EXPECT_EQ(OS0CommandForDockSlot(7), OS0UICommand::SANDBOX);
	EXPECT_EQ(OS0CommandForDockSlot(8), OS0UICommand::COUNT);
}

TEST(OS0UILayoutTest, DockNeverMovesWithWorldAndWindowsStayAboveIt)
{
	OS0UILayout layout;
	layout.configure(1280, 720, 682);
	EXPECT_EQ(layout.worldBottom(), 682);
	EXPECT_EQ(layout.dock().y, 682);
	EXPECT_EQ(layout.dock().h, 38);
	EXPECT_EQ(layout.command(OS0UICommand::TACTICAL).x, 0);
	EXPECT_EQ(layout.command(OS0UICommand::SANDBOX).x, 1137);
	OS0UIRect const clamped = layout.clampWindow({ 1200, 690, 420, 184 });
	EXPECT_EQ(clamped.x, 860);
	EXPECT_EQ(clamped.y, 495);
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
	carry.reset();
	EXPECT_FALSE(carry.active());
	EXPECT_EQ(carry.source, NOWHERE);
}

TEST(OS0TacticalSessionTest, PersistsSimulationAndClearsOnlyTransientState)
{
	OS0TacticalSession source;
	source.state().creatorCompleted = TRUE;
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

	source.endTacticalSector();
	EXPECT_TRUE(source.state().coverOrders.orders().empty());
	EXPECT_FALSE(source.state().carry.active());
	EXPECT_EQ(source.state().assetDamage.durability(key, 160), 135);

	SavedGameStates states;
	source.storePersistentState(states);
	SavedGameStates roundTrip;
	roundTrip.Deserialize(states.Serialize());
	OS0TacticalSession loaded;
	loaded.loadPersistentState(roundTrip);

	EXPECT_TRUE(loaded.state().creatorCompleted);
	EXPECT_EQ(loaded.state().assetDamage.durability(key, 160), 135);
	EXPECT_EQ(loaded.state().sectorEconomy.resource(sector,
		OS0ResourceKind::SCRAP), 777);
	EXPECT_TRUE(loaded.state().sectorEconomy.hasUpgrade(sector,
		OS0_SECTOR_UPGRADE_WORKSHOP));
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
