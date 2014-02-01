/*
allows user to get/set string keys/values from skse serialization data (tied to savegame) from scaleform

sort of similar to localStorage in html5

strings cannot contain null characters (escape them)

intrinsic class skse {
static var plugins:Object;
}

function skse.plugins.junk_serialization.SetData(key:String, value:String):Void;
function skse.plugins.junk_serialization.GetData(key:String):String;

pretty much copied from the skse plugin_example:
*/
#include "skse/PluginAPI.h"
#include "skse/skse_version.h"
#include "skse/ScaleformCallbacks.h"
#include "skse/ScaleformMovie.h"
#include <vector>
#include <string>
#include <map>
#include <shlobj.h>

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

bool RegisterScaleform(GFxMovieView * view, GFxValue * root)
{
	RegisterFunction <SKSEScaleform_SetData>(root, view, "SetData");
	RegisterFunction <SKSEScaleform_GetData>(root, view, "GetData");

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
						_MESSAGE("val %s", val.c_str());
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
	info->version =		1;

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
	g_serialization->SetUniqueID(g_pluginHandle, 'JUNK');

	g_serialization->SetRevertCallback(g_pluginHandle, Serialization_Revert);
	g_serialization->SetSaveCallback(g_pluginHandle, Serialization_Save);
	g_serialization->SetLoadCallback(g_pluginHandle, Serialization_Load);

	return true;
}

};