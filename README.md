# DynamicInterface
Proof of concept of a Python-based dynamic interface for C++.

The basic idea was to write a library which can dynamically modify arbitrary
public members of a struct / class during runtime.

## Motivation
I wrote this proof of concept just make the idea real. Nevertheless, I wanted
to use my library and wrote a small application around it. I made a book library 
management software (or parts of a possible back-end :D). Okay, I just 
implemented a CLI, and the used SQLite library stores only a subset of 
meta-data from each book. But for testing this library was enough. I used
it to dynamically import books from different sources as well as to export 
the stored data. Both directions not only use different sources (TCP, file), 
they also use different representations / formats. And this is the full strength
of this library. It can convert different formats to one (or more) resulting 
struct.

This conversion between the incoming (or outgoing) raw data and the resulting 
struct is done by a Python script. So, for each possible data representation a
Python mapping script must exist. This can either be stored in the database 
or dynamically retrieved from the host which also sends the raw data to the 
application. The conversion mechanism of this library is line-based. Therefore,
if the raw data consists of more than one line. The application has to care
about the consistent of the final data (it's not that hard).

Because of the fact that selecting the right Python script for the current raw 
data is a complicate task, this mechanism is solved by a dynamic script itself.
To differentiate between the formats the code has a script-selector functionality
where the Python code which should run on the raw data can be selected by another
Python code.

## Build
To build the library you need at least boost and Python libraries and a C++11
compliant compiler. The interpreter itself also builds on the automatic code 
generation. Thus, before building the library the Python script must be run.

```
python DynStructBinderCodeGen.py .
g++-6 -c -I /usr/include/python2.7/ -shared -fPIC -o libinterpreter.so interpreter.cpp
```

## Usage
The library has dependencies to the Boost framework (especially boost-python), 
and to the Python C-binding library.

The usage is really simple. After creating an instance of the interpreter, you've
to set the script-selector. The script selector script has to assign the variable
'Name' in the 'ScriptSelector' struct. The value of the variable is used to 
choose which script is applied to the raw data. If such a script doesn't exist
a parsing_error exception is thrown. Beside setting the script selector script
some additional (at least one) parsing scripts must be set.

When the setup is done the interpreter can be used by passing some structs 
to the interpreter. The interpreter operates on the members of these structs.
The raw data are set by the special method 'addVarToScriptNs' with 
'Interpreter::SRC_LINE_VARNAME' as the 1st parameter. This method can also be
used to set some 'constants' (not really const because they can be modified by
the Python script but for the C-perspective they are const) for the interpreter.

```
...
Interpreter::PyInterpreter m_interpreter;
m_interpreter.setScriptSelector("ScriptSelector_Name='Default'");
m_interpreter.setScript("Default", r->script);
...
Book bookToImport;
m_interpreter.setObj(bookToImport);
m_interpreter.addVarToScriptNs(Interpreter::SRC_LINE_VARNAME, importLine);
m_interpreter.execute();
...
```
### Binding Code Generation
The DynStructBinderCodeGen script is used to generate the bindings between the
Python-, and the C-world. The mechanism is based on annotation but much simpler.
Basically a member and a struct can be annotated by:
```
//@DYNAMIC_NAME
```
The script searches for such flags and extracts the struct's, or member's name.
It's a really simple implementation, therefore it can happen that it can't 
extract the name.

```
using ScriptSelectorType = std::string;
struct ScriptSelector               //@DYNAMIC_NAME(ScriptSelector)
{
  ScriptSelectorType scriptName;    //@DYNAMIC_NAME(Name)
};
```
This annotated code generates bindings so that the ScriptSelector.scriptName 
is linked to a variable named ScriptSelector_Name in the Python world.

Additionally, a special case is the C-String. That damned character arrays
without any length detection which can be used like strings. For such arrays an 
additional annotation must (can) be used (because I was to lazy to extract 
the length out of the code :D).
```
//@DYNAMIC_LEN
```
With this annotation the script adds some length checking code to the conversion 
methods.

The conversion itself is based on streams. So, for custom types streams have to 
be overloaded.

