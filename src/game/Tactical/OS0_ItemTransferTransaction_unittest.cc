#include "gtest/gtest.h"

#include "OS0_ItemTransferTransaction.h"


namespace
{
	constexpr OS0TransferredItemIdentity kRifle{ 17, 1001 };
	constexpr OS0TransferredItemIdentity kMedkit{ 44, 1002 };
}


TEST(OS0ItemTransferTransactionTest, InventoryCancelRestoresExactActorInstanceAndSlot)
{
	OS0ItemTransferTransaction transaction;
	OS0ItemTransferOrigin const source = OS0ItemTransferOrigin::Inventory(
		7, 9001, 3, kRifle);

	ASSERT_TRUE(transaction.begin(source));
	EXPECT_TRUE(transaction.held());
	EXPECT_EQ(transaction.origin(), source);
	ASSERT_TRUE(transaction.cancel());
	EXPECT_EQ(transaction.phase(), OS0ItemTransactionPhase::CANCELLED);

	OS0ItemRestorationDecision const restore =
		transaction.restorationDecision();
	ASSERT_TRUE(restore.required());
	EXPECT_EQ(restore.kind, OS0ItemRestorationKind::INVENTORY_SLOT);
	EXPECT_EQ(restore.origin, source);
	EXPECT_EQ(restore.origin.actorId, 7);
	EXPECT_EQ(restore.origin.actorInstanceId, 9001u);
	EXPECT_EQ(restore.origin.slot, 3);
	EXPECT_EQ(restore.origin.item, kRifle);
}


TEST(OS0ItemTransferTransactionTest, CommitIsTerminalAndForgetsSource)
{
	OS0ItemTransferTransaction transaction;
	ASSERT_TRUE(transaction.begin(OS0ItemTransferOrigin::World(
		1234, 1, -2, 0x2040, -3, kMedkit)));
	ASSERT_TRUE(transaction.commit());

	EXPECT_EQ(transaction.phase(), OS0ItemTransactionPhase::COMMITTED);
	EXPECT_FALSE(transaction.hasOrigin());
	EXPECT_EQ(transaction.origin().kind, OS0ItemTransferOriginKind::NONE);
	EXPECT_FALSE(transaction.restorationDecision().required());
	EXPECT_FALSE(transaction.cancel());
	EXPECT_FALSE(transaction.commit());
}


TEST(OS0ItemTransferTransactionTest, SpatialOriginsPreserveVisibilityAndKind)
{
	OS0ItemTransferTransaction world;
	OS0ItemTransferOrigin const worldSource = OS0ItemTransferOrigin::World(
		812, 0, -4, 0x0240, 7, kRifle);
	ASSERT_TRUE(world.begin(worldSource));
	ASSERT_TRUE(world.cancel());
	OS0ItemRestorationDecision const worldRestore =
		world.restorationDecision();
	EXPECT_EQ(worldRestore.kind, OS0ItemRestorationKind::WORLD_LOCATION);
	EXPECT_EQ(worldRestore.origin.gridNo, 812);
	EXPECT_EQ(worldRestore.origin.level, 0);
	EXPECT_EQ(worldRestore.origin.bVisible, -4);
	EXPECT_EQ(worldRestore.origin.usFlags, 0x0240);
	EXPECT_EQ(worldRestore.origin.bRenderZHeightAboveLevel, 7);
	EXPECT_EQ(worldRestore.origin.item, kRifle);

	OS0ItemTransferTransaction container;
	OS0ItemTransferOrigin const containerSource =
		OS0ItemTransferOrigin::Container(990, 1, 321, 1, 0x0040, -2,
			kMedkit, 4100, 990);
	ASSERT_TRUE(container.begin(containerSource));
	ASSERT_TRUE(container.cancel());
	OS0ItemRestorationDecision const containerRestore =
		container.restorationDecision();
	EXPECT_EQ(containerRestore.kind,
		OS0ItemRestorationKind::CONTAINER_LOCATION);
	EXPECT_EQ(containerRestore.origin, containerSource);
	EXPECT_EQ(containerRestore.origin.containerTileIndex, 321);
	EXPECT_EQ(containerRestore.origin.containerStructureId, 4100);
	EXPECT_EQ(containerRestore.origin.containerBaseGridNo, 990);
	EXPECT_EQ(containerRestore.origin.usFlags, 0x0040);
	EXPECT_EQ(containerRestore.origin.bRenderZHeightAboveLevel, -2);
}


TEST(OS0ItemTransferTransactionTest, FailedExactRestoreReturnsToHeldWithoutLosingOrigin)
{
	OS0ItemTransferTransaction transaction;
	OS0ItemTransferOrigin const source = OS0ItemTransferOrigin::Inventory(
		2, 8080, 5, kMedkit);
	ASSERT_TRUE(transaction.begin(source));
	ASSERT_TRUE(transaction.cancel());
	ASSERT_TRUE(transaction.resumeAfterFailedRestoration());

	EXPECT_TRUE(transaction.held());
	EXPECT_EQ(transaction.origin(), source);
	EXPECT_FALSE(transaction.restorationDecision().required());
	ASSERT_TRUE(transaction.cancel());
	ASSERT_TRUE(transaction.acknowledgeRestored());
	EXPECT_EQ(transaction.phase(), OS0ItemTransactionPhase::CANCELLED);
	EXPECT_FALSE(transaction.hasOrigin());
}


TEST(OS0ItemTransferTransactionTest, InvalidOrConcurrentOriginsAreRejected)
{
	OS0ItemTransferTransaction transaction;
	EXPECT_FALSE(transaction.begin({}));
	EXPECT_FALSE(transaction.begin(OS0ItemTransferOrigin::World(
		-1, 0, 1, 0, -1, kRifle)));
	EXPECT_FALSE(transaction.begin(OS0ItemTransferOrigin::Inventory(
		1, 0, 2, kRifle)));
	EXPECT_FALSE(transaction.begin(OS0ItemTransferOrigin::Inventory(
		1, 99, 2, {})));

	OS0ItemTransferOrigin const first = OS0ItemTransferOrigin::Inventory(
		1, 99, 2, kRifle);
	ASSERT_TRUE(transaction.begin(first));
	EXPECT_FALSE(transaction.begin(OS0ItemTransferOrigin::Container(
		400, 0, 10, 1, 0, -1, kMedkit, 4101, 400)));
	EXPECT_EQ(transaction.origin(), first);
}


TEST(OS0ItemTransferTransactionTest, ResetIsTheOnlyWayToReuseATerminalTransaction)
{
	OS0ItemTransferTransaction transaction;
	ASSERT_TRUE(transaction.begin(OS0ItemTransferOrigin::Container(
		400, 0, 10, 1, 0, -1, kMedkit, 4102, 400)));
	ASSERT_TRUE(transaction.commit());
	EXPECT_FALSE(transaction.begin(OS0ItemTransferOrigin::World(
		401, 0, 1, 0, -1, kRifle)));

	transaction.reset();
	EXPECT_EQ(transaction.phase(), OS0ItemTransactionPhase::EMPTY);
	EXPECT_FALSE(transaction.hasOrigin());
	EXPECT_TRUE(transaction.begin(OS0ItemTransferOrigin::World(
		401, 0, 1, 0, -1, kRifle)));
}


TEST(OS0ItemTransferTransactionTest, ContainerRequiresAStableAssetIdentity)
{
	OS0ItemTransferTransaction transaction;
	EXPECT_FALSE(transaction.begin(OS0ItemTransferOrigin::Container(
		400, 0, UINT16_MAX, 1, 0, -1, kMedkit)));

	OS0ItemTransferOrigin const source = OS0ItemTransferOrigin::Container(
		400, 0, 77, -3, 0x0240, 9, kMedkit, 4103, 400);
	ASSERT_TRUE(transaction.begin(source));
	ASSERT_TRUE(transaction.cancel());
	OS0ItemRestorationDecision const restore =
		transaction.restorationDecision();
	ASSERT_TRUE(restore.required());
	EXPECT_EQ(restore.origin.gridNo, 400);
	EXPECT_EQ(restore.origin.level, 0);
	EXPECT_EQ(restore.origin.containerTileIndex, 77);
	EXPECT_EQ(restore.origin.containerStructureId, 4103);
	EXPECT_EQ(restore.origin.containerBaseGridNo, 400);
	EXPECT_EQ(restore.origin.bVisible, -3);
	EXPECT_EQ(restore.origin.usFlags, 0x0240);
	EXPECT_EQ(restore.origin.bRenderZHeightAboveLevel, 9);

	// 0 is the engine's invalid/no-structure sentinel. 65535 is a real value
	// after the native monotonic structure ID wraps and must remain usable.
	transaction.reset();
	EXPECT_TRUE(transaction.begin(OS0ItemTransferOrigin::Container(
		401, 0, 78, -2, 0, -1, kRifle, UINT16_MAX, 401)));
}
