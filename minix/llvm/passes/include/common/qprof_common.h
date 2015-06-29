#ifndef _QPROF_COMMON_H
#define _QPROF_COMMON_H

#include <cstdlib>
#include <common/util/stdlib.h>

#define QPROF_SEP   ","
#define QPROF_SEP2  ":"
#define QPROF_SEP3  "|"

#define QPROF_DECLARE_LL_SITESTACKS_OPT(P, VAR) \
    static cl::opt<std::string> \
    VAR(#P "-ll-sitestacks", \
        cl::desc("Specify all the long-lived sitestacks on a per-task basis in qprof format"), \
        cl::init(""), cl::NotHidden, cl::ValueRequired)

#define QPROF_DECLARE_DEEPEST_LL_LOOPS_OPT(P, VAR) \
    static cl::opt<std::string> \
    VAR(#P "-deepest-ll-loops", \
        cl::desc("Specify all the deepest long-lived loops on a per-task basis in qprof format"), \
        cl::init(""), cl::NotHidden, cl::ValueRequired)

#define QPROF_DECLARE_DEEPEST_LL_LIBS_OPT(P, VAR) \
    static cl::opt<std::string> \
    VAR(#P "-deepest-ll-libs", \
        cl::desc("Specify all the deepest long-lived loop lib calls on a per-task basis in qprof format"), \
        cl::init(""), cl::NotHidden, cl::ValueRequired)

#define QPROF_DECLARE_TASK_CLASSES_OPT(P, VAR) \
    static cl::opt<std::string> \
    VAR(#P "-task-classes", \
        cl::desc("Specify all the task classes in qprof format"), \
        cl::init(""), cl::NotHidden, cl::ValueRequired)

#define QPROF_DECLARE_ALL_OPTS(P, VAR1, VAR2, VAR3, VAR4) \
    QPROF_DECLARE_LL_SITESTACKS_OPT(P, VAR1); \
    QPROF_DECLARE_DEEPEST_LL_LOOPS_OPT(P, VAR2); \
    QPROF_DECLARE_DEEPEST_LL_LIBS_OPT(P, VAR3); \
    QPROF_DECLARE_TASK_CLASSES_OPT(P, VAR4) \

using namespace llvm;

namespace llvm {

class QProfSite {
  public:
      static QProfSite* get(Module &M, int taskClassID, int taskSiteID, std::string moduleName, int lineNum, std::string functionName, std::string siteName, std::string siteFuncName, int siteDepth, int lib, unsigned long libFlags, bool refreshSite=true);
      static QProfSite* getFromString(Module &M, int taskClassID, int taskSiteID, std::string &siteString, bool refreshSite=true);
      static std::vector<QProfSite*> getFromSitesString(Module &M, std::string &sitesString, bool refreshSites=true);

      void refresh(Module &M);
      std::string toString();
      bool isLoop();
      bool isFunction();
      bool isCallsite();
      bool isLibCallsite();
      bool equals(QProfSite *other);

      int taskClassID;
      int taskSiteID;
      std::string moduleName;
      int lineNum;
      std::string functionName;
      std::string siteName;
      std::string siteFuncName;
      unsigned long libFlags;
      Function *function;
      Function *siteFunction;
      Instruction *siteInstruction;
  private:
      QProfSite() {}

      int siteDepth;
      int lib;
};

class QProfConf {
  public:
      static QProfConf* get(Module &M, std::string *llSitestacks, std::string *deepestLLLoops, std::string *deepestLLLibs, std::string *taskClasses, bool refreshSites=true);

      std::map<int, std::vector<QProfSite*> > getTaskClassLLSitestacks();
      std::map<int, QProfSite*> getTaskClassDeepestLLLoops();
      std::map<int, std::vector<QProfSite*> > getTaskClassDeepestLLLibs();
      int getNumTaskClasses();
      int getNumLLTaskClasses();
      int getNumLLBlockExtTaskClasses();
      int getNumLLBlockIntTaskClasses();
      int getNumLLBlockExtLibs();
      int getNumLLBlockIntLibs();

      std::vector<QProfSite*> getLLFunctions();
      std::vector<QProfSite*> getDeepestLLLoops();
      std::vector<QProfSite*> getDeepestLLLibs();
      bool lookupTaskClassLibFlags(int taskClassID, int *libFlags);
      void mergeTaskClassLLSitestacks(int taskClassID, int otherTaskClassID);
      void mergeTaskClassDeepestLLLoops(int taskClassID, int otherTaskClassID);
      void mergeTaskClassDeepestLLLibs(int taskClassID, int otherTaskClassID);
      void mergeTaskClassPair(int taskClassID, int otherTaskClassID);
      void mergeAllTaskClassesWithSameDeepestLLLoops();
      void print(raw_ostream &O);
      void printSiteList(raw_ostream &O, std::vector<QProfSite*> &list);

  private:
      QProfConf() {}

      static std::map<int, std::vector<QProfSite*> > parseTaskClassSiteList(Module &M, std::string &str, bool refreshSites=true);
      static std::map<int, QProfSite*> parseTaskClassSite(Module &M, std::string &str, bool refreshSites=true);
      static std::vector<int> parseIntList(std::string &str);

      std::map<int, std::vector<QProfSite*> > taskClassLLSitestacks;
      std::map<int, QProfSite*> taskClassDeepestLLLoops;
      std::map<int, std::vector<QProfSite*> > taskClassDeepestLLLibs;
      int numTaskClasses;
      int numLLTaskClasses;
      int numLLBlockExtTaskClasses;
      int numLLBlockIntTaskClasses;
      int numLLBlockExtLibs;
      int numLLBlockIntLibs;
};

static int stringRefToInt(StringRef &ref)
{
    return atoi(ref.str().c_str());
}

inline QProfSite* QProfSite::get(Module &M, int taskClassID, int taskSiteID,
    std::string moduleName, int lineNum, std::string functionName,
    std::string siteName, std::string siteFuncName, int siteDepth, int lib,
    unsigned long libFlags, bool refreshSite)
{
    QProfSite *site = new QProfSite();
    site->taskClassID = taskClassID;
    site->taskSiteID = taskSiteID;
    site->moduleName = moduleName;
    site->lineNum = lineNum;
    site->functionName = functionName;
    site->siteName = siteName;
    site->siteFuncName = siteFuncName;
    site->siteDepth = siteDepth;
    site->lib = lib;
    site->libFlags = libFlags;
    site->function = NULL;
    site->siteFunction = NULL;
    site->siteInstruction = NULL;
    if (refreshSite) {
        site->refresh(M);
    }

    return site;
}

inline QProfSite* QProfSite::getFromString(Module &M, int taskClassID, int taskSiteID,
    std::string &siteString, bool refreshSite)
{
    StringRef ref(siteString);
    SmallVector< StringRef, 3 > tokenVector;
    if (!siteString.compare("")) {
        return NULL;
    }
    ref.split(tokenVector, QPROF_SEP3);
    assert(tokenVector.size() == 8);
    return get(M, taskClassID, taskSiteID, tokenVector[0], stringRefToInt(tokenVector[1]),
        tokenVector[2], tokenVector[3], tokenVector[4],
        stringRefToInt(tokenVector[5]), stringRefToInt(tokenVector[6]),
        stringRefToInt(tokenVector[7]), refreshSite);
}

inline std::vector<QProfSite*> QProfSite::getFromSitesString(Module &M,
    std::string &sitesString, bool refreshSites)
{
    unsigned i;
    int taskClassID;
    std::vector<QProfSite*> sites;
    StringRef ref(sitesString);
    SmallVector< StringRef, 3 > tokenVector;
    if (!sitesString.compare("")) {
        return sites;
    }
    ref.split(tokenVector, QPROF_SEP2);
    assert(tokenVector.size() > 1);
    taskClassID = stringRefToInt(tokenVector[0]);
    for (i=1;i<tokenVector.size();i++) {
        std::string token = tokenVector[i].str();
        QProfSite *site = QProfSite::getFromString(M, taskClassID, i, token,
            refreshSites);
        sites.push_back(site);
    }
    return sites;
}

inline void QProfSite::refresh(Module &M)
{
    BasicBlock *siteBB = NULL;
    function = NULL;
    siteFunction = NULL;
    siteInstruction = NULL;
    function = M.getFunction(functionName);
    siteFunction = M.getFunction(siteFuncName);
    if (!siteFunction) {
        errs() << "Function " << siteFuncName << " not found. Invalid qprof profiling data?\n";
    }
    assert(siteFunction);
    for (Function::iterator BB = siteFunction->begin(),
        e = siteFunction->end(); BB != e; ++BB) {
        if (!BB->getName().compare(siteName)) {
            siteBB = BB;
            break;
        }
    }
    assert(siteBB);
    if (isCallsite()) {
        for (BasicBlock::iterator it = siteBB->begin(); it != siteBB->end(); ++it) {
            CallSite CS(it);
            if (CS.getInstruction() && CS.getCalledFunction() == function) {
                siteInstruction = it;
                break;
            }
        }
        assert(siteInstruction && "Invalid qprof callsite?");
    }
    else {
        siteInstruction = &siteBB->front();
    }
}

inline std::string QProfSite::toString()
{
    std::string str;
    raw_string_ostream ostream(str);
    ostream << taskClassID << QPROF_SEP3 << taskSiteID << QPROF_SEP3 << moduleName << QPROF_SEP3 << lineNum << QPROF_SEP3 << functionName << QPROF_SEP3 << siteName << QPROF_SEP3 << siteFuncName << QPROF_SEP3 << siteDepth << QPROF_SEP3 << lib << QPROF_SEP3 << libFlags;
    ostream.flush();
    return str;
}

inline bool QProfSite::isLoop()
{
    return siteDepth > 0;
}

inline bool QProfSite::isFunction()
{
    return !isLoop() && !isCallsite();
}

inline bool QProfSite::isCallsite()
{
    return isLibCallsite();
}

inline bool QProfSite::isLibCallsite()
{
    return lib != 0;
}

inline bool QProfSite::equals(QProfSite *other)
{
     if (lineNum != other->lineNum) {
         return false;
     }
     if (libFlags != other->libFlags) {
         return false;
     }
     if (moduleName.compare(other->moduleName)) {
         return false;
     }
     if (functionName.compare(other->functionName)) {
         return false;
     }
     if (siteName.compare(other->siteName)) {
         return false;
     }
     if (siteFuncName.compare(other->siteFuncName)) {
         return false;
     }
     return true;
}

inline QProfConf* QProfConf::get(Module &M, std::string *llSitestacks,
    std::string *deepestLLLoops, std::string *deepestLLLibs,
    std::string *taskClasses, bool refreshSites)
{
    std::vector<int> intValues;
    QProfConf *conf = new QProfConf();
    if (llSitestacks) {
        conf->taskClassLLSitestacks = parseTaskClassSiteList(M,
            *llSitestacks, refreshSites);
    }
    if (deepestLLLoops) {
        conf->taskClassDeepestLLLoops = parseTaskClassSite(M,
            *deepestLLLoops, refreshSites);
    }
    if (deepestLLLibs) {
        conf->taskClassDeepestLLLibs = parseTaskClassSiteList(M,
            *deepestLLLibs, refreshSites);
    }
    if (taskClasses) {
        intValues = parseIntList(*taskClasses);
    }
    if (intValues.size() > 0) {
        assert(intValues.size() == 6);
        conf->numTaskClasses = intValues[0];
        conf->numLLTaskClasses = intValues[1];
        conf->numLLBlockExtTaskClasses = intValues[2];
        conf->numLLBlockIntTaskClasses = intValues[3];
        conf->numLLBlockExtLibs = intValues[4];
        conf->numLLBlockIntLibs = intValues[5];
    }
    else {
        conf->numTaskClasses = 0;
        conf->numLLTaskClasses = 0;
        conf->numLLBlockExtTaskClasses = 0;
        conf->numLLBlockIntTaskClasses = 0;
        conf->numLLBlockExtLibs = 0;
        conf->numLLBlockIntLibs = 0;
    }

    return conf;
}

inline std::map<int, std::vector<QProfSite*> > QProfConf::getTaskClassLLSitestacks()
{
    return taskClassLLSitestacks;
}

inline std::map<int, QProfSite*> QProfConf::getTaskClassDeepestLLLoops()
{
    return taskClassDeepestLLLoops;
}

inline std::map<int, std::vector<QProfSite*> > QProfConf::getTaskClassDeepestLLLibs()
{
    return taskClassDeepestLLLibs;
}

inline int QProfConf::getNumTaskClasses()
{
    return numTaskClasses;
}

inline int QProfConf::getNumLLTaskClasses()
{
    return numLLTaskClasses;
}

inline int QProfConf::getNumLLBlockExtTaskClasses()
{
    return numLLBlockExtTaskClasses;
}

inline int QProfConf::getNumLLBlockIntTaskClasses()
{
    return numLLBlockIntTaskClasses;
}

inline int QProfConf::getNumLLBlockExtLibs()
{
    return numLLBlockExtLibs;
}

inline int QProfConf::getNumLLBlockIntLibs()
{
    return numLLBlockIntLibs;
}

inline std::vector<QProfSite*> QProfConf::getLLFunctions()
{
    std::vector<QProfSite*> sites;
    std::map<int, std::vector<QProfSite*> >::iterator it;
    it = taskClassLLSitestacks.begin();
    for (; it != taskClassLLSitestacks.end(); it++) {
        std::vector<QProfSite*> *siteVector = &it->second;
        for (unsigned i=0;i<siteVector->size();i++) {
            QProfSite* site = (*siteVector)[i];
            if (site->isFunction()) {
                sites.push_back((*siteVector)[i]);
            }
        }
    }
    return sites;
}

inline std::vector<QProfSite*> QProfConf::getDeepestLLLoops()
{
    std::vector<QProfSite*> sites;
    std::map<int, QProfSite*>::iterator it;
    it = taskClassDeepestLLLoops.begin();
    for (; it != taskClassDeepestLLLoops.end(); it++) {
        sites.push_back(it->second);
    }
    return sites;
}

inline std::vector<QProfSite*> QProfConf::getDeepestLLLibs()
{
    std::vector<QProfSite*> sites;
    std::map<int, std::vector<QProfSite*> >::iterator it;
    it = taskClassDeepestLLLibs.begin();
    for (; it != taskClassDeepestLLLibs.end(); it++) {
        std::vector<QProfSite*> *siteVector = &it->second;
        for (unsigned i=0;i<siteVector->size();i++) {
            sites.push_back((*siteVector)[i]);
        }
    }
    return sites;
}

inline bool QProfConf::lookupTaskClassLibFlags(int taskClassID, int *libFlags)
{
    bool found = false;
    std::vector<QProfSite*> deepestLLLibs = getDeepestLLLibs();

    *libFlags = 0;
    for (unsigned i=0;i<deepestLLLibs.size();i++) {
        QProfSite *site = deepestLLLibs[i];
        if (site->taskClassID == taskClassID) {
            *libFlags |= site->libFlags;
            found = true;
        }
    }
    return found;
}

inline void QProfConf::mergeTaskClassLLSitestacks(int taskClassID, int otherTaskClassID)
{
    size_t erased = taskClassLLSitestacks.erase(otherTaskClassID);
    assert(erased == 1);
}

inline void QProfConf::mergeTaskClassDeepestLLLoops(int taskClassID, int otherTaskClassID)
{
    size_t erased = taskClassDeepestLLLoops.erase(otherTaskClassID);
    assert(erased == 1);
}

inline void QProfConf::mergeTaskClassDeepestLLLibs(int taskClassID, int otherTaskClassID)
{
    size_t erased = taskClassDeepestLLLibs.erase(otherTaskClassID);
    assert(erased == 1);
}

inline void QProfConf::mergeTaskClassPair(int taskClassID,
    int otherTaskClassID)
{
    int libFlags;
    mergeTaskClassLLSitestacks(taskClassID, otherTaskClassID);
    mergeTaskClassDeepestLLLoops(taskClassID, otherTaskClassID);
    mergeTaskClassDeepestLLLibs(taskClassID, otherTaskClassID);

    numTaskClasses--;
    if (lookupTaskClassLibFlags(taskClassID, &libFlags)) {
        numLLTaskClasses--;
        if (libFlags & _UTIL_STLIB_FLAG(STLIB_BLOCK_EXT)) {
            numLLBlockExtTaskClasses--;
        }
        else {
            numLLBlockIntTaskClasses--;
        }
    }
}

inline void QProfConf::mergeAllTaskClassesWithSameDeepestLLLoops()
{
    std::vector<QProfSite*> deepestLLLoops = getDeepestLLLoops();
    std::vector<std::pair<QProfSite*, QProfSite*> > loopPairs;

    for (unsigned i=0;i<deepestLLLoops.size();i++) {
        QProfSite *site = deepestLLLoops[i];
        for (unsigned j=0;j<i;j++) {
            if (site->equals(deepestLLLoops[j])) {
                loopPairs.push_back(std::pair<QProfSite*, QProfSite*>(site, deepestLLLoops[j]));
            }
        }
    }
    for (unsigned i=0;i<loopPairs.size();i++) {
        int taskClassID = loopPairs[i].first->taskClassID;
        int otherTaskClassID = loopPairs[i].second->taskClassID;
        mergeTaskClassPair(taskClassID, otherTaskClassID);
    }
}

inline void QProfConf::print(raw_ostream &O)
{
    std::vector<QProfSite*> list;
    O << "*** QProfConf:\n";
    O << " - numTaskClasses=" << getNumTaskClasses() << "\n";
    O << " - numLLTaskClasses=" << getNumLLTaskClasses() << "\n";
    O << " - numLLBlockExtTaskClasses=" << getNumLLBlockExtTaskClasses() << "\n";
    O << " - numLLBlockIntTaskClasses=" << getNumLLBlockIntTaskClasses() << "\n";
    O << " - numLLBlockExtLibs=" << getNumLLBlockExtLibs() << "\n";
    O << " - numLLBlockIntLibs=" << getNumLLBlockIntLibs() << "\n";
    list =  getLLFunctions();
    O << " - LLFunctions="; printSiteList(O, list); O << "\n";
    list = getDeepestLLLoops();
    O << " - deepestLLLoops="; printSiteList(O, list); O << "\n";
    list = getDeepestLLLibs();
    O << " - deepestLLLibs="; printSiteList(O, list); O << "\n";
}

inline void QProfConf::printSiteList(raw_ostream &O, std::vector<QProfSite*> &list)
{
    for (std::vector<QProfSite*>::iterator it = list.begin(); it != list.end(); it++) {
        QProfSite* site = *it;
        if (it != list.begin()) {
            O << ", ";
        }
        O << "{ ";
        O << site->toString();
        O << " }";
    }
}

inline std::map<int, std::vector<QProfSite*> > QProfConf::parseTaskClassSiteList(Module &M,
    std::string &str, bool refreshSites)
{
    std::map<int, std::vector<QProfSite*> > siteListMap;
    StringRef ref(str);
    SmallVector< StringRef, 3 > tokenVector;
    if (!str.compare("")) {
        return siteListMap;
    }
    ref.split(tokenVector, QPROF_SEP);
    for (unsigned i=0;i<tokenVector.size();i++) {
        std::string token = tokenVector[i].str();
        std::vector<QProfSite*> sites = QProfSite::getFromSitesString(M,
            token, refreshSites);
        if (sites.size() > 0) {
            int taskClassID = sites[0]->taskClassID;
            siteListMap.insert(std::pair<int, std::vector<QProfSite*> >(taskClassID,
                sites));
        }
    }
    return siteListMap;
}

inline std::map<int, QProfSite*> QProfConf::parseTaskClassSite(Module &M,
    std::string &str, bool refreshSites)
{
    std::map<int, std::vector<QProfSite*> >::iterator it;
    std::map<int, std::vector<QProfSite*> > siteListMap =
        parseTaskClassSiteList(M, str, refreshSites);
    std::map<int, QProfSite*> siteMap;
    for (it=siteListMap.begin();it!=siteListMap.end();it++) {
        std::vector<QProfSite*> list = it->second;
        assert(list.size() == 1);
        siteMap.insert(std::pair<int, QProfSite*>(it->first, list[0]));
    }

    return siteMap;
}

inline std::vector<int> QProfConf::parseIntList(std::string &str)
{
    std::vector<int> intValues;
    StringRef ref(str);
    SmallVector< StringRef, 3 > tokenVector;
    if (!str.compare("")) {
        return intValues;
    }
    ref.split(tokenVector, QPROF_SEP);
    for (unsigned i=0;i<tokenVector.size();i++) {
        intValues.push_back(stringRefToInt(tokenVector[i]));
    }

    return intValues;
}

}

#endif /* _QPROF_COMMON_H */
