#ifndef EXT_OBJ_H
#define EXT_OBJ_H

#include "skse/ScaleformCallbacks.h"
#include "skse/ScaleformMovie.h"
#include "skse/GameMenus.h"
#include "skse/GameExtraData.h"
#include "skse/GameRTTI.h"

void ApplyPatches();
void RegisterUpdateControl(GFxMovieView * view, GFxValue * root);

extern UInt8 * bDontUpdate;

void ScheduleInventoryUpdate();

// defined in Hooks_Scaleform.cpp -- no changes
class StandardItemData
{
public:
	virtual ~StandardItemData();

	virtual const char *	GetName(void);
	virtual UInt32			GetCount(void);
	virtual UInt32			GetEquipState(void);
	virtual UInt32			GetFilterFlag(void);
	virtual UInt32			GetFavorite(void);
	virtual bool			GetEnabled(void);

//	void						** _vtbl;	// 00
	PlayerCharacter::ObjDesc	* objDesc;	// 04
	void						* unk08;	// 08 refr handle? owner id? 00100000 for player, diff for container
	UInt32						unk0C;		// 0C
	GFxValue					fxValue;	// 10

	//MEMBER_FN_PREFIX(StandardItemData);
	//DEFINE_MEMBER_FN(ctor_data, StandardItemData *, 0x00842140, GFxMovieView ** movieView, PlayerCharacter::ObjDesc * objDesc, int unk);

	//enum { kCtorHookAddress = 0x008433B0 + 0x0049 };

	//StandardItemData * ctor_Hook(GFxMovieView ** movieView, PlayerCharacter::ObjDesc * objDesc, int unk);
};

struct ObjDescExt : public PlayerCharacter::ObjDesc {
	DEFINE_MEMBER_FN(IsOwned, bool, 0x00477010, TESForm* checkOwner, bool defaultValue);
	DEFINE_MEMBER_FN(IsOwnedData, bool, 0x004755D0, TESForm* checkOwner, ExtraOwnership* list, bool defaultValue);

	bool IsStolen() {
		bool ret = CALL_MEMBER_FN(this, IsOwned)(*g_thePlayer, 1);
		return !ret;
	}

	bool FixStolenCheck(TESForm* checkOwner, ExtraOwnership* owner, bool defaultValue);

	UInt32 stolenCount() {
		UInt32 count = 0;
		tList<BaseExtraList>::Iterator li;
		//_MESSAGE("-");
		for (li = extraData->Begin(); li.Get(); ++li) {
			BaseExtraList * list = li.Get();
			if (li->HasType(kExtraData_Ownership)) {
				ExtraOwnership * owner = static_cast<ExtraOwnership*>(list->GetByType(kExtraData_Ownership));
				bool isOwned = CALL_MEMBER_FN(this, IsOwnedData)(*g_thePlayer, owner, true);
				if (!isOwned) {
					ExtraCount * extraCount = static_cast<ExtraCount*>(list->GetByType(kExtraData_Count));
					if (extraCount) {
						count += extraCount->count & 0xffff; // 16 bit value
						//_MESSAGE("  sc %d", extraCount->count & 0xffff);
					} else {
						++count;
						//_MESSAGE("  sc 1");
					}
				}
			}
		}
		return count;
	}
};

//struct BaseExtraListExt : public BaseExtraList
//{
	//DEFINE_MEMBER_FN(GetExtraData, BSExtraData*, 0x0040A8A0, ExtraDataType type);
//};

// 976580 - constructor?
class ItemMenuData
{
public:
	GFxMovieView * view; // 00
	UInt32 unk04;
	GFxValue listObject; // 08 - List object (gets selectedIndex property, etc)
	GFxValue itemList; // 18 - GFxValue array of items
	tArray<StandardItemData*> items; // 28
	UInt8 updatePending; // 34 - flag checked in GetSelectedItem, returns NULL if set
	UInt8 unk31[3];
	// ...
	
	// grabs StandardItem that corresponds to listObject's selectedIndex
	StandardItemData* GetSelectedItem() {
		return CALL_MEMBER_FN(this, GetSelectedItem)();
	}

	MEMBER_FN_PREFIX(ItemMenuData);
	// returns null if updatePending above is set
	DEFINE_MEMBER_FN(GetSelectedItem, StandardItemData*, 0x00841D90);
	// sets updatePending above, then calls 0x00897F90 which adds an InventoryUpdate message to the queue
	//DEFINE_MEMBER_FN(ScheduleUpdate, void, 0x00841E70, TESForm* source /*not sure*/);
	//DEFINE_MEMBER_FN(InvalidateListData, void, 0x00841D30);
};
//PlayerCharacter	** g_thePlayer = (PlayerCharacter **)0x01B2E8E4;
extern UInt32 * g_playerHandle;

/*
class IUIMessageData
{
public:
	virtual ~IUIMessageData();
//	void	** _vtbl;	// 00
	UInt32 message; // 04, ex 3. for close menu, 1 when opening, 8 for itemmenus inventoryupdatedata
};
*/

class InventoryUpdateData : public IUIMessageData
{
public:
	UInt32 refHandle; // 08 ref handle, 0x00100000 for player, properly *0x1B2E8E8, or use container
	TESForm * form; // 0C can be null
};

class BarterMenuExt : public IMenu
{
public:
	ItemMenuData * itemData; // 1C
	UInt32 unk20[(0x54 - 0x20) >> 2]; // 20
	UInt32 unkFlag; // 54 -- non-zero 843FF0 , zero 843EE0, initialized maybe
	UInt32 unk58;
	UInt32 unk5C;
	UInt32 sortFlag; // 60

	MEMBER_FN_PREFIX(BarterMenuExt);
	DEFINE_MEMBER_FN(UpdateInventory, void, 0x844450);
};

class InventoryMenu : public IMenu
{
public:
	UInt32 unk1C;
	UInt32 unk20;
	UInt32 unk24;
	UInt32 unk28;
	UInt32 unk2C;
	ItemMenuData * itemData; // 30
	UInt32 unk34;
	UInt32 unk38;
	UInt32 unk3C;
	UInt32 unk40;
	UInt32 unkFlag; // 44 -- non-zero 86AA50 , zero 86A9F0
	UInt32 unk48;
	UInt32 sortFlag; // 4C switch on

	MEMBER_FN_PREFIX(InventoryMenu);
	DEFINE_MEMBER_FN(UpdateInventory, void, 0x86B980);
};

class ContainerMenu : public IMenu
{
public:
	UInt32 unk1C;
	UInt32 unk20;
	UInt32 unk24;
	UInt32 unk28;
	UInt32 unk2C;
	ItemMenuData * itemData; // 30
	UInt32 unk34;
	UInt32 unk38;
	UInt32 unk3C;
	UInt32 unk40;
	UInt32 unkFlag; // 44 -- non-zero 84A710 , zero 84B020
	UInt32 unk48[(0x6C - 0x48) >> 2];
	UInt32 sortFlag; // 6C

	MEMBER_FN_PREFIX(ContainerMenu);
	DEFINE_MEMBER_FN(UpdateInventory, void, 0x84B720);
};

// could be all wrong, haven't double checked
class GiftMenu : IMenu
{
public:
	UInt32 unk1C;
	UInt32 unk20;
	UInt32 unk24;
	UInt32 unk28;
	UInt32 unk2C;
	ItemMenuData * itemData; // 30
	UInt32 unk34;
	UInt32 unk38;
	UInt32 unk3C;
	UInt32 unkFlag; // 40
	UInt32 unk44;
	UInt32 sortFlag; // 48

	MEMBER_FN_PREFIX(GiftMenu);
	DEFINE_MEMBER_FN(UpdateInventory, void, 0x85DDA0);
};

// Call with T = BarterMenuExt, ContainerMenu, InventoryMenu, GiftMenu
// disables the invalidatelistdata call and updates the item list immediately
extern bool g_disableInvalidate;

template<typename T>
inline void updateInventory(T * menu)
{
	g_disableInvalidate = true;
	CALL_MEMBER_FN(menu, UpdateInventory)();
	g_disableInvalidate = false;
}

// Dumb method to get ItemMenuData, checks open menus then returns if it finds an ItemMenu (oen of the 4 above) open
// todo: less shitty way
ItemMenuData * getItemData();

/*
GFxValue::ObjectInterface
  bool RemoveElements(void * data, int at, int count) = 0x00920F40
*/

inline UInt16 GetExtraCount(BaseExtraList * el) {
	ExtraCount * count = static_cast<ExtraCount*>(el->GetByType(kExtraData_Count));
	if (count) {
		return count->count & 0xffff;
	} else {
		return 1;
	}
}

#endif//EXT_OBJ_H
