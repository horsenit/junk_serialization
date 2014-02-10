#include "common/IPrefix.h"
#include "skse/SafeWrite.h"
#include "skse/ScaleformCallbacks.h"
#include "skse/ScaleformMovie.h"
#include "skse/GameMenus.h"

extern IDebugLog gLog;

static UInt32 ** g_containerMode = (UInt32 **)0x01B3E6FC;

// set to true if you don't want inventory list updates queued, used internally by TakeAllItems
static UInt8 * bDontUpdate = (UInt8 *)0x01B4019C;
// checked in 897F90 (used by many functions, does something similar to ScheduleInventoryUpdate below)
// set in 84B910 (TakeAllItems implementation)

class InventoryUpdateData : public IUIMessageData
{
public:
	UInt32 unk08; // flag of some kind? id? form?
	UInt32 unk0c; // form reference?
};

void ScheduleInventoryUpdate()
{
	if (*bDontUpdate)
		return;
	UIStringHolder * stringHolder = UIStringHolder::GetSingleton();
	BSFixedString * menuName = &stringHolder->topMenu;
	//MenuManager * mm = MenuManager::GetSingleton();
	//if (mm->IsMenuOpen(&stringHolder->inventoryMenu))
	//	menuName = &stringHolder->inventoryMenu;
	//else if (mm->IsMenuOpen(&stringHolder->containerMenu))
	//	menuName = &stringHolder->containerMenu;
	//else if (mm->IsMenuOpen(&stringHolder->giftMenu))
	//	menuName = &stringHolder->giftMenu;
	//else if (mm->IsMenuOpen(&stringHolder->barterMenu))
	//	menuName = &stringHolder->barterMenu;

	InventoryUpdateData* inventoryUpdate = (InventoryUpdateData*) CreateUIMessageData(&stringHolder->inventoryUpdateData);
	if (inventoryUpdate) {
		inventoryUpdate->unk08 = 0x00100000;
		inventoryUpdate->unk0c = 0x00000000;
		CALL_MEMBER_FN(UIManager::GetSingleton(), AddMessage)(menuName, 8, inventoryUpdate);
	}
}

// adds InvalidateListData call even if transfer didn't succeed

// containermenu transfer
const UInt32 TRANSFER_JUMPBACK = 0x0084B342;
__declspec(naked) void ItemTransfer_AddUpdate() {
__asm {
	pushad
}
	ScheduleInventoryUpdate();
__asm {
	// original code equiv
	mov eax, [g_containerMode]
	mov eax, [eax]
	cmp eax, 2
	popad
	jmp [TRANSFER_JUMPBACK] // jnz
}
}

// inventorymenu drop item
const UInt32 DROP_QUEST_JUMPBACK = 0x00869F96;
const UInt32 DROP_CALL = 0x008997A0;
__declspec(naked) void ItemDrop_QuestItem_AddUpdate() {
__asm {
	pushad
}
	ScheduleInventoryUpdate();
__asm {
	popad
	call [DROP_CALL]
	jmp [DROP_QUEST_JUMPBACK]
}
}

const UInt32 DROP_KEY_JUMPBACK = 0x00869FBC;
__declspec(naked) void ItemDrop_Key_AddUpdate() {
__asm {
	pushad
}
	ScheduleInventoryUpdate();
__asm {
	popad
	call [DROP_CALL]
	jmp [DROP_KEY_JUMPBACK]
}
}

// bartermenu only shows items that can be sold/bought i think

// giftmenu unimplemented

void ApplyPatches()
{
	// seems kind of superfluous after whats below
	WriteRelJump(0x0084B33B, (UInt32)ItemTransfer_AddUpdate);

	WriteRelJump(0x00869F91, (UInt32)ItemDrop_QuestItem_AddUpdate);
	WriteRelJump(0x00869FB7, (UInt32)ItemDrop_Key_AddUpdate);
	
}

class SendUpdate : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args * args)
	{
		ScheduleInventoryUpdate();
	}
};

class EnableUpdates : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args * args)
	{
		ASSERT(args->numArgs >= 1 && (args->args[0].GetType() == GFxValue::kType_Bool || args->args[0].GetType() == GFxValue::kType_Number));
		args->result->SetUndefined();
		bool enable = args->args[0].GetBool();

		*bDontUpdate = enable ? 0 : 1;

		IMenu * menu;
		UIStringHolder * stringHolder = UIStringHolder::GetSingleton();
		MenuManager * menuManager = MenuManager::GetSingleton();
		UInt32 invOffset;

		BSFixedString * menuName = NULL;
		if (menuManager->IsMenuOpen(&stringHolder->containerMenu)) {
			invOffset = 0x30;
			menuName = &stringHolder->containerMenu;
		}
		else if (menuManager->IsMenuOpen(&stringHolder->barterMenu)) {
			invOffset = 0x1C;
			menuName = &stringHolder->barterMenu;
		}
		//else if (menuManager->IsMenuOpen(&stringHolder->inventoryMenu))
		//	menuName = &stringHolder->inventoryMenu;
		//else if (menuManager->IsMenuOpen(&stringHolder->giftMenu))
		//	menuName = &stringHolder->giftMenu;
		else {
			_MESSAGE("EnableUpdates no menu found");
			return;
		}

		menu = menuManager->GetMenu(menuName);
		if (menu == NULL) {
			_MESSAGE("EnableUpdates couldn't get menu");
			return;
		}
		// member of itemmenu? at least barter and container anyway, holds inventory list info
		void* invlist = *((void**)((UInt32)menu + invOffset));
		// flag that gets set when the menu normally wants to disallow any more changes pending an update
		// set it to 0 after it gets set (ex. ItemSelect/ItemTransfer) to be able to call it again without
		// waiting for an update
		// checked in 841D90 (to get selected item)
		*((UInt8*)((UInt32)invlist + 0x34)) = enable ? 1 : 0;
	}
};

// for a simple loop using ItemTransfer or ItemSelect to work:
//   must call enableUpdates(false) before each ItemTransfer otherwise the transfer will silently fail
//   must _not_ call transfer on same item twice (maybe, dunno, best not to)
//   (after calling transfer on an item, any information in the item object or entryList at the selectedIndex will be stale)
//   must call enableUpdates(true) true after otherwise no inventory updates will be sent
//   must call sendUpdate() after that otherwise the inventory shown to the player will not reflect actual inventories
void RegisterUpdateControl(GFxMovieView * view, GFxValue * root) {
	RegisterFunction <EnableUpdates> (root, view, "enableUpdates");
	RegisterFunction <SendUpdate> (root, view, "sendUpdate");
}
