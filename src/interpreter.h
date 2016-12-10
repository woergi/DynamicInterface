#pragma once

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include<unordered_map>
#include<vector>
#include<string>
#include<memory>
#include<sstream>

#include <boost/python.hpp>

namespace Interpreter
{
using MappingType = std::unordered_map<std::string, std::string>;
using BoostPyObj = boost::python::object;

static constexpr const char* SRC_LINE_VARNAME = "SRC";
static constexpr const char* DST_LINE_VARNAME = "DST";

using ScriptSelectorType = std::string;
struct ScriptSelector               //@DYNAMIC_NAME(ScriptSelector)
{
  ScriptSelectorType scriptName;    //@DYNAMIC_NAME(Name)
};

/*! \brief Parsing exception
 * 
 * This exception can be thrown by the interpreter during pre-, 
 * post-execution and execution itself.
 */
class parsing_error : public std::exception
{
private:
    std::string m_what;
public:
    parsing_error(std::string const & w) : m_what(w) { }
    virtual ~parsing_error() noexcept { }
    virtual const char* what() const noexcept override { return m_what.c_str(); }
};

template<typename T> inline std::string toString(T const & v)
{
    std::stringstream ss;
    ss << v;
    return ss.str();
}
template<> inline std::string toString(char const * const & v)
{
    std::stringstream ss;
    ss << "'" << v << "'";
    return ss.str();
}
template<> inline std::string toString(std::string const & v) { return toString(v.c_str()); }

namespace AutoCode
{
/*! \brief Base class for the auto generated mapping code 
 *         between the Python and the C world.
 * 
 * To build the auto code a Python script is shipped with the code
 * which generates a file named \file interpreter_autocode.h
*/
template<typename T> struct Mapper { };
}

/*! \brief Python based interpreter
 * 
 * This class can be used to run Python scripts within the context
 * of the C program
 */
class PyInterpreter final
{
public:
  using ScriptType = std::string;
  using ScriptMappingType = std::unordered_map<ScriptSelectorType, ScriptType>;

private:
    struct Base_ObjWrapper
    {
        virtual void preExec(BoostPyObj& main) = 0;
        virtual void postExec(BoostPyObj& main) = 0;
        virtual ~Base_ObjWrapper() {}
    };

    template<typename T>
    class ObjWrapper : public Base_ObjWrapper
    {
    private:
        AutoCode::Mapper<T> m_mapper;

    public:
        ObjWrapper(T& obj) : m_mapper(obj) { }

        virtual void preExec(BoostPyObj& main) override {
            auto map = m_mapper.toPython();
            for (const auto ele : map)
                main.attr(ele.first.c_str()) = ele.second;
        }

        virtual void postExec(BoostPyObj& main) override {
            auto main_namespace = main.attr("__dict__");
            auto map = m_mapper.toPython();
            for (auto& ele : map) {
                boost::python::exec((ele.first + "=str(" + ele.first + ")").c_str(), main_namespace);
                ele.second = boost::python::extract<std::string>(main.attr(ele.first.c_str()));
            }
            m_mapper.toC(map);
        }
    };

    BoostPyObj m_main;            //! Python global environment
    BoostPyObj m_main_namespace;  //! Python __dict__ object

    std::vector<std::shared_ptr<Base_ObjWrapper>> m_obj;  //! Objects which can be used in the Python world
    ScriptMappingType m_script;                           //! Scripts to apply
    ScriptSelectorType m_scriptSelectorScript;            //! Selects specific execution script
    
    void setObj() {}

  public:
    PyInterpreter();

    template<typename T, typename... Ts>
    void setObj(T& obj, Ts&... args) {
        m_obj.push_back(std::move(std::shared_ptr<Base_ObjWrapper>(new ObjWrapper<T>(obj))));
        setObj(args...);
    }
    
    void setScript(ScriptSelectorType const & name, ScriptType const & script) { m_script[name] = script; }
    void setScriptSelector(ScriptSelectorType const & script) { m_scriptSelectorScript = script; }
    void execute();
    
    template<typename T>
    void addVarToScriptNs(std::string const & name, T value) {
      m_main.attr(name.c_str()) = value;
    }
    
    void reset() { m_obj.clear(); }

    PyInterpreter(PyInterpreter const &) = delete;
    PyInterpreter(PyInterpreter const &&) = delete;
    PyInterpreter operator= (PyInterpreter const &) = delete;
    PyInterpreter operator= (PyInterpreter const &&) = delete;
};
}

#include "interpreter_autocode.h"

#endif 
