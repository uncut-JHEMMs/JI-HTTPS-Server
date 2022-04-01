#include "faker.hpp"

#include <iostream>
#include <thread>

#include <Python.h>

namespace faker
{
    Faker Faker::Instance = Faker();

    Faker::Faker()
    {
        Py_Initialize();
        auto* fakerModule = PyImport_ImportModule("faker"); // import faker
        if (!fakerModule)
        {
            PyErr_Print();
            exit(1);
        }

        auto* dict = PyModule_GetDict(fakerModule);
        if (!dict)
        {
            PyErr_Print();
            exit(1);
        }
        Py_DECREF(fakerModule);

        auto* module_class = PyDict_GetItemString(dict, "Faker"); // faker.Faker
        if (!module_class)
        {
            PyErr_Print();
            exit(1);
        }
        Py_DECREF(dict);

        if (PyCallable_Check(module_class))
        {
            this->module = PyObject_CallObject(module_class, nullptr);
            Py_DECREF(module_class);
        }
        else
        {
            std::cerr << "Cannot instantiate the Faker class\n";
            Py_DECREF(module_class);
            exit(1);
        }

        this->ts = PyEval_SaveThread();
    }

    Faker::~Faker() noexcept
    {
        PyEval_RestoreThread(this->ts);
        Py_Finalize();
    }

    std::string Faker::CallMethod(const std::string_view& view)
    {
        auto state = PyGILState_Ensure();
        auto* value = PyObject_CallMethod(Instance.module, view.data(), nullptr); // Faker().{view}()
        if (!value)
        {
            PyErr_Print();
            return std::string{};
        }

        const char* str = PyUnicode_AsUTF8(value);
        Py_DECREF(value);
        PyGILState_Release(state);
        return std::string{str};
    }
}
