/*
allows user to get/set string keys/values from skse serialization data (tied to savegame) from scaleform

sort of similar to localStorage in html5

strings cannot contain null characters (escape them)

intrinsic class skse {
static var plugins:Object;
}

// store a string
function skse.plugins.junk_serialization.SetData(key:String, value:String):Void;
// retrieve string
function skse.plugins.junk_serialization.GetData(key:String):String;
// remove data associated with key
function skse.plugins.junk_serialization.Remove(key:String):Void;

SetObjects and GetObjects are pretty specific to junk stuff and not generically usable,
test to increase json parsing speed when structure of object is known

much faster than an actionscript json parser or other as string parsing methods

actionscript: couple minutes for 20,000 objects and about 4mb of raw json data
or a couple seconds w/ the C parser

pretty much copied from the skse plugin_example:
*/
#include "common/IPrefix.h"
#include "common/IFileStream.h"
#include "skse/PluginAPI.h"
#include "skse/skse_version.h"
#include "skse/ScaleformCallbacks.h"
#include "skse/ScaleformMovie.h"
#include "cJSON.h"
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <shlobj.h>

const UInt32 kPluginVersion = 1;
const UInt32 kSerializationUniqueID = 'JUNK';

IDebugLog gLog;
PluginHandle	g_pluginHandle = kPluginHandle_Invalid;
SKSEScaleformInterface		* g_scaleform = NULL;
SKSESerializationInterface	* g_serialization = NULL;

typedef std::map< std::string, std::string > dataMap;
typedef dataMap::iterator iter;

dataMap g_data;

// Scaleform

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

public:
	virtual void Visit(const char* name) = 0;
};

#define BDISPLAYOBJECT_GET 0x92CC00

bool visitMembers(GFxValue* val, ObjectVisitor* visitor, bool isDisplayObject) {
	void* data = val->data.obj;
	GFxValue::ObjectInterface* othis = val->objectInterface;
	GFxMovieRoot* root = othis->root;

	if (isDisplayObject) {
	__asm {
		pushad

		mov ecx, data
		mov ebx, root
		push ebx
		mov esi, BDISPLAYOBJECT_GET
		call esi
		test eax, eax
		jz errorDisplay
		lea esi, [eax+78h]
		test esi, esi
		jz errorDisplay
		mov data, esi
		jmp good
	errorDisplay:
		popad
	}
		return false;
	}

__asm {
	pushad
good:
	mov ecx, root
	mov ecx, [ecx+30h]
	mov edx, [ecx]
	mov eax, [edx+70h]
	call eax

	mov edi, eax
	add edi, 78h

	mov ecx, data
	//mov eax, [ecx]
	mov edx, [ecx]
	mov edx, [edx+20h]

	push 0
	push 3
	mov eax, visitor
	push eax
	push edi
	call edx

	popad
}

	return true;
}

cJSON* stringify(GFxValue* groot, GFxMovieView* movie, std::vector<GFxValue*>& parents);

class Stringify
{
public:
	class StringifyVisitor : public ObjectVisitor
	{
	public:
		virtual ~StringifyVisitor() {}
		virtual void Visit(const char* name) {
			//_MESSAGE("prop name %s", name);
			GFxValue item;
			groot->GetMember(name, &item);
			cJSON* jv = stringify.stringify_s(&item);
			std::string spaces;
			//for (int i = 0; i < level; ++i) {
			//	spaces = spaces + "  ";
			//}
			//_MESSAGE("%s", (spaces + name).c_str());
			if (jv) // if undefined, or unable to parse don't add
				cJSON_AddItemToObject(jroot, name, jv);
		}

		StringifyVisitor(Stringify& stringify, cJSON* jroot, GFxValue* groot)
			: stringify(stringify), jroot(jroot), groot(groot) {}

		Stringify& stringify;
		cJSON* jroot;
		GFxValue* groot;
	};

	int level;
	GFxMovieView* movie;
	std::vector<GFxValue*> parents;
	bool error;

	Stringify(GFxMovieView* movie) : movie(movie), error(false) {}

	cJSON* stringify_s (GFxValue* groot) {
		if (error)
			return NULL;
		cJSON* jroot = NULL;
		parents.push_back(groot);
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
				level++;
				jroot = cJSON_CreateArray();
				UInt32 len = groot->GetArraySize();
				for (UInt32 i = 0; i < len; ++i) {
					GFxValue item;
					groot->GetElement(i, &item);
					cJSON* jv = stringify_s(&item);
					std::vector<GFxValue*>::iterator found = std::find(parents.begin(), parents.end(), &item);
					if (found != parents.end()) {
						error = true; // cyclic reference
						return NULL;
					}
					cJSON_AddItemToArray(jroot, jv ? jv : cJSON_CreateNull()); // if undefined or unable to parse add null (consistent with browser JSON generators)
				}
			}
			break;
		case GFxValue::kType_DisplayObject:
			// display objects like to stack overflowwwww
		case GFxValue::kType_Object:
			{
				level++;
				std::vector<GFxValue*>::iterator found = std::find(parents.begin(), parents.end(), groot);
				if (found != parents.end()) {
					error = true;
					return NULL;
				}
				jroot = cJSON_CreateObject();
				StringifyVisitor visitor(*this, jroot, groot);
				visitMembers(groot, &visitor, groot->IsDisplayObject());
			}
			break;
		}
		parents.pop_back();
		return jroot;
	}

	cJSON* stringify(GFxValue* groot) {
		parents.clear();
		return stringify_s(groot);
	}
};

class hasMember : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args * args)
	{
		bool b = args->args[0].HasMember("fart");
		args->result->SetBool(b);
	}
};

class SKSEScaleform_stringify : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args * args)
	{
		ASSERT(args->numArgs >= 1);
		_MESSAGE("type %d", args->args[0].GetType());
		if (!(args->args[0].GetType() == GFxValue::kType_Array || args->args[0].IsObject() || args->args[0].IsDisplayObject())) {
			args->result->SetUndefined();
			return;
		}
		bool pretty = false;
		if (args->numArgs > 1) {
			pretty = args->args[1].GetBool();
		}

		Stringify stringifier(args->movie);
		cJSON* jroot = stringifier.stringify(&args->args[0]);
		if (jroot != NULL) {
			char* text;
			if (pretty)
				text = cJSON_Print(jroot);
			else
				text = cJSON_PrintUnformatted(jroot);

			char	path[MAX_PATH];

			ASSERT(SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, path)));
			strcat_s(path, sizeof(path), "\\My Games\\Skyrim\\SKSE\\json.log");
			IFileStream::MakeAllDirs(path);

			FILE* fp = NULL;
			errno_t ec = fopen_s(&fp, path, "wb");
			if (fp) {
				fwrite(text, 1, strlen(text), fp);
				fclose(fp);
			}

			args->movie->CreateString(args->result, text);

			free(text);
			cJSON_Delete(jroot);
		} else {
			if (stringifier.error) {
				_MESSAGE("stringify encountered cyclic reference");
			}
			args->result->SetUndefined();
		}
	}
};

// function SetSerializationData(name:String, data:String):Void
class SKSEScaleform_SetData : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args * args)
	{
		ASSERT(args->numArgs >= 2);
		args->result->SetUndefined();
		g_data[args->args[0].GetString()] = args->args[1].GetString();

		_MESSAGE("SetData %s", args->args[0].GetString());
	}
};

// function GetSerializationData(name:String):String
class SKSEScaleform_GetData : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args * args)
	{
		ASSERT(args->numArgs >= 1);

		iter i = g_data.find(args->args[0].GetString());
		if (i != g_data.end()) {
			args->movie->CreateString(args->result, i->second.c_str());
		}
		else {
			args->result->SetUndefined();
		}
		_MESSAGE("GetData(%s)", args->args[0].GetString());
	}
};

bool parseJSON(cJSON* jroot, GFxMovieView* movie, GFxValue* groot);

bool parseArray(cJSON* jroot, GFxMovieView* movie, GFxValue* groot) {
	movie->CreateArray(groot);
	for (cJSON* c = jroot->child; c;c = c->next) {
		GFxValue val;
		if (parseJSON(c, movie, &val)) {
			groot->PushBack(&val);
		} else {
			return false;
		}
	}
	return true;
}

bool parseObject(cJSON* jroot, GFxMovieView* movie, GFxValue* groot) {
	movie->CreateObject(groot);
	for (cJSON* c = jroot->child; c; c = c->next) {
		GFxValue val;
		if (parseJSON(c, movie, &val)) {
			groot->SetMember(c->string, &val);
		} else {
			return false;
		}
	}
	return true;
}

bool parseJSON(cJSON* jroot, GFxMovieView* movie, GFxValue* groot) {
	switch (jroot->type) {
	case cJSON_Array:
		if (!parseArray(jroot, movie, groot))
			return false;
		break;
	case cJSON_Object:
		if (!parseObject(jroot, movie, groot))
			return false;
		break;
	case cJSON_True:
		groot->SetBool(true);
		break;
	case cJSON_False:
		groot->SetBool(false);
		break;
	case cJSON_NULL:
		groot->SetNull();
		break;
	case cJSON_Number:
		groot->SetNumber(jroot->valuedouble);
		break;
	case cJSON_String:
		movie->CreateString(groot, jroot->valuestring);
		break;
	default:
		return false;
	}
	return true;
}

// function parse(json:String):Object
class SKSEScaleform_parse : public GFxFunctionHandler
{

public:
	virtual void Invoke(Args * args)
	{
		ASSERT(args->numArgs >= 1);
		args->result->SetUndefined();

		cJSON* root = cJSON_Parse(args->args[0].GetString());
		if (root) {
			if (!parseJSON(root, args->movie, args->result)) {
				args->result->SetUndefined();
			}
			cJSON_Delete(root);
		}
	}
};

// function GetObjects(name:String):Array
class SKSEScaleform_GetObjects : public GFxFunctionHandler
{

public:
	virtual void Invoke(Args * args)
	{
		ASSERT(args->numArgs >= 1);
		args->result->SetUndefined();
		iter i = g_data.find(args->args[0].GetString());
		cJSON* root;
		if (i != g_data.end() && (root = cJSON_Parse(i->second.c_str()))) {
			if (!parseJSON(root, args->movie, args->result)) {
				args->result->SetUndefined();
			}
			cJSON_Delete(root);
		}
	}
};

//static const char* members[] = {"baseId", "formId", "name", "effects"};
// function SetObjects(name:String, objects:Array, memberNames:Array):Boolean
class SKSEScaleform_SetObjects : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args * args)
	{
		if (args->numArgs < 2 ||
			args->args[0].GetType() != GFxValue::kType_String) {
				args->result->SetBool(false);
				return;
		}
		_MESSAGE("SetObjects %s", args->args[0].GetString());
		
		Stringify stringifier(args->movie);
		cJSON* root = stringifier.stringify(&args->args[1]);
		args->result->SetBool(root != NULL);
		if (root != NULL) {
			char * text = cJSON_PrintUnformatted(root);

			g_data[std::string(args->args[0].GetString())] = std::string(text);

			free(text);
			cJSON_Delete(root);
		}
	}
};

class SKSEScaleform_Remove : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args * args)
	{
		ASSERT(args->numArgs >= 1);
		args->result->SetUndefined();
		iter i = g_data.find(args->args[0].GetString());
		if (i != g_data.end()) {
			_MESSAGE("Remove %s %d bytes", i->first.c_str(), i->second.size());
			g_data.erase(i);
		}
	}
};

class TLog : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args * args)
	{
		ASSERT(args->numArgs >= 1);
		args->result->SetUndefined();

		gLog.Message(args->args[0].GetString());
	}
};

bool RegisterScaleform(GFxMovieView * view, GFxValue * root)
{
	RegisterFunction <SKSEScaleform_SetData>(root, view, "SetData");
	RegisterFunction <SKSEScaleform_GetData>(root, view, "GetData");
	
	RegisterFunction <SKSEScaleform_SetObjects>(root, view, "SetObjects");
	RegisterFunction <SKSEScaleform_GetObjects>(root, view, "GetObjects");

	RegisterFunction <SKSEScaleform_Remove>(root, view, "Remove");

	RegisterFunction <SKSEScaleform_parse>(root, view, "parse");
	RegisterFunction <SKSEScaleform_stringify>(root, view, "stringify");

	RegisterFunction <hasMember>(root, view, "hasMember");

	//RegisterFunction <TLog>(root, view, "log");

	return true;
}

// Serialization

void Serialization_Revert(SKSESerializationInterface * intfc)
{
	_MESSAGE("Serialization_Revert");
	g_data.clear();
}

const UInt32 kSerializationDataVersion = 1;

void Serialization_Save(SKSESerializationInterface * intfc)
{
	_MESSAGE("Serialization_Save");

	for (iter i = g_data.begin(); i != g_data.end(); ++i) {
		if (intfc->OpenRecord('KEYS', kSerializationDataVersion))
		{
			if (!intfc->WriteRecordData(i->first.c_str(), i->first.length()))
				_MESSAGE("Error writing key record data");
		}
		else
			_MESSAGE("Couldn't open key record");

		if (intfc->OpenRecord('VALS', kSerializationDataVersion))
		{
			if (!intfc->WriteRecordData(i->second.c_str(), i->second.length()))
				_MESSAGE("Error writing value record data");
		}
		else
			_MESSAGE("Couldn't open value record");
	}
	_MESSAGE("Wrote %d kv pairs", g_data.size());
}

void Serialization_Load(SKSESerializationInterface * intfc)
{
	_MESSAGE("Serialization_Load");

	UInt32	type;
	UInt32	version;
	UInt32	length;
	bool	error = false;
	std::string key;
	bool haveKey = false;

	while (!error && intfc->GetNextRecordInfo(&type, &version, &length))
	{
		switch(type)
		{
			case 'KEYS':
			{
				if (version == kSerializationDataVersion && length)
				{
					char* buf = new char[length];
					intfc->ReadRecordData(buf, length);
					key = std::string(buf, buf+length);
					haveKey = true;
					_MESSAGE("read key: %s", key.c_str());
					delete [] buf;
					_MESSAGE("tracing");
				}
				else
				{
					_MESSAGE("Unknown version or zero-length key");
					error = true;
				}
			}
			break;
			case 'VALS':
			{
				_MESSAGE("VALS");
				if (version == kSerializationDataVersion && haveKey)
				{
					if (length) {
						_MESSAGE("read vals");
						char* buf = new char[length];
						intfc->ReadRecordData(buf, length);
						std::string val = std::string(buf, buf+length);
						_MESSAGE("val read");
						g_data[key] = val;
						delete [] buf;
						_MESSAGE("add to map");
					} else {
						g_data[key] = std::string();
					}
					haveKey = false;
				}
				else
				{
					_MESSAGE("Unknown version or no key");
					error = true;
				}
			}
			break;

			default:
				_MESSAGE("unhandled type %08X", type);
				break;
		}
	}
	_MESSAGE("successfully loaded %d kv pairs", g_data.size());
}

// plugin

extern "C"
{

bool SKSEPlugin_Query(const SKSEInterface * skse, PluginInfo * info)
{
	gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim\\SKSE\\junk_serialization_plugin.log");

	// populate info structure
	info->infoVersion =	PluginInfo::kInfoVersion;
	info->name =		"junk serialization plugin";
	info->version =		kPluginVersion;

	_MESSAGE("%s %d, %d", info->name, info->infoVersion, info->version);

	// store plugin handle so we can identify ourselves later
	g_pluginHandle = skse->GetPluginHandle();

	if(skse->isEditor)
	{
		_MESSAGE("loaded in editor, marking as incompatible");

		return false;
	}
	else if(skse->runtimeVersion != RUNTIME_VERSION_1_9_32_0)
	{
		_MESSAGE("unsupported runtime version %08X", skse->runtimeVersion);

		return false;
	}

	// get the scaleform interface and query its version
	g_scaleform = (SKSEScaleformInterface *)skse->QueryInterface(kInterface_Scaleform);
	if(!g_scaleform)
	{
		_MESSAGE("couldn't get scaleform interface");

		return false;
	}

	if(g_scaleform->interfaceVersion < SKSEScaleformInterface::kInterfaceVersion)
	{
		_MESSAGE("scaleform interface too old (%d expected %d)", g_scaleform->interfaceVersion, SKSEScaleformInterface::kInterfaceVersion);

		return false;
	}

	// get the serialization interface and query its version
	g_serialization = (SKSESerializationInterface *)skse->QueryInterface(kInterface_Serialization);
	if(!g_serialization)
	{
		_MESSAGE("couldn't get serialization interface");

		return false;
	}

	if(g_serialization->version < SKSESerializationInterface::kVersion)
	{
		_MESSAGE("serialization interface too old (%d expected %d)", g_serialization->version, SKSESerializationInterface::kVersion);

		return false;
	}

	// ### do not do anything else in this callback
	// ### only fill out PluginInfo and return true/false

	// supported runtime version
	return true;
}

bool SKSEPlugin_Load(const SKSEInterface * skse)
{
	_MESSAGE("load");

	// register scaleform callbacks
	g_scaleform->Register("junk_serialization", RegisterScaleform);

	// register callbacks and unique ID for serialization

	// ### this must be a UNIQUE ID, change this and email me the ID so I can let you know if someone else has already taken it
	g_serialization->SetUniqueID(g_pluginHandle, kSerializationUniqueID);

	g_serialization->SetRevertCallback(g_pluginHandle, Serialization_Revert);
	g_serialization->SetSaveCallback(g_pluginHandle, Serialization_Save);
	g_serialization->SetLoadCallback(g_pluginHandle, Serialization_Load);

	return true;
}

};