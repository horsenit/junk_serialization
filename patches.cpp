#include "common/IPrefix.h"
#include "skse/SafeWrite.h"
#include "skse/ScaleformCallbacks.h"
#include "skse/GameMenus.h"

extern IDebugLog gLog;

static UInt32 ** g_containerMode = (UInt32 **)0x01B3E6FC;

// adds callbacks to ItemDrop, ItemSelect, ItemTransfer (if a valid entry selected)

/*

ref attemptchargeitem

0x00A63C40
externalinterface "call":
void callback(unk, char* callbackName, FXResponseArgs args)

0x00A63CC0
externalinterface "respond":

void __thiscall respond(FxResponseArgs* args)

*/

//const UInt32 RESPOND = 0x00A63CC0;

/*
FxResponseArgs<0> vtable
0x010E37A4
FxResponseArgs<1> vtable
0x010E37AC
*/
/*
const UInt32 VTABLE_FXRESPONSEARGS_0 = 0x010E37A4;

__declspec(naked) void EmptyResponse()
{
__asm {
	sub esp, 20h // room for FxResponseArgs
	// ecx has 'this'
	
	// construct FxResponseArgs<0>
	mov edx, VTABLE_FXRESPONSEARGS_0
	mov dword ptr [esp], edx
	mov dword ptr [esp+08h], 0
	mov dword ptr [esp+0Ch], 0
	mov dword ptr [esp+18h], 1

	lea edx, [esp]
	push edx
	// ecx is this already
	call [RESPOND]
	add esp, 20h
	ret
}
}

const UInt32 DROP_CALL = 0x4759B0;
const UInt32 DROP_JUMPBACK = 0x00869F83;


__declspec(naked) void ItemDrop_Callback()
{
__asm {
	call [DROP_CALL]

	pushad
	mov ecx, esi
	call EmptyResponse
	popad
	jmp [DROP_JUMPBACK]
}
}

const UInt32 TRANSFER_JUMPBACK = 0x0084B2A3;

__declspec(naked) void ItemTransfer_Callback()
{
__asm {
	mov ecx, [edi+18h]
	fnstcw word ptr [esp+28h+4h]

	pushad
	mov ecx, edi
	call EmptyResponse
	pushad
}
	ScheduleInventoryUpdate();
__asm {
	popad
	popad
	jmp [TRANSFER_JUMPBACK]
}
}
*/


class InventoryUpdateData : public IUIMessageData
{
public:
	UInt32 unk08; // flag of some kind? id? form?
	UInt32 unk0c; // form reference?
};

inline void ScheduleInventoryUpdate()
{
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
	//00869F7E                 call    sub_4759B0
	//WriteRelJump(0x00869F7E, (UInt32)ItemDrop_Callback);
	//0084B29C                 mov     ecx, [edi+18h]
	//WriteRelJump(0x0084B29C, (UInt32)ItemTransfer_Callback);
	WriteRelJump(0x0084B33B, (UInt32)ItemTransfer_AddUpdate);

	WriteRelJump(0x00869F91, (UInt32)ItemDrop_QuestItem_AddUpdate);
	WriteRelJump(0x00869FB7, (UInt32)ItemDrop_Key_AddUpdate);
	
}