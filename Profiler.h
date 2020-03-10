#pragma once
#include <unordered_map>

class Profiler
{
public:

	Profiler(const char * filename = NULL, bool enableLineNumbers = false, unsigned minimumSamples = 10, unsigned minimumCPUUsage = 1);
	~Profiler();

private:

	static bool useLineNumbers;
	static const char * logFile;
	static unsigned minSamples;
	static unsigned minPercent;

	static unsigned instanceCount;
	static HANDLE mainThread;
	static HANDLE queueTimer;
	static std::unordered_map<DWORD, unsigned> eipLocations;

	static void init_profiler();
	static void generate_log();
	static void CALLBACK record_eip(PVOID, BOOLEAN);

};