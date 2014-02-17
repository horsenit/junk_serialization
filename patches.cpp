#include "common/IPrefix.h"
#include "skse/SafeWrite.h"
#include "gfxvalue_visitor.h"
#include "ext_obj.h"

extern IDebugLog gLog;

static UInt32 ** g_containerMode = (UInt32 **)0x01B3E6FC;

UInt32 * g_playerHandle = (UInt32*)0x01B2E8E8;

// set to true if you don't want inventory list updates queued, used internally by TakeAllItems
UInt8 * bDontUpdate = (UInt8 *)0x01B4019C;
// checked in 897F90 (used by many functions, does something similar to ScheduleInventoryUpdate below)
// set in 84B910 (TakeAllItems implementation)

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
		inventoryUpdate->refHandle = *g_playerHandle;
		inventoryUpdate->form = 0x00000000;
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

bool g_disableInvalidate = false;

const UInt32 InvalidateListData_loc = 0x841D30;
const UInt32 InvalidateListData_jmp = InvalidateListData_loc + 5;

__declspec(naked) void callInvalidateListData()
{
__asm {
	push eax
	mov al, byte ptr [g_disableInvalidate]
	test al, al
	pop eax
	jz doInvalidateCall
	ret // no args

doInvalidateCall:
	// original code
	sub esp, 20h
	mov ecx, [ecx]
	// will ret
	jmp [InvalidateListData_jmp]
}
}

// Normally inv menus only check the first extraData for ownership info,
// if the first is a reference handle or alias list or whatever, it hides
// the fact that there exist stolen items in the item stack.
// this checks all entries in extradata and returns as soon as ownership is encountered (if it is)
bool ObjDescExt::FixStolenCheck(TESForm* checkOwner, ExtraOwnership* owner, bool defaultValue)
{
	tList<BaseExtraList>::Iterator li;
	for (li = extraData->Begin(); li.Get(); ++li) {
		BaseExtraList * list = li.Get();
		if (owner = static_cast<ExtraOwnership*>(list->GetByType(kExtraData_Ownership))) {
			bool isOwned = CALL_MEMBER_FN(this, IsOwnedData)(checkOwner, owner, true);
			if (!isOwned)
				return false;
		}
	}
	return defaultValue;
}

bool __stdcall FixStolenCheck(TESForm* checkOwner, ExtraOwnership* owner, bool defaultValue)
{
	ObjDescExt * pthis;
	__asm mov pthis, ecx
	return pthis->FixStolenCheck(checkOwner, owner, defaultValue);
}

void ApplyPatches()
{
	// sort of superfluous after allowing multiple ItemTransfers and ItemSelects with enableUpdate/etc
	// cause an invalidatelistdata event to be scheduled even if nothing was transferred or dropped
	WriteRelJump(0x0084B33B, (UInt32)ItemTransfer_AddUpdate);

	WriteRelJump(0x00869F91, (UInt32)ItemDrop_QuestItem_AddUpdate);
	WriteRelJump(0x00869FB7, (UInt32)ItemDrop_Key_AddUpdate);

	// small bug fix, the default stolen check to display in inventory only checks the first BaseExtraList
	// of each item, assuming it will contain ExtraOwnership if the item is stolen, but if it's something else,
	// like a ExtraData ReferenceHandle, then it won't necessarily contain ownership info even if another item
	// in the item stack does, and the inventory screen doesn't reflect that there are stolen items in that
	// item stack
	WriteRelCall(0x00477044, (UInt32)FixStolenCheck);

	// allows to disable callback InvalidateListData depending on g_disableInvalidate
	WriteRelJump(InvalidateListData_loc, (UInt32)callInvalidateListData);
}
