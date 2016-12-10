#include "interpreter.h"

#include <iostream>

namespace Interpreter
{
[[noreturn]] void handleError()
{
  PyObject* type_ptr = NULL, *value_ptr = NULL, *traceback_ptr = NULL;
  PyErr_Fetch(&type_ptr, &value_ptr, &traceback_ptr);
  std::string ret("Unfetchable Python error");

  if (type_ptr != NULL) {
    boost::python::handle<> h_type(type_ptr);
    boost::python::str type_pstr(h_type);
    boost::python::extract<std::string> e_type_pstr(type_pstr);
    if (e_type_pstr.check())
      ret = e_type_pstr();
    else
      ret = "Unknown exception type";
  }

  if (traceback_ptr != NULL) {
    boost::python::handle<> h_tb(traceback_ptr);
    BoostPyObj tb(boost::python::import("traceback"));
    BoostPyObj fmt_tb(tb.attr("format_tb"));
    BoostPyObj tb_list(fmt_tb(h_tb));
    BoostPyObj tb_str(boost::python::str("\n").join(tb_list));
    boost::python::extract<std::string> returned(tb_str);
    if (returned.check())
      ret += ": " + returned();
    else
      ret += std::string(": Unparseable Python traceback");
  }

  std::cout << "Error in Python: " << ret << std::endl;
  throw parsing_error(ret);
}

PyInterpreter::PyInterpreter()
{
  Py_Initialize();
  m_main = boost::python::import("__main__");
  m_main_namespace = m_main.attr("__dict__");
  m_main_namespace["sys"] = boost::python::import("sys");
}

void PyInterpreter::execute()
{
  try
  {
    ScriptSelector scriptSelObj;
    std::shared_ptr<Base_ObjWrapper> scriptSelObjWrapper(new ObjWrapper<ScriptSelector>(scriptSelObj));
    scriptSelObjWrapper->preExec(m_main);
    boost::python::exec(m_scriptSelectorScript.c_str(), m_main_namespace);
    scriptSelObjWrapper->postExec(m_main);
    
    if (m_script.find(scriptSelObj.scriptName) == m_script.end())
      throw parsing_error("Script selector selected invalid script");

    for (auto ele : m_obj)
      ele->preExec(m_main);
    boost::python::exec(m_script[scriptSelObj.scriptName].c_str(), m_main_namespace);
    for (auto ele : m_obj)
      ele->postExec(m_main);

    m_obj.clear();
  }
  catch (boost::python::error_already_set const&)
  {
    handleError();
  }
}
}
