#include "common/IPrefix.h"
#include "skse/SafeWrite.h"
#include "gfxvalue_visitor.h"
#include "ext_obj.h"
//#include "gfxvalue_logdump.h"

ItemMenuData * getItemData() {
	IMenu * menu;
	UIStringHolder * stringHolder = UIStringHolder::GetSingleton();
	MenuManager * menuManager = MenuManager::GetSingleton();
	UInt32 invOffset;

	BSFixedString * menuName = NULL;
	if (menuManager->IsMenuOpen(&stringHolder->containerMenu)) {
		invOffset = offsetof(ContainerMenu, itemData);
		menuName = &stringHolder->containerMenu;
	}
	else if (menuManager->IsMenuOpen(&stringHolder->barterMenu)) {
		invOffset = offsetof(BarterMenuExt, itemData);
		menuName = &stringHolder->barterMenu;
	}
	else if (menuManager->IsMenuOpen(&stringHolder->inventoryMenu)) {
		invOffset = offsetof(InventoryMenu, itemData);
		menuName = &stringHolder->inventoryMenu;
	}
	else {
		_MESSAGE("no menu");

		if (menuManager->IsMenuOpen(&stringHolder->inventoryMenu)) {
			menu = menuManager->GetMenu(&stringHolder->inventoryMenu);
			_MESSAGE(" INVMENU %08x", menu);
		}
		return NULL;
	}
	menu = menuManager->GetMenu(menuName);
	if (!menu) {
		_MESSAGE("menu null");
		return NULL;
	}
	ItemMenuData* data = *((ItemMenuData**)((UInt32)menu + invOffset));
	return data;
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
		ItemMenuData * data = getItemData();
		// flag that gets set when the menu normally wants to disallow any more changes pending an update
		// set it to 0 after it gets set (ex. ItemSelect/ItemTransfer) to be able to call it again without
		// waiting for an update
		// checked in 841D90 (to get selected item)
		data->updatePending = enable ? 1 : 0;
	}
};

class GetStolenCount : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args * args)
	{
		args->result->SetUndefined();
		ItemMenuData* data = getItemData();
		if (!data)
			return;
		UInt32 index = args->args[0].GetNumber();
		if (index >= data->items.count)
			return;
		StandardItemData * item = data->items[index];
		ObjDescExt * ext = static_cast<ObjDescExt*>(item->objDesc);
		UInt32 stolenCount = ext->stolenCount();
		args->result->SetNumber(stolenCount);
	}
};

class GetFirstExtraCount : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args * args)
	{
		args->result->SetUndefined();
		ItemMenuData * data = getItemData();
		if (!data)
			return;
		UInt32 index = args->args[0].GetNumber();
		if (index >= data->items.count)
			return;
		StandardItemData * item = data->items[index];
		ObjDescExt * ext = static_cast<ObjDescExt*>(item->objDesc);
		UInt32 count = 1;
		ExtraCount * extraCount = NULL;
		if (ext->extraData) {
			tList<BaseExtraList>::Iterator beli = ext->extraData->Begin();
			if (beli.Get()) {
				BaseExtraList * l = beli.Get();
				count = GetExtraCount(l);
			}
		}
		args->result->SetNumber(count);
	}
};

class UpdateList : public GFxFunctionHandler
{
public:
	/*void dumpData(ItemMenuData * data)
	{
		for (int i = 0; i < data->items.count; ++i) {
			StandardItemData * idata = data->items[i];
			_MESSAGE("%d %08x", idata->objDesc->countDelta, idata->objDesc->form);
			tList<BaseExtraList>::Iterator li;
			for (li = idata->objDesc->extraData->Begin(); li.Get(); ++li) {
				BaseExtraList* list = li.Get();
				BSExtraData * d = list->m_data;
				_MESSAGE("  xd");
				while (d) {
					_MESSAGE("    %x", d->GetType());
					d = d->next;
				}
			}
		}
	}

	void dumpObj(ItemMenuData * data)
	{
		for (int i = 0; i < data->itemList.GetArraySize(); ++i) {
			GFxValue obj;
			if (data->itemList.GetElement(i, &obj)) {
				double formId = -1, count = -1, filterFlag = -1;
				GFxValue val;
				if (obj.GetMember("formId", &val))
					formId = val.GetNumber();
				if (obj.GetMember("count", &val))
					count = val.GetNumber();
				if (obj.GetMember("filterFlag", &val))
					filterFlag = val.GetNumber();
				const char * text = NULL;
				if (obj.GetMember("text", &val))
					text = val.GetString();
				
				_MESSAGE("%d: %f %f %f %s", i, formId, count, filterFlag, text);
			}
		}
	}
*/
	virtual void Invoke(Args * args)
	{
		args->result->SetUndefined();
		ItemMenuData* data = getItemData();
		if (!data)
			return;

		UIStringHolder * stringHolder = UIStringHolder::GetSingleton();
		MenuManager * menuManager = MenuManager::GetSingleton();
		if (menuManager->IsMenuOpen(&stringHolder->barterMenu)) {
			BarterMenuExt * t = static_cast<BarterMenuExt*>(menuManager->GetMenu(&stringHolder->barterMenu));
			updateInventory(t);
		}
	}
};
/*
class DumpRoot : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args * args)
	{
		args->result->SetUndefined();
		UIStringHolder * stringHolder = UIStringHolder::GetSingleton();
		MenuManager * menuManager = MenuManager::GetSingleton();
		if (menuManager->IsMenuOpen(&stringHolder->inventoryMenu)) {
			InventoryMenu * t = static_cast<InventoryMenu*>(menuManager->GetMenu(&stringHolder->inventoryMenu));
			_MESSAGE("flags 1: %x, 2: %x", t->unkFlag, t->sortFlag);
		}

		/*typedef bool (* _LookupREFRByHandle)(UInt32 * refHandle, TESObjectREFR ** refrOut);
		static _LookupREFRByHandle LookupREFRByHandle = (_LookupREFRByHandle)0x004A9180;
		
		ItemMenuData* data = getItemData();
		if (!data)
			return;

		StandardItemData* selected = data->GetSelectedItem();

		_MESSAGE("getselecteditem %08x", selected);

		_MESSAGE("items %d %d", data->items.count, data->items.arr.capacity);
		for (UInt32 i = 0; i < data->items.count; ++i) {
			StandardItemData* item = data->items[i];
			//_MESSAGE("%s count %d %s", item->GetName(), item->GetCount(), item->objDesc->form->GetFullName());
			//TESObjectREFR * refr;
			/*bool b = LookupREFRByHandle((UInt32*)item->unk08, &refr);
			_MESSAGE("  %d", b?1:0);
			if (b) {
				_MESSAGE("%s", refr->GetName());
			}
			*/
			//LogDump::dump(&item->fxValue, 1);
/*		//}
	}
};

class DumpVar : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args * args)
	{
		ASSERT(args->numArgs >= 1);
		_MESSAGE("dump %x", &args->args[0]);
		//LogDump::dump(&args->args[0], 1);
	}
};
//*/
// for a simple loop using ItemTransfer or ItemSelect to work:
//   must call enableUpdates(false) before each ItemTransfer otherwise the transfer will silently fail
//   must _not_ call transfer on same item twice (maybe, dunno, best not to)
//   (after calling transfer on an item, any information in the item object or entryList at the selectedIndex will be stale)
//   must call enableUpdates(true) true after otherwise no inventory updates will be sent
//   must call sendUpdate() after that otherwise the inventory shown to the player will not reflect actual inventories
void RegisterUpdateControl(GFxMovieView * view, GFxValue * root) {
	RegisterFunction <EnableUpdates> (root, view, "enableUpdates");
	RegisterFunction <SendUpdate> (root, view, "sendUpdate");

	// when selling stolen items
	// shrug a dug, if the number to sell is <= the first ExtraCount (or 1 w/ no ExtraCount, doesn't
	// matter if no ExtraData) it takes the items from that "sub-list", otherwise it goes to the well
	// first (the well being items w/ no extra data like ownership etc)
	// patching ItemSelect for BarterMenu not really an option, tracing to try to reimplement it would be painful
	RegisterFunction <GetStolenCount> (root, view, "getStolenCount");
	RegisterFunction <GetFirstExtraCount> (root, view, "getFirstExtraCount");
	RegisterFunction <UpdateList> (root, view, "updateList");
	// explaining the usage here -- well, i won't, it's fucking stupid

	//RegisterFunction <DumpRoot> (root, view, "_debugDumpRoot");
	//RegisterFunction <DumpVar> (root, view, "_debugDumpVar");
}