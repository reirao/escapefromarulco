#include "gtest/gtest.h"

#include "OS0_ItemTransferRuntime.h"

#include "Handle_Items.h"
#include "Interface_Items.h"
#include "Items.h"
#include "Structure.h"
#include "WorldDef.h"

#include <cstddef>


namespace
{
	class NativeItemPointerScope
	{
	public:
		NativeItemPointerScope()
			: savedItem_(gItemPointer), savedPointer_(gpItemPointer),
			savedSoldier_(gpItemPointerSoldier),
			savedSlot_(gbItemPointerSrcSlot)
		{
			gItemPointer = {};
			gItemPointer.usItem = 1;
			gItemPointer.ubNumberOfObjects = 1;
			gpItemPointer = &gItemPointer;
			gpItemPointerSoldier = nullptr;
			gbItemPointerSrcSlot = NO_SLOT;
		}

		~NativeItemPointerScope()
		{
			gItemPointer = savedItem_;
			gpItemPointer = savedPointer_;
			gpItemPointerSoldier = savedSoldier_;
			gbItemPointerSrcSlot = savedSlot_;
		}

	private:
		OBJECTTYPE savedItem_;
		OBJECTTYPE* savedPointer_;
		SOLDIERTYPE* savedSoldier_;
		INT8 savedSlot_;
	};

	class EmptyMapElementScope
	{
	public:
		explicit EmptyMapElementScope(INT32 const gridNo) :
			gridNo_(gridNo), saved_(gpWorldLevelData[gridNo])
		{
			gpWorldLevelData[gridNo_] = {};
		}

		~EmptyMapElementScope()
		{
			gpWorldLevelData[gridNo_] = saved_;
		}

	private:
		INT32 gridNo_;
		MAP_ELEMENT saved_;
	};

	class ContainerStructureScope
	{
	public:
		ContainerStructureScope(INT32 const gridNo, UINT8 const level,
			UINT16 const tileIndex, UINT16 const structureId) :
			gridNo_(gridNo), level_(level), saved_(gpWorldLevelData[gridNo])
		{
			gpWorldLevelData[gridNo_] = {};
			structure_.fFlags = STRUCTURE_BASE_TILE | STRUCTURE_OPENABLE;
			structure_.sGridNo = gridNo_;
			structure_.sBaseGridNo = gridNo_;
			structure_.usStructureID = structureId;
			node_.usIndex = tileIndex;
			node_.pStructureData = &structure_;
			if (level_ == 0) gpWorldLevelData[gridNo_].pStructHead = &node_;
			else gpWorldLevelData[gridNo_].pOnRoofHead = &node_;
		}

		~ContainerStructureScope()
		{
			gpWorldLevelData[gridNo_] = saved_;
		}

	private:
		INT32 gridNo_;
		UINT8 level_;
		MAP_ELEMENT saved_;
		STRUCTURE structure_{};
		LEVELNODE node_{};
	};
}


TEST(OS0ItemTransferRuntimeTest, ItemRepresentationIgnoresOnlyStructuralPadding)
{
	OBJECTTYPE original{};
	original.usItem = 17;
	original.ubNumberOfObjects = 2;
	original.bStatus[0] = 81;
	original.bStatus[1] = 67;
	original.usAttachItem[0] = 4;
	original.bAttachStatus[0] = 55;
	original.fFlags = OBJECT_KNOWN_TO_BE_TRAPPED;

	OBJECTTYPE copy = original;
	auto* const raw = reinterpret_cast<unsigned char*>(&copy);
	raw[offsetof(OBJECTTYPE, bStatus) - 1] ^= 0x5A;
	EXPECT_TRUE(OS0SameObjectRepresentation(original, copy));

	copy.bStatus[1]--;
	EXPECT_FALSE(OS0SameObjectRepresentation(original, copy));
}


TEST(OS0ItemTransferRuntimeTest, SpatialBeginsIssueDistinctSessionIdentities)
{
	NativeItemPointerScope pointer;
	OS0ItemTransferRuntime first;
	OS0ItemTransferRuntime second;
	ContainerStructureScope container(200, 1, 91, 4001);

	ASSERT_TRUE(first.beginWorld(100, 0, VISIBLE, 0x0200, -1));
	ASSERT_TRUE(second.beginContainer(200, 1, 91, HIDDEN_IN_OBJECT,
		0x0040, 4));
	EXPECT_TRUE(first.identity().valid());
	EXPECT_TRUE(second.identity().valid());
	EXPECT_NE(first.identity().instanceId, second.identity().instanceId);
	EXPECT_EQ(first.identity().itemType, gItemPointer.usItem);
	EXPECT_EQ(first.origin().kind, OS0ItemTransferOriginKind::WORLD);
	EXPECT_EQ(second.origin().kind, OS0ItemTransferOriginKind::CONTAINER);
	EXPECT_EQ(first.origin().usFlags, 0x0200);
	EXPECT_EQ(first.origin().bRenderZHeightAboveLevel, -1);
	EXPECT_EQ(second.origin().containerTileIndex, 91);
	EXPECT_EQ(second.origin().containerStructureId, 4001);
	EXPECT_EQ(second.origin().containerBaseGridNo, 200);
	EXPECT_EQ(second.origin().usFlags, 0x0040);
	EXPECT_EQ(second.origin().bRenderZHeightAboveLevel, 4);
}


TEST(OS0ItemTransferRuntimeTest, BeginRejectsMissingPointerAndInvalidSpatialData)
{
	OBJECTTYPE* const savedPointer = gpItemPointer;
	gpItemPointer = nullptr;
	OS0ItemTransferRuntime runtime;
	EXPECT_FALSE(runtime.beginWorld(100, 0, VISIBLE, 0, -1));
	gpItemPointer = savedPointer;

	NativeItemPointerScope pointer;
	EXPECT_FALSE(runtime.beginWorld(-1, 0, VISIBLE, 0, -1));
	EXPECT_FALSE(runtime.beginWorld(WORLD_MAX, 0, VISIBLE, 0, -1));
	EXPECT_FALSE(runtime.beginWorld(100, 2, VISIBLE, 0, -1));
	EXPECT_FALSE(runtime.beginWorld(100, 0, ANY_VISIBILITY_VALUE, 0, -1));
	EXPECT_FALSE(runtime.beginContainer(100, 0, UINT16_MAX, VISIBLE,
		0, -1));
	EXPECT_FALSE(runtime.hasOrigin());
}


TEST(OS0ItemTransferRuntimeTest, CommitDoesNotDestroyTheNativeCursor)
{
	NativeItemPointerScope pointer;
	OS0ItemTransferRuntime runtime;
	ASSERT_TRUE(runtime.beginWorld(100, 0, VISIBLE, 0, -1));
	ASSERT_TRUE(runtime.nativePointerStillBound());
	ASSERT_TRUE(runtime.commit());

	EXPECT_EQ(gpItemPointer, &gItemPointer);
	EXPECT_EQ(gpItemPointer->usItem, 1);
	EXPECT_FALSE(runtime.hasOrigin());
	EXPECT_FALSE(runtime.nativePointerStillBound());
}


TEST(OS0ItemTransferRuntimeTest, BindAfterDetachPreservesPrecapturedOrigin)
{
	NativeItemPointerScope pointer;
	OS0ItemTransferRuntime runtime;
	OS0ItemTransferOrigin const captured = OS0ItemTransferOrigin::World(
		321, 1, HIDDEN_ITEM, 0x0240, 8);
	ASSERT_FALSE(captured.valid());
	ASSERT_TRUE(runtime.bindAfterDetach(captured));

	EXPECT_TRUE(runtime.origin().valid());
	EXPECT_EQ(runtime.origin().gridNo, 321);
	EXPECT_EQ(runtime.origin().level, 1);
	EXPECT_EQ(runtime.origin().bVisible, HIDDEN_ITEM);
	EXPECT_EQ(runtime.origin().usFlags, 0x0240);
	EXPECT_EQ(runtime.origin().bRenderZHeightAboveLevel, 8);
	EXPECT_EQ(runtime.origin().item.itemType, gItemPointer.usItem);
	EXPECT_NE(runtime.origin().item.instanceId, 0u);
}


TEST(OS0ItemTransferRuntimeTest, ChangedNativeItemIsNeverRestoredAsTheSource)
{
	NativeItemPointerScope pointer;
	OS0ItemTransferRuntime runtime;
	ASSERT_TRUE(runtime.beginWorld(100, 0, VISIBLE, 0, -1));
	gItemPointer.usItem = 2;

	EXPECT_FALSE(runtime.nativePointerStillBound());
	EXPECT_EQ(runtime.cancel(),
		OS0ItemTransferCancelResult::NATIVE_ITEM_CHANGED);
	EXPECT_EQ(gpItemPointer, &gItemPointer);
	EXPECT_EQ(gpItemPointer->usItem, 2);
	EXPECT_TRUE(runtime.held());
}


TEST(OS0ItemTransferRuntimeTest, AnyHeldObjectMutationBreaksTheBinding)
{
	NativeItemPointerScope pointer;
	gItemPointer.bStatus[0] = 73;
	gItemPointer.usAttachItem[0] = 12;
	gItemPointer.bAttachStatus[0] = 66;
	OS0ItemTransferRuntime runtime;
	ASSERT_TRUE(runtime.beginWorld(100, 0, VISIBLE, 0x0200, 3));
	ASSERT_TRUE(runtime.nativePointerStillBound());

	gItemPointer.bAttachStatus[0] = 65;
	EXPECT_FALSE(runtime.nativePointerStillBound());
	EXPECT_EQ(runtime.cancel(),
		OS0ItemTransferCancelResult::NATIVE_ITEM_CHANGED);
	EXPECT_TRUE(runtime.held());
	EXPECT_EQ(gItemPointer.bAttachStatus[0], 65);
}


TEST(OS0ItemTransferRuntimeTest, ContainerCancelFailsClosedWhenAssetIdentityIsGone)
{
	NativeItemPointerScope pointer;
	OS0ItemTransferRuntime runtime;
	constexpr INT32 gridNo = WORLD_MAX - 1;
	ContainerStructureScope container(gridNo, 0, 42, 4002);

	ASSERT_TRUE(runtime.beginContainer(gridNo, 0, 42, HIDDEN_IN_OBJECT,
		0x0200, 5));
	{
		EmptyMapElementScope removedContainer(gridNo);
		EXPECT_EQ(runtime.cancel(),
			OS0ItemTransferCancelResult::RESTORE_FAILED_ITEM_HELD);
	}
	EXPECT_TRUE(runtime.held());
	EXPECT_TRUE(runtime.nativePointerStillBound());
	EXPECT_EQ(gItemPointer.usItem, 1);
	EXPECT_EQ(gItemPointer.ubNumberOfObjects, 1);
}


TEST(OS0ItemTransferRuntimeTest, SameTileReplacementIsNotTheOriginalContainer)
{
	NativeItemPointerScope pointer;
	OS0ItemTransferRuntime runtime;
	constexpr INT32 gridNo = WORLD_MAX - 2;
	ContainerStructureScope original(gridNo, 0, 43, 4010);
	ASSERT_TRUE(runtime.beginContainer(gridNo, 0, 43, HIDDEN_IN_OBJECT,
		0x0200, 5));

	{
		EmptyMapElementScope removedOriginal(gridNo);
		ContainerStructureScope replacement(gridNo, 0, 43, 4011);
		EXPECT_EQ(runtime.cancel(),
			OS0ItemTransferCancelResult::RESTORE_FAILED_ITEM_HELD);
	}
	EXPECT_TRUE(runtime.held());
	EXPECT_TRUE(runtime.nativePointerStillBound());
}
