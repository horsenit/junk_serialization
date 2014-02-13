#ifndef GFXVALUE_LOGDUMP_H
#define GFXVALUE_LOGDUMP_H

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

class LogDump
{
	class LogVisitor : public ObjectVisitor
	{
	public:
		virtual ~LogVisitor() {}
		LogVisitor(LogDump& dumper, GFxValue* root) : dumper(dumper), root(root) {}

		LogDump& dumper;
		GFxValue* root;

		virtual void Visit(const char* name) {
			dumper.logf("%s:", name);
			GFxValue item;
			if (!root->GetMember(name, &item)) {
				dumper.logf("couldn't get member");
			} else {
				dumper.do_dump(&item);
			}
		}
	};

	typedef std::vector<void*> Parents;
	Parents parents;
	int level;

	bool IsObject(GFxValue* val) {
		return (val->GetType() == GFxValue::kType_Array || val->IsObject() || val->IsDisplayObject());
	}

	bool findParent(GFxValue* val) {
		Parents::iterator found = std::find(parents.begin(), parents.end(), val->data.obj);
		return found != parents.end();
	}

	void addParent(GFxValue* val) {
		parents.push_back(val->data.obj);
	}

	void popParent() {
		parents.pop_back();
	}

	void log(const char* message) {
		std::stringstream ss;
		for (int i = 0; i < level; ++i) {
			ss << "  ";
		}
		ss << message;
		gLog.Message(ss.str().c_str());
	}

	void log(const std::string& message) {
		log(message.c_str());
	}

	void logf(const char* format, ...) {
		va_list args;
		va_start(args, format);
		char buf[1024*32];
		vsnprintf_s(buf, sizeof(buf), ARRAYSIZE(buf), format, args);
		log(buf);
		va_end(args);
	}

	void do_dump(GFxValue* root) {
		if (findParent(root)) {
			logf("circular reference parent=%08x", root->data.obj);
			return;
		}
		addParent(root);
		++level;
		switch(root->GetType()) {
		case GFxValue::kType_Undefined:
			log("undefined");
			break;
		case GFxValue::kType_Null:
			log("null");
			break;
		case GFxValue::kType_Bool:
			log(root->GetBool() ? "true" : "false");
			break;
		case GFxValue::kType_Number:
			logf("%f", root->GetNumber());
			break;
		case GFxValue::kType_String:
			logf("\"%s\"", root->GetString());
			break;
		case GFxValue::kType_Array:
		case GFxValue::kType_Object:
		case GFxValue::kType_DisplayObject:
			{
				LogVisitor v(*this, root);
				logf("%08x properties (type: %d)", root->data.obj, root->GetType());
				visitMembers(root, &v, root->IsDisplayObject());
			}
			break;
		default:
			logf("unknown object type %x (unmasked %08x)", root->GetType(), root->type);
			break;
		}
		--level;
		popParent();
	}

	LogDump() : level(-1) {}

public:
	static void dump(GFxValue* root, int level = 0) {
		LogDump dumper;
		dumper.level = level - 1;
		dumper.do_dump(root);
	}
};

#endif//GFXVALUE_LOGDUMP_H