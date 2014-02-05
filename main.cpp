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
#include "skse/PluginAPI.h"
#include "skse/skse_version.h"
#include "skse/ScaleformCallbacks.h"
#include "skse/ScaleformMovie.h"
#include "cJSON.h"
#include <vector>
#include <string>
#include <map>
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
			if (root->type == cJSON_Array) {
				args->movie->CreateArray(args->result);

				int len = cJSON_GetArraySize(root);
				_MESSAGE("GetObjects %s (%d items)", args->args[0].GetString(), len);
				for (int i = 0; i < len; ++i) {
					GFxValue gobj;
					cJSON* item = cJSON_GetArrayItem(root, i);
					args->movie->CreateObject(&gobj);

					int np = cJSON_GetArraySize(item);
					for (int j = 0; j < np; ++j) {
						cJSON* jm = cJSON_GetArrayItem(item, j);
						GFxValue gm;
						bool add = true;
						switch (jm->type) {
						case cJSON_True:
							gm.SetBool(true);
							break;
						case cJSON_False:
							gm.SetBool(false);
							break;
						case cJSON_Number:
							gm.SetNumber(jm->valuedouble);
							break;
						case cJSON_String:
							args->movie->CreateString(&gm, jm->valuestring);
							break;
						default:
							add = false;
							break;
						}
						if (add) {
							gobj.SetMember(jm->string, &gm);
						}
					}
					args->result->PushBack(&gobj);
				}
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
		if (args->numArgs < 3 ||
			args->args[0].GetType() != GFxValue::kType_String ||
			args->args[1].GetType() != GFxValue::kType_Array ||
			args->args[2].GetType() != GFxValue::kType_Array) {
				args->result->SetBool(false);
				return;
		}
		_MESSAGE("SetObjects %s (%d items)", args->args[0].GetString(), args->args[1].GetArraySize());
		cJSON* root = cJSON_CreateArray();

		std::vector<std::string> memberNames;
		typedef std::vector<std::string>::iterator miter;
		UInt32 mlen = args->args[2].GetArraySize();
		for (UInt32 i = 0; i < mlen; ++i) {
			GFxValue amem;
			if (args->args[2].GetElement(i, &amem)) {
				_MESSAGE("member %s", amem.GetString());
				memberNames.push_back(amem.GetString());
			}
		}

		UInt32 len = args->args[1].GetArraySize();
		for (UInt32 i = 0; i < len; ++i) {
			GFxValue gobj, gmem;
			args->args[1].GetElement(i, &gobj);
			cJSON* jobj = cJSON_CreateObject();

			for (miter j = memberNames.begin(); j != memberNames.end(); ++j) {
				if (gobj.HasMember(j->c_str())) {
					gobj.GetMember(j->c_str(), &gmem);
					
					switch(gmem.GetType()) {
					case GFxValue::kType_Bool:
						cJSON_AddBoolToObject(jobj, j->c_str(), gmem.GetBool());
						break;
					case GFxValue::kType_Number:
						cJSON_AddNumberToObject(jobj, j->c_str(), gmem.GetNumber());
						break;
					case GFxValue::kType_String:
						cJSON_AddStringToObject(jobj, j->c_str(), gmem.GetString());
						break;
					}
				}
			}

			cJSON_AddItemToArray(root, jobj);
		}

		char* value = cJSON_PrintUnformatted(root);
		g_data[std::string(args->args[0].GetString())] = std::string(value);
		free(value);
		cJSON_Delete(root);
		
		args->result->SetBool(true);
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