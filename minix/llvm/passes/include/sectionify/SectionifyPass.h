#ifndef SECTIONIFY_PASS_H

#define SECTIONIFY_PASS_H

#include <pass.h>

using namespace llvm;

namespace llvm {

class SectionifyPass : public ModulePass {

  public:
      static char ID;

      SectionifyPass();

      virtual bool runOnModule(Module &M);

  private:
      std::map<Regex*, std::string> functionRegexMap;
      std::vector<Regex*> functionRegexList;
      std::map<Regex*, std::string> dataRegexMap;
      std::vector<Regex*> dataRegexList;
      std::string moduleName;

      bool sectionifyFromRegex(GlobalObject *value, Regex *regex, std::string &section);
      bool sectionify(GlobalObject *value, std::vector<Regex*> &regexList, std::map<Regex*, std::string> &regexMap);
      void parseAndInitRegexMap(cl::list<std::string> &stringListOpt, std::vector<Regex*> &regexList, std::map<Regex*, std::string> &regexMap, std::string regexType);
      bool initRegexMap(std::map<Regex*, std::string> &regexMap, std::vector<Regex*> &regexList, std::map<std::string, std::string> &stringMap, std::vector<std::string> &stringList, std::string regexType);
      bool parseStringMapOpt(std::map<std::string, std::string> &map, std::vector<std::string> &keyList, std::vector<std::string> &stringList);
};

}

#endif
