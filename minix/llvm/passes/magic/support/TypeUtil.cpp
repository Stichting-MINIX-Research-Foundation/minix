#include <magic/support/TypeUtil.h>

using namespace llvm;

namespace llvm {

//===----------------------------------------------------------------------===//
// Public static methods
//===----------------------------------------------------------------------===//

bool TypeUtil::isPaddedType(TYPECONST Type *type) {
    if(type->getNumContainedTypes() < 2) {
        return false;
    }
    TYPECONST Type *lastContainedType = type->getContainedType(type->getNumContainedTypes() - 1);
    bool paddedTy = lastContainedType->isIntegerTy() || (lastContainedType->isArrayTy() &&
        lastContainedType->getContainedType(0)->isIntegerTy());
    return paddedTy;
}

TYPECONST Type* TypeUtil::lookupTopStructType(TYPECONST Type *type, unsigned index) {
    static unsigned level = 0;
    static unsigned structsLeft;

    if(level == 0) {
        structsLeft = index;
    }

    if(type->isStructTy() || TypeUtil::isOpaqueTy(type)) {
        if(structsLeft == 0) {
            return type;
        }
        else {
            structsLeft--;
            return NULL;
        }
    }
    unsigned numContainedTypes = type->getNumContainedTypes();
    for(unsigned i=0;i<numContainedTypes;i++) {
        TYPECONST Type *containedType = type->getContainedType(i);
        level++;
        TYPECONST Type *topStructType = lookupTopStructType(containedType, index);
        level--;
        if(topStructType != NULL) {
            return topStructType;
        }
    }
    return NULL;
}

void TypeUtil::parseTopStructTypes(Module &M, TYPECONST Type *type, std::vector<std::string> *names, std::vector<unsigned> *flags) {
    std::string string;
    raw_string_ostream ostream(string);
    EDIType::writeTypeSymbolic(ostream, type, &M);
    ostream.flush();
    Regex anonRegex("%(union|struct)\\.(\\.*[0-9]*anon)", 0);
    Regex regularRegex("%(union|struct)\\.([^{}(), *]+)", 0);
    Regex unnamedRegex("%(%)?([0-9]+)", 0);
    std::string error;
    assert(anonRegex.isValid(error) && regularRegex.isValid(error) && unnamedRegex.isValid(error));
    size_t index = -1;
    while((index=string.find("%", index+1))!=std::string::npos) {
        std::string entryString = string.substr(index);
        if(entryString[entryString.size()-1] == ']') {
            entryString = entryString.substr(0, entryString.size()-1);
        }
        StringRef entryStringRef(entryString);
        SmallVector<StringRef, 8> entryMatches;
        unsigned entryFlags;
        entryMatches.clear();
        entryFlags = 0;
        if(anonRegex.match(entryString, &entryMatches)) {
            entryFlags |= TypeUtil::TYPE_ANONYMOUS;
        }
        else if(unnamedRegex.match(entryString, &entryMatches)) {
            entryFlags |= TypeUtil::TYPE_UNNAMED;
        }
        else {
            assert(regularRegex.match(entryString, &entryMatches) && "Unsupported struct type");
        }
        assert(entryStringRef.startswith(entryMatches[0]));
        std::string prefix = entryMatches[1];
        std::string name = entryMatches[2];
        entryFlags |= !prefix.compare("union") ? TypeUtil::TYPE_UNION : TypeUtil::TYPE_STRUCT;
        if(names) names->push_back(name);
        if(flags) flags->push_back(entryFlags);
    }
}

int TypeUtil::findTopStructTypeIndex(Module &M, TYPECONST Type *type, std::string &name, unsigned flagsToAccept) {
    std::vector<std::string> names;
    std::vector<unsigned> flags;
    parseTopStructTypes(M, type, &names, &flags);
    int index = -1;
    for(unsigned i=0;i<names.size();i++) {
        if(!name.compare(names[i]) && (flagsToAccept | flags[i]) == flagsToAccept) {
            index = i;
            break;
        }
    }
    return index;
}

TYPECONST Type* TypeUtil::getRecursiveElementType(TYPECONST Type *type) {
    TYPECONST PointerType* pointerType = dyn_cast<PointerType>(type);
    if(!pointerType) {
        return type;
    }
    return getRecursiveElementType(pointerType->getElementType());
}

TYPECONST Type* TypeUtil::getArrayFreePointerType(TYPECONST Type *type) {
    if(type->isPointerTy() || type->isArrayTy()) {
        TYPECONST Type* elementType = getArrayFreePointerType(type->getContainedType(0));
        type = PointerType::get(elementType, 0);
    }
    return type;
}

bool TypeUtil::hasInnerPointers(TYPECONST Type *type) {
    if(TypeUtil::isOpaqueTy(type) || type->isFunctionTy()) {
        return false;
    }
    if(type->isPointerTy()) {
        return true;
    }

    unsigned numContainedTypes = type->getNumContainedTypes();
    if(numContainedTypes == 0) {
        return false;
    }
    else if(type->isArrayTy() || type->isVectorTy()) {
        return hasInnerPointers(type->getContainedType(0));
    }
    else {
        assert(type->isStructTy());
        for(unsigned i=0;i<numContainedTypes;i++) {
            if(hasInnerPointers(type->getContainedType(i))) {
                return true;
            }
        }
   }

   return false;
}

bool TypeUtil::isArrayAsStructTy(TYPECONST Type *type) {
   if(!type->isStructTy()) {
       return false;
   }
   if (type->getNumContainedTypes() == 1) {
       return true;
   }
   /*
    * This check is no longer used, because it may wrongly fail in the case of
    * an array of instances of a structure that contains a union that is
    * initialized in different ways for array element 0 and 1.  What we really
    * need is a check to see whether the elements are *compatible*, but we do
    * not appear to have the means to do that here.  We warn if the check fails
    * so as to make sure that its removal is not the source of new problems.
    */
   if (type->getContainedType(0) != type->getContainedType(1)) {
       TypeUtilLog("strict isArrayAsStructTy test failed");
       //return false;
   }
   return true;
}

unsigned TypeUtil::getHash(TYPECONST Type* type) {
    return (unsigned) PassUtil::getTypeHash(type);
}

const std::string TypeUtil::getDescription(TYPECONST Type* type,
    size_t max_chars /*= 0*/, size_t max_levels /*= 0*/) {
    std::string string;
    if(!PRINT_SKIP_UNIONS && !PRINT_SKIP_STRUCTS && PRINT_USE_BUILTIN_PRINTING) {
    string = PassUtil::getTypeDescription(type);
    }
    else {
        raw_string_ostream ostream(string);
        printTypeString(ostream, type, max_chars, max_levels);
        ostream.flush();
    }
    if(VERBOSE_LEVEL > 0) {
        string = getFormattedDescription(string);
    }
    return string;
}

const std::string TypeUtil::getDescription(const EDIType* aEDIType) {
    std::string string;
    string = aEDIType->getDescription(PRINT_SKIP_UNIONS, PRINT_SKIP_STRUCTS, PRINT_MULTI_NAMES);
    if(VERBOSE_LEVEL > 0) {
        string = getFormattedDescription(string);
    }
    if(VERBOSE_LEVEL > 1) {
        raw_string_ostream ostream(string);
        ostream << "\n\t";
        aEDIType->getDIType()->print(ostream);
        ostream.flush();
    }
    return string;
}

const std::string TypeUtil::getDescription(TYPECONST Type* type, const EDIType* aEDIType,
    size_t max_chars /*= 0*/, size_t max_levels /*= 0*/) {
    std::string string;
    string = "[\ntype = \n";
    string.append(TypeUtil::getDescription(type, max_chars, max_levels));
    string.append("\nEDIType =\n");
    string.append(TypeUtil::getDescription(aEDIType));
    string.append("\n]");
    return string;
}

const std::string TypeUtil::getFormattedDescription(std::string &description) {
    std::string string;
    raw_string_ostream ostream(string);
    printFormattedTypeString(ostream, description, 0, description.size());
    ostream.flush();
    return string;
}

void TypeUtil::printFormattedTypeString(raw_ostream &OS, std::string &typeStr, int start, int length) {
    static int indent = 0;
    for(int k=0;k<indent;k++) OS << " ";
    for(int i=start;i<start+length;i++) {
        OS << typeStr[i];
        if(typeStr[i] == '{') {
            int newLength = 0;
            int structsFound = 0;
            int j;
            for(j=i+2;j<start+length;j++) {
                switch(typeStr[j]) {
                    case '{':
                        structsFound++;
                    break;
                    case '}':
                        if(structsFound == 0) {
                            newLength = j-i-3;
                        }
                        else {
                            structsFound--;
                        }
                    break;
                }
                if(newLength != 0) {
                    break;
                }
            }
            assert(newLength > 0);
            OS << "\n";
            indent += 2;
            printFormattedTypeString(OS, typeStr, i+2, newLength);
            indent -= 2;
            OS << "\n";
            for(int k=0;k<indent;k++) OS << " ";
            i = j;
            OS << typeStr[i];
        }
    }
}

void TypeUtil::printTypeString(raw_ostream &OS, TYPECONST Type* type,
    size_t max_chars /*= 0*/, size_t max_levels /*= 0*/) {
    static std::vector<TYPECONST Type*> nestedTypes;
    static unsigned level = 0;
    static unsigned counter;

    if (level == 0) {
        counter = 0;
    }
    else if(max_chars && counter >= max_chars) {
        OS << "%%";
        return;
    }
    else if(max_levels && level >= max_levels) {
        OS << "%%";
        return;
    }

    if(TypeUtil::isOpaqueTy(type)) {
        OS << "opaque";
        counter += 6;
        return;
    }

    unsigned numContainedTypes = type->getNumContainedTypes();
    if(numContainedTypes == 0) {
        assert(!type->isStructTy());
        type->print(OS);
        counter += 2;
        return;
    }

    if(type->isPointerTy() && type->getContainedType(0)->isStructTy()) {
        bool isNestedType = false;
        unsigned j;
        for(j=0;j<nestedTypes.size();j++) {
            if(nestedTypes[j] == type) {
                isNestedType = true;
                break;
            }
        }
        if(isNestedType) {
            OS << "\\" << nestedTypes.size() - j;
            counter += 2;
            return;
        }
    }

    nestedTypes.push_back(type);
    if(type->isPointerTy()) {
        TYPECONST Type* subType = type->getContainedType(0);
        level++;
        printTypeString(OS, subType);
        level--;
        OS << "*";
        counter++;
    }
    else if(type->isArrayTy() || type->isVectorTy()) {
        TYPECONST Type* subType = type->getContainedType(0);
        unsigned numElements = type->isArrayTy() ? ((TYPECONST ArrayType*) type)->getNumElements() : ((TYPECONST VectorType*) type)->getNumElements();
        char startSep = type->isArrayTy() ? '[' : '<';
        char endSep = type->isArrayTy() ? ']' : '>';
        OS << startSep;
        if(numElements) {
            OS << numElements << " x ";
        }
        level++;
        printTypeString(OS, subType);
        level--;
        OS << endSep;
        counter += 4;
    }
    else if(type->isStructTy()) {
        if(PRINT_SKIP_STRUCTS || PRINT_SKIP_UNIONS) {
            OS << "$STRUCT/UNION";
            counter += 13;
            nestedTypes.pop_back();
            return;
        }
        unsigned numContainedTypes = type->getNumContainedTypes();
        OS << "{ ";
        OS << "$STRUCT/UNION ";
        for(unsigned i=0;i<numContainedTypes;i++) {
            if(i > 0) {
                OS << ", ";
            }
            TYPECONST Type* subType = type->getContainedType(i);
            level++;
            printTypeString(OS, subType);
            level--;
        }
        OS << " }";
        counter += 18 + 2*numContainedTypes;
   }
   else if(type->isFunctionTy()) {
       unsigned numContainedTypes = type->getNumContainedTypes();
       assert(numContainedTypes > 0);
       TYPECONST Type* subType = type->getContainedType(0);
       level++;
       printTypeString(OS, subType);
       level--;
       numContainedTypes--;
       OS << " (";
       for(unsigned i=0;i<numContainedTypes;i++) {
           if(i > 0) {
               OS << ", ";
           }
           subType = type->getContainedType(i+1);
           level++;
           printTypeString(OS, subType);
           level--;
       }
       OS << ")";
       counter += 3 + 2*numContainedTypes;
   }
   else {
       OS << "???";
       counter +=3;
   }
   nestedTypes.pop_back();
}

unsigned TypeUtil::typeToBits(TYPECONST Type *type) {
    if (type->isIntegerTy()) {
        return ((IntegerType*)type)->getBitWidth();
    }
    else if (type->isArrayTy() && type->getContainedType(0)->isIntegerTy()) {
        TYPECONST Type *containedType = type->getContainedType(0);
        return ((IntegerType*)containedType)->getBitWidth() * ((ArrayType*)containedType)->getNumElements();
    }
    return 0;
}

int TypeUtil::VERBOSE_LEVEL = 1;
int TypeUtil::PRINT_SKIP_UNIONS = 0;
int TypeUtil::PRINT_SKIP_STRUCTS = 0;
int TypeUtil::PRINT_USE_BUILTIN_PRINTING = 0;
int TypeUtil::PRINT_MULTI_NAMES = 0;

}
