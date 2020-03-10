#include <Windows.h>
#include <map>
#include <DbgHelp.h>
#include <fstream>
#include <vector>
#include <algorithm>

#include "Profiler.h"


bool Profiler::useLineNumbers = false;
const char * Profiler::logFile = "profile.log";
unsigned Profiler::minSamples = 10;
unsigned Profiler::minPercent = 1;
unsigned Profiler::instanceCount = 0;
HANDLE Profiler::mainThread;
HANDLE Profiler::queueTimer;
std::unordered_map<DWORD, unsigned> Profiler::eipLocations;


void InitSymbols()
{
	static bool symbolsInitialized = false;
	if (!symbolsInitialized)
	{
		SymSetOptions(SymGetOptions() | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
		SymInitialize(GetCurrentProcess(), NULL, true);
		symbolsInitialized = true;
	}
}


bool maxSamples(const std::pair<DWORD, unsigned>& lhs, const std::pair<DWORD, unsigned>& rhs)
{
	return lhs.second > rhs.second;
}


Profiler::Profiler(const char * filename, bool enableLineNumbers, unsigned minimumSamples, unsigned minimumCPUUsage)
{
	if (instanceCount == 0)
	{
		logFile = filename ? filename : "profile.log";
		useLineNumbers = enableLineNumbers;
		minSamples = minimumSamples;
		minPercent = minimumCPUUsage;
		++instanceCount;
		init_profiler();
	}
	else
	{
		throw "Error: Instance of 'Profiler' already exists.";
	}
}


Profiler::~Profiler()
{
	//static variables prevent memory leakage in case of exception issues while generating log

	//stop the timer so it doesn't keep functioning outisde the scope of the profiler
	DeleteTimerQueueTimer(NULL, queueTimer, NULL);
	generate_log();
	//clean up global so it can be reused
	eipLocations.clear();
	--instanceCount;
}


void Profiler::init_profiler()
{
	static bool profilerInitialized = false;
	if (!profilerInitialized)
	{
		mainThread = OpenThread(THREAD_SUSPEND_RESUME |
			THREAD_GET_CONTEXT |
			THREAD_QUERY_INFORMATION,
			0,
			GetCurrentThreadId());
		CreateTimerQueueTimer(&queueTimer, NULL, record_eip, 0, 1, 1, 0);
		profilerInitialized = true;
	}
}


void Profiler::generate_log()
{
	unsigned totalSamples = 0;
	std::ofstream profileLog;
	std::unordered_map<DWORD, unsigned> functionCount;

	//make sure symbols have been loaded prior to using them
	InitSymbols();
	profileLog.open(logFile);

	//accumulate eip into address of it's parent function
	for (auto it = eipLocations.begin(); it != eipLocations.end(); ++it)
	{
		PDWORD64 displacement = 0;
		ULONG64 buffer[(sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR) + sizeof(ULONG64) - 1) / sizeof(ULONG64)];
		PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen = MAX_SYM_NAME;
		if (SymFromAddr(GetCurrentProcess(), it->first, displacement, symbol))
		{
			auto funcIt = functionCount.find(symbol->Address);

			//function has been accessed before
			if (funcIt != functionCount.end())
			{
				//add to total samples
				funcIt->second += it->second;
			}
			else
			{
				//new function access
				functionCount.insert(std::make_pair(symbol->Address, it->second));
			}

			totalSamples += it->second;
		}
	}
	
	profileLog << "Total Samples: " << totalSamples << std::endl;
	profileLog << "Minimum Samples to Display: " << minSamples << std::endl;
	profileLog << "Minimum Percent Usage to Display: " << minPercent << std::endl;
	profileLog << std::endl;

	//dump and sort for data organization
	std::vector < std::pair<DWORD, unsigned>> sortedFunctions(functionCount.begin(), functionCount.end());
	std::sort(sortedFunctions.begin(), sortedFunctions.end(), maxSamples);

	for (auto it = sortedFunctions.begin(); it != sortedFunctions.end(); ++it)
	{
		PDWORD64 displacement = 0;
		ULONG64 buffer[(sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR) + sizeof(ULONG64) - 1) / sizeof(ULONG64)];
		PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen = MAX_SYM_NAME;
		if (SymFromAddr(GetCurrentProcess(), it->first, displacement, symbol))
		{
			//display only samples of data that were found more than MIN_SAMPLES
			if (it->second >= minSamples)
			{
				unsigned cpuUsage = (it->second * 100) / totalSamples;

				//display only samples above MIN_PERCENT cpu usage. Not float/double for formatting
				if (cpuUsage >= minPercent)
				{
					if (cpuUsage > 0)
					{
						profileLog << symbol->Name << ": cpu usage: " << cpuUsage << "% samples: " << it->second << std::endl;
					}
					else
					{
						profileLog << symbol->Name << ": cpu usage: <1% samples: " << it->second << std::endl;
					}

					if (useLineNumbers)
					{
						DWORD displacement = 0;
						IMAGEHLP_LINE line;
						line.SizeOfStruct = sizeof(IMAGEHLP_LINE);

						if (SymGetLineFromAddr(GetCurrentProcess(), it->first, &displacement, &line))
						{
							profileLog << "In file: " << line.FileName << ":" << line.LineNumber << std::endl;
						}
					}

					profileLog << std::endl;
				}
			}
		}
	}

}


void CALLBACK Profiler::record_eip(PVOID, BOOLEAN)//UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	SuspendThread(mainThread);
	CONTEXT context = {0};
	context.ContextFlags = CONTEXT_i386 | CONTEXT_CONTROL;
	GetThreadContext(mainThread, &context);
	ResumeThread(mainThread);

	auto it = eipLocations.find(context.Eip);

	//new location in code
	if (it == eipLocations.end())
	{
		eipLocations.insert(std::make_pair(context.Eip, 1));
	}
	//been here before, increase visit count
	else
	{
		++(it->second);
	}
}