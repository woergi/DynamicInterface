#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import sys


fileStartTag = r'''#ifndef INTERPRETER_AUTOCODE_H
#define INTERPRETER_AUTOCODE_H

'''
fileNamespaceStartTag = r'''namespace Interpreter
{
namespace AutoCode
{
'''
fileNamespaceEndTag = r'''}
}
'''
fileEndTag = r'''#endif // INTERPRETER_AUTOCODE_H
'''

class Wrapper:
    def __init__(self, c_struct_name, py_struct_name):
        self.member = []
        self.c_struct_name = c_struct_name
        self.py_struct_name = py_struct_name
        self.holds_str_mem = False

    def addVarMapping(self, c_mem_name, py_mem_name, strLen):
        py_mem_name = self.c_struct_name + "_" + py_mem_name
        if strLen > 0:
            self.member.append((c_mem_name, py_mem_name, strLen))
            self.holds_str_mem = True
        else:
            self.member.append((c_mem_name, py_mem_name))

    def getNameTableElements(self):
        result = '  std::array<std::string, 3>{{"%s", "", "%s"}},\n' % (self.c_struct_name, self.py_struct_name)
        for m in self.member:
            result += '  std::array<std::string, 3>{{"", "%s", "%s"}},\n' % (m[0], m[1])
        return result

    def getMapperCode(self):
        result =  'template<>\n'
        result += 'struct Mapper<%s> {\n' % (self.c_struct_name)
        result += '  %s& m_obj;\n' % (self.c_struct_name)
        result += '  Mapper(%s& obj) : m_obj(obj) { }\n' % (self.c_struct_name)
        result += '  inline MappingType toPython() {\n'
        result += '    return {\n'
        for m in self.member:
            result += '            {"%s", toString(m_obj.%s) },\n' % (m[1], m[0])
        result += '           };\n  }\n\n'

        result += '  inline void toC(MappingType const & fromPy) {\n'
        if self.holds_str_mem:
            result += '    std::string temp;\n'
        for m in self.member:
            if len(m) == 2:
                result += '    std::stringstream(fromPy.at("%s")) >> m_obj.%s;\n' % (m[1], m[0])
            else:
                result += '    std::stringstream(fromPy.at("%s")) >> temp;\n' % (m[1])
                result += '    if (temp.length() >= %s)\n' % m[2]
                result += '      throw parsing_error("PY_Value \'%s\' for C_Value \'%s\' has invalid length");\n' % (m[1], m[0])
                result += '    strcpy(m_obj.%s, temp.c_str());' % (m[0])

        result += '  }\n};\n\n'
        return result


if __name__ == "__main__":
    AUTO_CODE_FILE = "interpreter_autocode.h"
    ANNOTATION_TOKEN = "//@"
    ANNOTATION_VAR = "DYNAMIC_NAME"
    ANNOTATION_LEN = "DYNAMIC_LEN"

    structs = []
    affectedFiles = []

    print("Executing pre-build step...")
    with open(AUTO_CODE_FILE, 'w') as auto_code_file:
        for dir in sys.argv[1:]:
            print ("Analyzing dir: %s" % (dir))
            for file in os.listdir(dir):
                if not os.path.isfile(file):
                    continue
                if not (file.endswith(".cpp") or file.endswith(".h")) or file == AUTO_CODE_FILE:
                    continue

                currentStructLen = len(structs)
                print ("Analyzing file: %s" % (file))
                f = open(file, 'r')
                for line in f:
                    annotation = line.find(ANNOTATION_TOKEN)
                    struct_start = line.find("struct")
                    if annotation < 0:
                        continue
                    if struct_start >= 0:
                        currentCStructName = line[struct_start+6:annotation].strip()
                        currentPyStructName = line[line.find(ANNOTATION_VAR)+len(ANNOTATION_VAR)+1:]
                        currentPyStructName = currentPyStructName[:currentPyStructName.find(')')]
                        structs.append(Wrapper(currentCStructName, currentPyStructName))
                    else:
                        varCName = line[:line.find(';')].strip()
                        varCName = varCName[varCName.find(' '):].strip()

                        varPyName = line[line.find(ANNOTATION_VAR)+len(ANNOTATION_VAR)+1:]
                        varPyName = varPyName[:varPyName.find(')')]

                        strLen = -1
                        if line.find(ANNOTATION_LEN) > 0:
                            strLen = line[line.find(ANNOTATION_LEN)+len(ANNOTATION_LEN)+1:]
                            strLen = strLen[:strLen.find(')')]
                            varCName = varCName[:varCName.find('[')].strip()

                        structs[-1].addVarMapping(varCName, varPyName, strLen)

                if currentStructLen != len(structs):
                    affectedFiles.append(file)

        for s in structs:
            print("Struct C({0}), PY({1})".format(s.c_struct_name, s.py_struct_name))
            for m in s.member:
                print ("\tMember C({0}), PY({1})".format(m[0], m[1]))

        numElements = len(structs)
        for s in structs:
            numElements += len(s.member)
        auto_code_file.write(fileStartTag)
        for file in affectedFiles:
            auto_code_file.write('#include "{0}"\n'.format(file))
        auto_code_file.write(fileNamespaceStartTag)
        if structs:
            mappingNameTable = 'std::array<std::array<std::string, 3>, %s > const DYN_STRUCTS = {\n' % (numElements)
            for s in structs:
                mappingNameTable += s.getNameTableElements()
                auto_code_file.write(s.getMapperCode())
            mappingNameTable += "};\n\n"
        auto_code_file.write(mappingNameTable)
        auto_code_file.write(fileNamespaceEndTag)
        auto_code_file.write(fileEndTag)
