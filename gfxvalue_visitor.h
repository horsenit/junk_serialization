#ifndef GFXVALUE_VISITOR_H
#define GFXVALUE_VISITOR_H

#include "skse/ScaleformMovie.h"
#include "skse/ScaleformCallbacks.h"
#include "cJSON.h"
#include <vector>
#include <algorithm>

class ObjectVisitor
{
public:
	virtual ~ObjectVisitor() {}
protected:
	virtual void Visit_impl(const char *** name, void* unk1, void* unk2) {
		/*const char * name_str;
		__asm {
			push eax
			mov eax, name
			mov eax, [eax]
			mov eax, [eax]
			mov name_str, eax
			pop eax
		}*/
		Visit(**name);
	}

	// fairly certain this is where original vtable ends
public:
	virtual void Visit(const char* name) = 0;
};

bool visitMembers(GFxValue* val, ObjectVisitor* visitor, bool isDisplayObject);

class Stringify
{
public:
	class StringifyVisitor : public ObjectVisitor
	{
	public:
		virtual ~StringifyVisitor() {}
		virtual void Visit(const char* name) {
			if (stringify.error)
				return;
			//_MESSAGE("prop name %s", name);
			GFxValue item;
			groot->GetMember(name, &item);
			cJSON* jv = stringify.stringify_s(&item);
			if (jv) // if undefined, or unable to parse don't add
				cJSON_AddItemToObject(jroot, name, jv);
		}

		StringifyVisitor(Stringify& stringify, cJSON* jroot, GFxValue* groot)
			: stringify(stringify), jroot(jroot), groot(groot) {}

		Stringify& stringify;
		cJSON* jroot;
		GFxValue* groot;
	};

	typedef std::vector<void*> Parents;
	Parents parents;
	bool error;

	Stringify() : error(false) {}

	cJSON* stringify_s (GFxValue* groot) {
		if (error)
			return NULL;
		cJSON* jroot = NULL;
		if (groot->GetType() == GFxValue::kType_Array || groot->IsObject() || groot->IsDisplayObject()) {
			Parents::iterator found = std::find(parents.begin(), parents.end(), groot->data.obj);
			if (found != parents.end()) {
				_MESSAGE("stringify encountered cyclic reference");
				error = true; // cyclic reference
				return NULL;
			}
			parents.push_back(groot->data.obj);
		}
		switch (groot->GetType()) {
		case GFxValue::kType_Undefined:
			break;
		case GFxValue::kType_Null:
			jroot = cJSON_CreateNull();
			break;
		case GFxValue::kType_Bool:
			jroot = cJSON_CreateBool(groot->GetBool());
			break;
		case GFxValue::kType_Number:
			jroot = cJSON_CreateNumber(groot->GetNumber());
			break;
		case GFxValue::kType_String:
			jroot = cJSON_CreateString(groot->GetString());
			break;
		case GFxValue::kType_Array:
			{
				jroot = cJSON_CreateArray();
				UInt32 len = groot->GetArraySize();
				for (UInt32 i = 0; i < len; ++i) {
					GFxValue item;
					groot->GetElement(i, &item);
					cJSON* jv = stringify_s(&item);
					if (error)
						return NULL;
					cJSON_AddItemToArray(jroot, jv ? jv : cJSON_CreateNull()); // if undefined or unable to parse add null (consistent with browser JSON generators)
				}
			}
			break;
		case GFxValue::kType_DisplayObject:
		case GFxValue::kType_Object:
			{
				jroot = cJSON_CreateObject();
				StringifyVisitor visitor(*this, jroot, groot);
				visitMembers(groot, &visitor, groot->IsDisplayObject());
				if (error)
					return NULL;
			}
			break;
		}
		if (groot->GetType() == GFxValue::kType_Array || groot->IsObject() || groot->IsDisplayObject()) {
			parents.pop_back();
		}
		return jroot;
	}

	cJSON* stringify(GFxValue* groot) {
		parents.clear();
		return stringify_s(groot);
	}
};

#endif//GFXVALUE_VISITOR_H
