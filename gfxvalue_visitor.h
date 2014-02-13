#ifndef GFXVALUE_VISITOR_H
#define GFXVALUE_VISITOR_H

#include "skse/ScaleformCallbacks.h"

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

#endif//GFXVALUE_VISITOR_H
