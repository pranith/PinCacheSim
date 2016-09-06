// PinBasedCacheHitProfiler.cpp : Defines the exported functions for the DLL application.
//
#include "pin.H"
#include <set>
#include <fstream>
#include <iostream>
#include <assert.h>

#define KB(x) ((x)*1024)
#define MB(x) ((x)*1024*1024)

#define ASSERTM(condition, ...) do { \
	if (!(condition)) { printf(__VA_ARGS__); } \
assert ((condition)); } while(false)

static bool debugging               = false;
static bool insertInCacheHitProfile = false;

namespace CacheSimulator {

  static const size_t cacheLineSizeLog2 = 6;
  static const size_t cacheLineSize     = 1 << cacheLineSizeLog2;

  class CacheHitCounter {

    static const size_t  depthLog2 = 4;
    static const size_t  depth     = 1 << depthLog2;
    size_t  widthLog2;
    size_t  width;
    size_t  widthMask;
    size_t  hits;
    size_t  misses;
    size_t  addressesLen;
    size_t* addresses;
    size_t  maxSize;

    CacheHitCounter & operator =(CacheHitCounter const & CacheHitProfile1);
    CacheHitCounter(CacheHitCounter const &);

  public:
    CacheHitCounter() {}
    CacheHitCounter(size_t maxSizeLog2) {
      maxSize         = size_t(1)<<maxSizeLog2;
      widthLog2       = maxSizeLog2 - cacheLineSizeLog2 - depthLog2;
      width           = size_t(1)<<widthLog2;
      widthMask       = width-1;
      addresses       = new size_t[addressesLen];

      clear();
    }

    void initialize(size_t size) {
      maxSize         = size;
      width           = size / ((1<<depthLog2) * cacheLineSize);
      widthMask       = width-1;
      addressesLen    = depth*width;

      addresses	      = new size_t[addressesLen];

      clear();
    }

    void clear() {
      hits   = 0;
      misses = 0;
      for (size_t i = 0; i < addressesLen; i++) addresses[i] = 0;
    }

    void clearAddresses() 
    {
      memset(addresses, 0, addressesLen * sizeof(size_t));
    }

    ~CacheHitCounter() {
      delete [] addresses;
    }

    void insert(size_t cacheLine, size_t hashedCacheLine) {

      size_t col = hashedCacheLine % width; 
      size_t* c  = &addresses[col*depth];
      size_t  pc = cacheLine;
      size_t  r  = 0;
      for (; r < depth; r++) {
	size_t oldC = c[r];
	c[r] = pc;
	if (oldC == cacheLine) {
	  hits++;
	  return;
	}
	pc = oldC;
      }
      misses++;
    };

    size_t getHits() {
      return hits;
    }

    double getHitRatio() {
      size_t total = hits + misses;

      return (double)hits / total;
    }

    double getMissRatio() {
      size_t total = hits + misses;

      return (double)misses / total;
    }

    size_t getTotalAccesses() { return hits + misses; }

    size_t getCacheSize()     { return maxSize / MB(1); }

    void PrintConfig() {
      printf("CacheSize %lu, width %lu, addressesLen %lu\n", maxSize / MB(1), width, addressesLen);
    }
  };

  class CacheHitProfile {
    // 1M, 2M, 4M, 6M, 8M, 10M, 12M, 14M, 16M
    static const size_t numberOfCacheConfigs = 1;
    CacheHitCounter	_hitCounter[numberOfCacheConfigs];
		
  public:
    CacheHitProfile()
    {
      // 1M, 2M, 4M, 6M, 8M, 10M, 12M, 14M, 16M
      size_t cacheSize = MB(8);
      _hitCounter[0].initialize(cacheSize);
      cacheSize = MB(2);
      //#pragma omp parallel for shared(cacheSize)
      for (size_t configIdx = 1; configIdx < numberOfCacheConfigs; configIdx++) {
	_hitCounter[configIdx].initialize(cacheSize);
	cacheSize += MB(2);
      }
    }

    void PrintGranularity(std::ostream & os) {
      for (size_t configIdx = 0; configIdx < numberOfCacheConfigs; configIdx++) {
	os << ", " << _hitCounter[configIdx].getCacheSize();
      }
    }

    void clear() {
      for (size_t configIdx = 0; configIdx < numberOfCacheConfigs; configIdx++) 
	_hitCounter[configIdx].clear();
    }

    void clearAddresses() {
      for (size_t configIdx = 0; configIdx < numberOfCacheConfigs; configIdx++) 
	_hitCounter[configIdx].clearAddresses();
    }

    void insert(size_t cacheLine) {
      size_t hashedCacheLine = cacheLine ^ (cacheLine>>13);
			  
      for (size_t configIdx = 0; configIdx < numberOfCacheConfigs; configIdx++) 
	_hitCounter[configIdx].insert(cacheLine, hashedCacheLine);
    }

    void PrintConfigs() {
      for (size_t configIdx = 0; configIdx < numberOfCacheConfigs; configIdx++)
	_hitCounter[configIdx].PrintConfig();
    }

    void printHitRatios(std::ostream &os, std::string& name) {
      os << name;
      for (size_t configIdx = 0; configIdx < numberOfCacheConfigs; configIdx++)
	os << "," << _hitCounter[configIdx].getHitRatio();
      os << ", " << _hitCounter[0].getTotalAccesses() << std::endl;
    }
  };

  class AnnotatedSites {

    class Site {
      Site(Site &other);
      Site();

      CacheHitProfile 	*currentCHiP;
      uint32_t		executionCount;

    public:
      std::string	siteName;

      Site(char *name) : executionCount(0)
      {
	currentCHiP  = new CacheHitProfile;
	siteName.assign(name);
      }

      ~Site()
      {
	delete currentCHiP;
      }

      void insert(uint64_t address) {
	currentCHiP->insert(address);
      }

      void PrintStats(std::ostream &os) {
	currentCHiP->printHitRatios(os, siteName);
      }

      void ClearChipAddresses() {
	currentCHiP->clearAddresses();
      }

      void PrintGranularity(std::ostream& os) {
	//currentCHiP->PrintConfigs();
	currentCHiP->PrintGranularity(os);
      }

    };

    class SiteObjPtr {
      SiteObjPtr();
    public:
      Site *site;
    };

    bool			siteActive;

    Site			*currentSite;

    std::set<Site*> sitesHashSet;
  public:
    AnnotatedSites() 
    {
      siteActive      = false;
      currentSite     = NULL;
    }

    ~AnnotatedSites() 
    {
    }

    void recordMemoryAccess(size_t addr)
    {
      currentSite->insert(addr);
    }

    void StartCollection(char* name, void* siteObj)
    {
      siteActive  = true;
      SiteObjPtr *sitePtr = (SiteObjPtr *)siteObj;
      currentSite = sitePtr->site;

      if (currentSite == NULL) {
	std::cout << "Site found : " << name << std::endl;
	currentSite		= new Site(name);
	sitePtr->site	= currentSite;
	sitesHashSet.insert(currentSite);
      }
    }

    void StopCollection(void* siteObj)
    {
      SiteObjPtr *sitePtr = (SiteObjPtr *)siteObj;
      if ((siteActive == false) || (currentSite != sitePtr->site)) {
	std::cerr << "Error: Annotation Mismatch at " << currentSite->siteName << 
	  "! Nested SITEs are not supported. Check your annotations." << std::endl;
	exit(-1);
      }

      siteActive = false;
      currentSite->ClearChipAddresses();
    }

    void PrintStats(std::ostream & os)
    {
      os << "region";
      if (currentSite)
	currentSite->PrintGranularity(os); 
      os << std::endl;
      for (auto it = sitesHashSet.begin(); it != sitesHashSet.end(); it++)
	(*it)->PrintStats(os);
    }
  };
};	// namespace

KNOB<bool> KNOB_RECORD_ALL (KNOB_MODE_WRITEONCE, "pintool",
			    "recordall" , "0", "record all mem insts");
KNOB<string> KNOB_DETAILED_SITE_REPORT (KNOB_MODE_WRITEONCE, "pintool",
					"detailedSiteReport", "detailedSiteReport.csv" ,"detailed report file name");
KNOB<string> KNOB_SITE_REPORT (KNOB_MODE_WRITEONCE, "pintool",
			       "siteReport", "siteReport.csv" ,"report file name");
KNOB<string> KNOB_DETAILED_TASK_REPORT (KNOB_MODE_WRITEONCE, "pintool",
					"detailedTaskReport", "detailedTaskReport.csv" ,"detailed report file name");
KNOB<string> KNOB_TASK_REPORT (KNOB_MODE_WRITEONCE, "pintool",
			       "taskReport", "taskReport.csv" ,"report file name");
std::ofstream siteReportFile;
std::ofstream detailedSiteReportFile;
std::ofstream taskReportFile;
std::ofstream detailedTaskReportFile;
std::ofstream traceInFile;
std::ofstream traceOutFile;

size_t noted(0);
size_t inserted(0);
size_t considered(0);
size_t instrumented(0);
size_t numThreads(0);

static const  size_t maxThreads        = 4;
static size_t cacheLineSizeLog2        = 6;

CacheSimulator::AnnotatedSites annotatedSites;

PIN_LOCK lock, simlock;
static TLS_KEY tlsKey;

class PerThreadAddressStore {
  size_t *addresses;
  size_t  count;
  size_t  max_count;
  size_t  top;
public:
  PerThreadAddressStore() : count(0), top(0) {
    max_count = MB(1) / sizeof(size_t);
    addresses = new size_t[max_count];
  }

  ~PerThreadAddressStore() {
    if (debugging) printf("deleting address store\n");
    delete [] addresses;
  }

  // stores address for the thread and returns if buffer is full
  bool StoreAddress(char* addr, size_t size, int threadId) {
    size_t lo = size_t(addr       ) >> cacheLineSizeLog2;
    size_t hi = size_t(addr+size-1) >> cacheLineSizeLog2;

    for (size_t cacheLine = lo; cacheLine <= hi; cacheLine++) {
      ASSERTM(cacheLine != 0, "cacheline is 0 while inserting\n");
      addresses[count++] = cacheLine;
      traceInFile << hex << cacheLine << std::endl;
    }

    // keep a padding of 64 entries to report buffer filled
    if (count + 64 >= max_count)
      return true;

    return false;
  }

  size_t getAddress() {
    //if (debugging) printf("current store top: %d, count: %d, max_count: %d\n", top, count, max_count);
    if (top < count)
      return addresses[top++];

    top = 0; count = 0;
    return 0;
  }
};

static PerThreadAddressStore* getThreadData(THREADID tid)
{
  //if (debugging) printf("getting thread data for %d\n", tid);
  PerThreadAddressStore *addressStore = 
    static_cast<PerThreadAddressStore*>(PIN_GetThreadData(tlsKey, tid));

  return addressStore;
}

static void printAndClearStats() {
  if (debugging) printf("noted %.0f, inserted %0.f, considered %.0f, instrumented %.0f\n", float(noted), float(inserted), float(considered), float(instrumented));
  noted = 0;
  inserted = 0;
  considered = 0;
  instrumented = 0;
}

class AddressStore {
  PerThreadAddressStore **stores;

  bool   *storeExhausted;
  size_t numStoresExhausted;
  size_t numStores;
  size_t currentIdx;
  AddressStore();

public:
  AddressStore(size_t numThreads) {

    if (debugging) printf("creating address store for %lu threads\n", numThreads);
    numStores			= numThreads;
    stores				= new PerThreadAddressStore*[numThreads];
    storeExhausted		= new bool[numThreads];

    for (size_t i = 0; i < numThreads; i++) {
      stores[i]         =  getThreadData(i);
      storeExhausted[i] =  false;
    }

    numStoresExhausted	= 0;
    currentIdx			= 0;
  }

  ~AddressStore() {
    delete [] stores;
    delete [] storeExhausted;
  }

  void incrementIndex() {
    currentIdx++;
    if (currentIdx == numStores) 
      currentIdx = 0;
  }


  // return false if all the addresses have been exhausted
  bool getNextAddress(size_t *addr) {
		
    while(numStoresExhausted < numStores) {

      if (storeExhausted[currentIdx] == false) {
	*addr = (size_t)stores[currentIdx]->getAddress();
				
	// current store has exhausted
	// move to next store
	if (*addr == 0) { 
	  if (debugging) printf("store with idx %lu exhausted...\n", currentIdx);
	  storeExhausted[currentIdx] = true;
	  numStoresExhausted++;

	  incrementIndex();
	  continue;
	}

	return true;
      }
      else
	incrementIndex();
    }

    return false;
  }
};

static VOID SimulateAddresses()
{
  PIN_LockClient();
  AddressStore addressStore(numThreads);

  size_t addr = 0;
  bool ret;
  while((ret = addressStore.getNextAddress(&addr)) != false) {
    if (addr == 0) {
      ASSERTM(addr != 0, "BUG: addr is 0. ret %d\n", ret);
    }
		
    traceOutFile << hex << addr << std::endl;
    annotatedSites.recordMemoryAccess(addr);
  }

  PIN_UnlockClient();
}

void changeInsertInCacheHitProfile(bool to) {
  if (insertInCacheHitProfile == to) return;

  insertInCacheHitProfile = to;

  if (debugging) printf("PIN_RemoveInstrumentation()\n");
  PIN_RemoveInstrumentation();
}

// ref: http://tech.groups.yahoo.com/group/pinheads/message/3574
static VOID PIN_FAST_ANALYSIS_CALL noteMemoryAccess(CHAR * addr, UINT32 size, UINT32 threadId)
{
  noted++;
  if (insertInCacheHitProfile && (size > 0)) {
    inserted++;
    auto addressStore = getThreadData(threadId);
    if (addressStore == NULL) {
      fprintf(stderr, "getThreadData returned 0 for tid %d\n", threadId);
      assert(0);
    }

    // buffer full
    if (addressStore->StoreAddress(addr, size, threadId) == true) {
      if (debugging) {
	printf("buffer is full, simulating...\n");
	traceInFile  << "buffer is full, simulating...\n" << std::endl;
	traceOutFile << "buffer is full, simulating...\n" << std::endl;
      }
      SimulateAddresses();
    }
  }
}

VOID PIN_FAST_ANALYSIS_CALL startCacheHitProfiling(char *name, void* siteObj)
{
  if (debugging) printf("startCacheHitProfiling(%s)\n", name);

  changeInsertInCacheHitProfile(true);
  annotatedSites.StartCollection(name, siteObj);
}

VOID PIN_FAST_ANALYSIS_CALL stopCacheHitProfiling(void* siteObj)
{
  if (debugging) printf("stopCacheHitProfiling\n");
  if (!insertInCacheHitProfile) return;
  changeInsertInCacheHitProfile(false);

  // flush the current buffers for simulation
  SimulateAddresses();
  annotatedSites.StopCollection(siteObj);
}

VOID PIN_FAST_ANALYSIS_CALL startTaskCacheHitProfile(void* siteObj)
{ 
  assert(0);
  if (debugging) printf("startTaskCacheHitProfiling\n");
}

VOID PIN_FAST_ANALYSIS_CALL stopTaskCacheHitProfile(void* siteObj)
{
  assert(0);
  if (debugging) printf("stopTaskCacheHitProfiling\n");
}

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID *v)
{
  considered++;
  if (debugging) if (considered == 1) printf("Instruction(...) considered\n");

  if (!insertInCacheHitProfile) return;

  instrumented++;
  if (debugging) if (instrumented == 1) printf("Instruction(...) instrumented\n");

  for (bool b = INS_IsMemoryRead(ins); b; b = false) {
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)noteMemoryAccess, IARG_FAST_ANALYSIS_CALL, IARG_MEMORYREAD_EA,  IARG_MEMORYREAD_SIZE, IARG_THREAD_ID, IARG_END);
    if (!INS_HasMemoryRead2(ins)) break;
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)noteMemoryAccess, IARG_FAST_ANALYSIS_CALL, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_THREAD_ID, IARG_END);
  }
  if (INS_IsMemoryWrite(ins)) {
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)noteMemoryAccess, IARG_FAST_ANALYSIS_CALL, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_THREAD_ID, IARG_END);
  }
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
INT32 Usage()
{
  cerr << KNOB_BASE::StringKnobSummary() << endl;

  return -1;
}

VOID Image(IMG img, VOID *v)
{
  string startSiteName("ANNOTATE_SITE_BEGIN_WKR");
  string stopSiteName ("ANNOTATE_SITE_END_WKR");
  string startTaskName("ANNOTATE_TASK_BEGIN_WKR");
  string stopTaskName ("ANNOTATE_TASK_END_WKR");

  for (SYM sym = IMG_RegsymHead(img); SYM_Valid(sym); sym = SYM_Next(sym)) {

    string undFuncName = PIN_UndecorateSymbolName(SYM_Name(sym), UNDECORATION_NAME_ONLY);

    enum {StartSiteCase, StopSiteCase, StartTaskCase, StopTaskCase, OtherFuncCase} funcCase(OtherFuncCase);

    if (debugging)
      cout << "Func " << undFuncName << endl;

    if (undFuncName == startSiteName) funcCase = StartSiteCase;
    if (undFuncName == stopSiteName)  funcCase = StopSiteCase;
    //if (undFuncName == startTaskName) funcCase = StartTaskCase;
    //if (undFuncName == stopTaskName)  funcCase = StopTaskCase;

    if (funcCase == OtherFuncCase) continue;

    RTN rtn = RTN_FindByAddress(SYM_Value(sym));
    if (!RTN_Valid(rtn)) {
      cout << "Turns out to be invalid" << endl;
      continue;
    }


    cout << "Found " << undFuncName << endl;

    AFUNPTR afunptr(0);
    switch (funcCase) {
    case StartSiteCase:  afunptr = (AFUNPTR)startCacheHitProfiling;   break;
    case StopSiteCase:   afunptr = (AFUNPTR)stopCacheHitProfiling;    break;
    case StartTaskCase:  afunptr = (AFUNPTR)startTaskCacheHitProfile; break;
    case StopTaskCase:   afunptr = (AFUNPTR)stopTaskCacheHitProfile;  break;
    default: break;
    };

    RTN_Open(rtn);
    if (funcCase != StopSiteCase)
      RTN_InsertCall(rtn, IPOINT_BEFORE, afunptr,
		     IARG_FAST_ANALYSIS_CALL,
		     IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
		     IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
		     IARG_END);
    else // StopSiteCase
      RTN_InsertCall(rtn, IPOINT_BEFORE, afunptr,
		     IARG_FAST_ANALYSIS_CALL,
		     IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
		     IARG_END);
    RTN_Close(rtn);
  }
  if (debugging) printf("Image() done\n");
}

VOID Fini(INT32 code, VOID *v)
{
  if (debugging) printAndClearStats();
  annotatedSites.PrintStats(cout);
  annotatedSites.PrintStats(siteReportFile);
  siteReportFile.close();
}

VOID InitThreadData(THREADID threadId, CONTEXT *ctxt, INT32 flags, VOID *v)
{
  if (PIN_IsApplicationThread() == false)
    return;

  if (1 || debugging) printf("Creating thread data for tid %d\n", threadId);
  PIN_GetLock(&lock, threadId+1);
  numThreads++;

  PerThreadAddressStore *addressStore = new PerThreadAddressStore();

  PIN_SetThreadData(tlsKey, addressStore, threadId);

  PIN_ReleaseLock(&lock);
}

VOID CleanThreadData(THREADID threadId, const CONTEXT *ctxt, INT32 flags, VOID *v)
{
  if (PIN_IsApplicationThread() == false)
    return;

  if (1 || debugging) printf("Cleaning thread data for tid %d\n", threadId);
  PIN_GetLock(&lock, threadId+1);
  auto addressStore = getThreadData(threadId);

  if (addressStore == NULL) {
    fprintf(stderr, "CleanThreadData: getThreadData returned 0 for tid %d\n", threadId);
    assert(0);
  }

  delete addressStore;

  numThreads--;
  PIN_ReleaseLock(&lock);
}

int main(int argc, char* argv[])
{
  // Initialize pin
  if (PIN_Init(argc, argv)) {
    return Usage();
  }

  cout << "Created Site report in " << KNOB_SITE_REPORT.Value() << endl;
  siteReportFile.open(KNOB_SITE_REPORT.Value().c_str());
  traceOutFile.open("trace.out.txt");
  traceInFile.open("tace.in.txt");

  PIN_InitSymbols();

  tlsKey = PIN_CreateThreadDataKey(0);

  IMG_AddInstrumentFunction(Image, 0);
  // Register Instruction to be called to instrument instructions
  INS_AddInstrumentFunction(Instruction, 0);

  PIN_AddThreadStartFunction(InitThreadData, 0);
  PIN_AddThreadFiniFunction (CleanThreadData, 0);
  // Register Fini to be called when the application exits
  PIN_AddFiniFunction(Fini, 0);

  // Start the program, never returns
  PIN_StartProgram();

  return 0;
}
