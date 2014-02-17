#include "common/IPrefix.h"
#include "common/IFileStream.h"
#include "common/ICriticalSection.h"
#include "skse/ScaleformCallbacks.h"
#include <string>
#include <map>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <shlobj.h>
#include "gfxvalue_visitor.h"
#include "gfxvalue_logdump.h"
#include "log.h"

// closes file on removal of last reference
struct FILEDeleter {
	void operator()(FILE* fp) const {
		fclose(fp);
	}
};

typedef std::tr1::shared_ptr<FILE> FILEPtr;
typedef std::map<std::string, FILEPtr> LogMap;
LogMap g_logMap;
ICriticalSection g_logLock;

static FILE * openLog(const std::string& fileName) {
	// copyin pastin from skse's common stuff
	char path[MAX_PATH];
	ASSERT(SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, path)));
	strcat_s(path, sizeof(path), "\\My Games\\Skyrim\\SKSE\\");
	strcat_s(path, sizeof(path), fileName.c_str());
	strcat_s(path, sizeof(path), ".log");
	IFileStream::MakeAllDirs(path);

	_MESSAGE("Opening log %s", path);

	FILE * fp = _fsopen(path, "w", _SH_DENYWR);
	if (!fp) {
		UInt32 id = 0;
		char name[1024];
		do
		{
			sprintf_s(name, sizeof(name), "%s%d", path, id);
			id++;
			fp = _fsopen(name, "w", _SH_DENYWR);
		} while (!fp && (id < 5));
	}
	return fp;
}

static FILEPtr& getLog(const std::string& fileName) {
	FILEPtr& fp = g_logMap[fileName];
	if (!fp.get()) {
		fp = FILEPtr(openLog(fileName), FILEDeleter());
	}
	return fp;
}

class FLogDump : public LogDump
{
protected:
	virtual void log(const char * message) {
		sout << '[' << now << "] ";
		for (int i = 0; i < level; ++i)
			sout << "    ";
		sout << message << '\n';
	}

	std::stringstream& sout;
	boost::posix_time::ptime& now;
public:
	FLogDump(std::stringstream& sout, boost::posix_time::ptime& now) : LogDump(), sout(sout), now(now) {}
};

static void log(FILEPtr& fp, UInt32 logArg, GFxFunctionHandler::Args * args) {
	std::stringstream sout;
	boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
	boost::posix_time::time_facet* facet = new boost::posix_time::time_facet("%H:%M:%S.%f");
	sout.imbue(std::locale(sout.getloc(), facet));
	FLogDump dumper(sout, now);
	while (logArg < args->numArgs) {
		if (args->args[logArg].GetType() == GFxValue::kType_String) {
			sout << "[" << now << "] " << args->args[logArg].GetString() << '\n';
		} else {
			dumper.dump(&args->args[logArg]);
		}
		logArg++;
	}
	fputs(sout.str().c_str(), fp.get());
	fflush(fp.get());
}

// function log(message:Object ...):Void
class TLog : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args * args)
	{
		ASSERT(args->numArgs >= 1);
		std::string fileName = "junk";
		g_logLock.Enter();
		FILEPtr& fp = getLog(fileName);
		log(fp, 0, args);
		g_logLock.Leave();
	}
};

// function logFn(fileName:String, message:Object ...):Void
class TLogFn : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args * args)
	{
		ASSERT(args->numArgs >= 1);
		ASSERT(args->args[0].GetType() == GFxValue::kType_String);
		std::string fileName = args->args[0].GetString();
		g_logLock.Enter();
		FILEPtr& fp = getLog(fileName);
		log(fp, 1, args);
		g_logLock.Leave();
	}
};

void RegisterLogging(GFxMovieView * view, GFxValue * root)
{
	RegisterFunction <TLog>(root, view, "log");
	RegisterFunction <TLogFn>(root, view, "logFn");
}
