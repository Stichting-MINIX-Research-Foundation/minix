#include <sectionify/SectionifyPass.h>

using namespace llvm;

static cl::list<std::string>
sectionMapOpt("sectionify-section-map",
    cl::desc("Specify all the comma-separated symbol_regex/section pairs which need to be sectionified. \"NULL\" section doesn't sectionify."),
    cl::ZeroOrMore, cl::CommaSeparated, cl::NotHidden, cl::ValueRequired);

static cl::list<std::string>
functionSectionMapOpt("sectionify-function-section-map",
    cl::desc("Specify all the comma-separated function_regex/section pairs which need to be sectionified"),
    cl::ZeroOrMore, cl::CommaSeparated, cl::NotHidden, cl::ValueRequired);

static cl::list<std::string>
dataSectionMapOpt("sectionify-data-section-map",
    cl::desc("Specify all the comma-separated data_regex/section pairs which need to be sectionified"),
    cl::ZeroOrMore, cl::CommaSeparated, cl::NotHidden, cl::ValueRequired);

static cl::opt<std::string>
prefixOpt("sectionify-prefix",
    cl::desc("Specify the prefix-string for prefixing names of global variables and functions"),
    cl::Optional, cl::NotHidden, cl::ValueRequired);

static cl::opt<bool>
sectionifyExternalFunctions("sectionify-function-external",
    llvm::cl::desc("Also sectionify external functions"),
    cl::init(false));

static cl::opt<bool>
sectionifyExternalData("sectionify-data-external",
    llvm::cl::desc("Also sectionify external data"),
    cl::init(false));

static cl::opt<bool>
sectionifySkipReadOnlyData("sectionify-data-skip-read-only",
    llvm::cl::desc("Don't sectionify read-only data"),
    cl::init(false));

static cl::opt<bool>
sectionifyTLSData("sectionify-data-tls",
    llvm::cl::desc("Also sectionify tls data"),
    cl::init(false));

static cl::opt<bool>
sectionifyNoOverride("sectionify-no-override",
    llvm::cl::desc("Do not override existing sections"),
    cl::init(false));

STATISTIC(numSectionifiedGVs, "Number of sectionified global variables");
STATISTIC(numSectionifiedFuncs, "Number of sectionified functions");

namespace llvm {

//===----------------------------------------------------------------------===//
// Constructors, destructor, and operators
//===----------------------------------------------------------------------===//

SectionifyPass::SectionifyPass() : ModulePass(ID) {}
//===----------------------------------------------------------------------===//
// Public methods
//===----------------------------------------------------------------------===//

bool SectionifyPass::runOnModule(Module &M) {
    bool sectionified = false;
    Module::GlobalListType &globalList = M.getGlobalList();
    Module::FunctionListType &functionList = M.getFunctionList();

    functionSectionMapOpt.insert(functionSectionMapOpt.end(), sectionMapOpt.begin(), sectionMapOpt.end());
    dataSectionMapOpt.insert(dataSectionMapOpt.end(), sectionMapOpt.begin(), sectionMapOpt.end());
    parseAndInitRegexMap(functionSectionMapOpt, functionRegexList, functionRegexMap, "function");
    parseAndInitRegexMap(dataSectionMapOpt, dataRegexList, dataRegexMap, "data");

    moduleName = "";
    if ("" != prefixOpt && "%MODULE_NAME%" == prefixOpt)
    {
	PassUtil::getModuleName(M, NULL, NULL, &moduleName);
    }

    for (Module::global_iterator it = globalList.begin(); it != globalList.end(); ++it) {
        GlobalVariable *GV = it;
        if (GV->isDeclaration() && !sectionifyExternalData) {
            DEBUG(errs() << "Skipping external GlobalVariable " << GV->getName() << "\n");
            continue;
        }
        if (GV->isConstant() && sectionifySkipReadOnlyData) {
            DEBUG(errs() << "Skipping read-only GlobalVariable " << GV->getName() << "\n");
            continue;
        }
        if (GV->isThreadLocal() && !sectionifyTLSData) {
            DEBUG(errs() << "Skipping TLS GlobalVariable " << GV->getName() << "\n");
            continue;
        }
        if (GV->hasSection() && sectionifyNoOverride) {
            DEBUG(errs() << "Skipping sectionified GlobalVariable " << GV->getName() << "\n");
            continue;
        }
        if (GV->getName().startswith("llvm.")) {
            DEBUG(errs() << "Skipping LLVM instrinsic GlobalVariable " << GV->getName() << "\n");
            continue;
        }
        if (sectionify(GV, dataRegexList, dataRegexMap)) {
            numSectionifiedGVs++;
            sectionified = true;
        }
    }
    for (Module::iterator it = functionList.begin(); it != functionList.end(); ++it) {
        Function *F = it;
        if (F->isDeclaration() && !sectionifyExternalFunctions) {
            DEBUG(errs() << "Skipping external Function " << F->getName() << "\n");
            continue;
        }
        if (F->hasSection() && sectionifyNoOverride) {
            DEBUG(errs() << "Skipping sectionified Function " << F->getName() << "\n");
            continue;
        }
        if (F->getName().startswith("llvm.")) {
            DEBUG(errs() << "Skipping LLVM instrinsic Function " << F->getName() << "\n");
            continue;
        }
        if (sectionify(F, functionRegexList, functionRegexMap)) {
            numSectionifiedFuncs++;
            sectionified = true;
        }
    }

    return sectionified;
}

bool SectionifyPass::sectionifyFromRegex(GlobalObject *value, Regex *regex, std::string &section)
{
    bool returnValue = false;

    if("NULL" != section && regex->match(value->getName(), NULL)) {
	std::string trgSection = section;
	std::string valueStrPrefix = "";
	GlobalVariable *GV = dyn_cast<GlobalVariable>(value);
	if (GV && GV->isConstant()) {
            trgSection += "_ro";
            valueStrPrefix = "read-only ";
	}
	DEBUG(errs() << "Sectionified " << valueStrPrefix << (isa<Function>(value) ? "Function " : "GlobalVariable ") << value->getName() << " with section " << trgSection << "\n");
        value->setSection(trgSection);
        if (value->hasCommonLinkage()) {
            value->setLinkage(GlobalObject::WeakAnyLinkage);
        }
        returnValue = true;
    }
    if ("" != prefixOpt)
    {
	std::string originalName = value->getName();
	std::string prefixString = "";
        if ("" != moduleName)
	{
	   prefixString = moduleName;
	}
	else
	{
	   prefixString = prefixOpt;
	}
	DEBUG(errs() << "Prefixing the " << (isa<Function>(value) ? "Function name " : "GlobalVariable name ") << originalName << " with " << prefixString << "\n");
	std::string prefixedName = prefixString + originalName;
	value->setName(prefixedName);
	returnValue = true;
    }

    return returnValue;
}

bool SectionifyPass::sectionify(GlobalObject *value, std::vector<Regex*> &regexList, std::map<Regex*, std::string> &regexMap)
{
    std::map<Regex*, std::string>::iterator regexMapIt;
    for (std::vector<Regex*>::iterator it = regexList.begin(); it != regexList.end(); ++it) {
	Regex *regex = *it;
	regexMapIt = regexMap.find(regex);
	assert(regexMapIt != regexMap.end());
	std::string section = regexMapIt->second;
	if (sectionifyFromRegex(value, regex, section)) {
		return true;
	}
    }

    return false;
}

bool SectionifyPass::parseStringMapOpt(std::map<std::string, std::string> &map, std::vector<std::string> &keyList, std::vector<std::string> &stringList)
{
    for (std::vector<std::string>::iterator it = stringList.begin(); it != stringList.end(); ++it) {
	StringRef token = *it;
	SmallVector< StringRef, 2 > tokenVector;
        token.split(tokenVector, "/");
        if(tokenVector.size() != 2) {
	    return false;
        }
        StringRef value = tokenVector.pop_back_val();
        StringRef key = tokenVector.pop_back_val();
        map.insert( std::pair<std::string, std::string>(key, value) );
        keyList.push_back(key);
    }

    return true;
}

void SectionifyPass::parseAndInitRegexMap(cl::list<std::string> &stringListOpt, std::vector<Regex*> &regexList, std::map<Regex*, std::string> &regexMap, std::string regexType)
{
    std::map<std::string, std::string> sectionMap;
    std::vector<std::string> sectionList;
    if (!parseStringMapOpt(sectionMap, sectionList, stringListOpt) || !initRegexMap(regexMap, regexList, sectionMap, sectionList, regexType)) {
        stringListOpt.error("Invalid format!");
        exit(1);
    }
}

bool SectionifyPass::initRegexMap(std::map<Regex*, std::string> &regexMap, std::vector<Regex*> &regexList, std::map<std::string, std::string> &stringMap, std::vector<std::string> &stringList, std::string regexType)
{
    std::map<std::string, std::string>::iterator stringMapIt;
    for (std::vector<std::string>::iterator it = stringList.begin(); it != stringList.end(); ++it) {
	std::string key = *it;
	stringMapIt = stringMap.find(key);
	assert(stringMapIt != stringMap.end());
	std::string value = stringMapIt->second;
	std::string error;
	Regex *regex = new Regex(key, 0);
	if(!regex->isValid(error)) {
		return false;
	}
	DEBUG(errs() << "Using " << regexType << " regex " << key << " with section " << value << "\n");
        regexMap.insert(std::pair<Regex*, std::string>(regex, value));
        regexList.push_back(regex);
    }

    return true;
}

} // end namespace

char SectionifyPass::ID = 1;
static RegisterPass<SectionifyPass> AP("sectionify", "Sectionify Pass");
