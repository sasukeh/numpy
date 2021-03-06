#define NPY_NO_DEPRECATED_API NPY_API_VERSION
#define NO_IMPORT_ARRAY

#include "npy_pycompat.h"
#include "numpy/ufuncobject.h"
#include "npy_import.h"

#include "ufunc_override.h"
#include "override.h"

/*
 * The following functions normalize ufunc arguments. The work done is similar
 * to what is done inside ufunc_object by get_ufunc_arguments for __call__ and
 * generalized ufuncs, and by PyUFunc_GenericReduction for the other methods.
 * It would be good to unify (see gh-8892).
 */

/*
 * ufunc() and ufunc.outer() accept 'sig' or 'signature';
 * normalize to 'signature'
 */
static int
normalize_signature_keyword(PyObject *normal_kwds)
{
    PyObject* obj = PyDict_GetItemString(normal_kwds, "sig");
    if (obj != NULL) {
        if (PyDict_GetItemString(normal_kwds, "signature")) {
            PyErr_SetString(PyExc_TypeError,
                            "cannot specify both 'sig' and 'signature'");
            return -1;
        }
        Py_INCREF(obj);
        PyDict_SetItemString(normal_kwds, "signature", obj);
        PyDict_DelItemString(normal_kwds, "sig");
    }
    return 0;
}

static int
normalize___call___args(PyUFuncObject *ufunc, PyObject *args,
                        PyObject **normal_args, PyObject **normal_kwds)
{
    /*
     * ufunc.__call__(*args, **kwds)
     */
    int i;
    int not_all_none;
    int nin = ufunc->nin;
    int nout = ufunc->nout;
    int nargs = PyTuple_GET_SIZE(args);
    PyObject *obj;

    if (nargs < nin) {
        PyErr_Format(PyExc_TypeError,
                     "ufunc() missing %d of %d required positional argument(s)",
                     nin - nargs, nin);
        return -1;
    }
    if (nargs > nin+nout) {
        PyErr_Format(PyExc_TypeError,
                     "ufunc() takes from %d to %d arguments but %d were given",
                     nin, nin+nout, nargs);
        return -1;
    }

    *normal_args = PyTuple_GetSlice(args, 0, nin);
    if (*normal_args == NULL) {
        return -1;
    }

    /* If we have more args than nin, they must be the output variables.*/
    if (nargs > nin) {
        if(PyDict_GetItemString(*normal_kwds, "out")) {
            PyErr_Format(PyExc_TypeError,
                         "argument given by name ('out') and position (%d)",
                         nin);
            return -1;
        }
        for (i = nin; i < nargs; i++) {
            not_all_none = (PyTuple_GET_ITEM(args, i) != Py_None);
            if (not_all_none) {
                break;
            }
        }
        if (not_all_none) {
            if (nargs - nin == nout) {
                obj = PyTuple_GetSlice(args, nin, nargs);
            }
            else {
                PyObject *item;

                obj = PyTuple_New(nout);
                if (obj == NULL) {
                    return -1;
                }
                for (i = 0; i < nout; i++) {
                    if (i + nin < nargs) {
                        item = PyTuple_GET_ITEM(args, nin+i);
                    }
                    else {
                        item = Py_None;
                    }
                    Py_INCREF(item);
                    PyTuple_SET_ITEM(obj, i, item);
                }
            }
            PyDict_SetItemString(*normal_kwds, "out", obj);
            Py_DECREF(obj);
        }
    }
    /* finally, ufuncs accept 'sig' or 'signature' normalize to 'signature' */
    return normalize_signature_keyword(*normal_kwds);
}

static int
normalize_reduce_args(PyUFuncObject *ufunc, PyObject *args,
                      PyObject **normal_args, PyObject **normal_kwds)
{
    /*
     * ufunc.reduce(a[, axis, dtype, out, keepdims])
     */
    int nargs = PyTuple_GET_SIZE(args);
    int i;
    PyObject *obj;
    static char *kwlist[] = {"array", "axis", "dtype", "out", "keepdims"};

    if (nargs < 1 || nargs > 5) {
        PyErr_Format(PyExc_TypeError,
                     "ufunc.reduce() takes from 1 to 5 positional "
                     "arguments but %d were given", nargs);
        return -1;
    }
    *normal_args = PyTuple_GetSlice(args, 0, 1);
    if (*normal_args == NULL) {
        return -1;
    }

    for (i = 1; i < nargs; i++) {
        if (PyDict_GetItemString(*normal_kwds, kwlist[i])) {
            PyErr_Format(PyExc_TypeError,
                         "argument given by name ('%s') and position (%d)",
                         kwlist[i], i);
            return -1;
        }
        obj = PyTuple_GET_ITEM(args, i);
        if (obj != Py_None) {
            if (i == 3) {
                obj = PyTuple_GetSlice(args, 3, 4);
            }
            PyDict_SetItemString(*normal_kwds, kwlist[i], obj);
            if (i == 3) {
                Py_DECREF(obj);
            }
        }
    }
    return 0;
}

static int
normalize_accumulate_args(PyUFuncObject *ufunc, PyObject *args,
                          PyObject **normal_args, PyObject **normal_kwds)
{
    /*
     * ufunc.accumulate(a[, axis, dtype, out])
     */
    int nargs = PyTuple_GET_SIZE(args);
    int i;
    PyObject *obj;
    static char *kwlist[] = {"array", "axis", "dtype", "out", "keepdims"};

    if (nargs < 1 || nargs > 4) {
        PyErr_Format(PyExc_TypeError,
                     "ufunc.accumulate() takes from 1 to 4 positional "
                     "arguments but %d were given", nargs);
        return -1;
    }
    *normal_args = PyTuple_GetSlice(args, 0, 1);
    if (*normal_args == NULL) {
        return -1;
    }

    for (i = 1; i < nargs; i++) {
        if (PyDict_GetItemString(*normal_kwds, kwlist[i])) {
            PyErr_Format(PyExc_TypeError,
                         "argument given by name ('%s') and position (%d)",
                         kwlist[i], i);
            return -1;
        }
        obj = PyTuple_GET_ITEM(args, i);
        if (obj != Py_None) {
            if (i == 3) {
                obj = PyTuple_GetSlice(args, 3, 4);
            }
            PyDict_SetItemString(*normal_kwds, kwlist[i], obj);
            if (i == 3) {
                Py_DECREF(obj);
            }
        }
    }
    return 0;
}

static int
normalize_reduceat_args(PyUFuncObject *ufunc, PyObject *args,
                    PyObject **normal_args, PyObject **normal_kwds)
{
    /*
     * ufunc.reduceat(a, indicies[, axis, dtype, out])
     * the number of arguments has been checked in PyUFunc_GenericReduction.
     */
    int i;
    int nargs = PyTuple_GET_SIZE(args);
    PyObject *obj;
    static char *kwlist[] = {"array", "indices", "axis", "dtype", "out"};

    if (nargs < 2 || nargs > 5) {
        PyErr_Format(PyExc_TypeError,
                     "ufunc.reduceat() takes from 2 to 4 positional "
                     "arguments but %d were given", nargs);
        return -1;
    }
    /* a and indicies */
    *normal_args = PyTuple_GetSlice(args, 0, 2);
    if (*normal_args == NULL) {
        return -1;
    }

    for (i = 2; i < nargs; i++) {
        if (PyDict_GetItemString(*normal_kwds, kwlist[i])) {
            PyErr_Format(PyExc_TypeError,
                         "argument given by name ('%s') and position (%d)",
                         kwlist[i], i);
            return -1;
        }
        obj = PyTuple_GET_ITEM(args, i);
        if (obj != Py_None) {
            if (i == 4) {
                obj = PyTuple_GetSlice(args, 4, 5);
            }
            PyDict_SetItemString(*normal_kwds, kwlist[i], obj);
            if (i == 4) {
                Py_DECREF(obj);
            }
        }
    }
    return 0;
}

static int
normalize_outer_args(PyUFuncObject *ufunc, PyObject *args,
                     PyObject **normal_args, PyObject **normal_kwds)
{
    /*
     * ufunc.outer(*args, **kwds)
     * all positional arguments should be inputs.
     * for the keywords, we only need to check 'sig' vs 'signature'.
     */
    int nin = ufunc->nin;
    int nargs = PyTuple_GET_SIZE(args);

    if (nargs < nin) {
        PyErr_Format(PyExc_TypeError,
                     "ufunc.outer() missing %d of %d required positional "
                     "argument(s)", nin - nargs, nin);
        return -1;
    }
    if (nargs > nin) {
        PyErr_Format(PyExc_TypeError,
                     "ufunc.outer() takes %d arguments but %d were given",
                     nin, nargs);
        return -1;
    }

    *normal_args = PyTuple_GetSlice(args, 0, nin);
    if (*normal_args == NULL) {
        return -1;
    }

    /* ufuncs accept 'sig' or 'signature' normalize to 'signature' */
    return normalize_signature_keyword(*normal_kwds);
}

static int
normalize_at_args(PyUFuncObject *ufunc, PyObject *args,
                  PyObject **normal_args, PyObject **normal_kwds)
{
    /* ufunc.at(a, indices[, b]) */
    int nargs = PyTuple_GET_SIZE(args);

    if (nargs < 2 || nargs > 3) {
        PyErr_Format(PyExc_TypeError,
                     "ufunc.at() takes from 2 to 3 positional "
                     "arguments but %d were given", nargs);
        return -1;
    }
    *normal_args = PyTuple_GetSlice(args, 0, nargs);
    return (*normal_args == NULL);
}

/*
 * Check a set of args for the `__array_ufunc__` method.  If more than one of
 * the input arguments implements `__array_ufunc__`, they are tried in the
 * order: subclasses before superclasses, otherwise left to right. The first
 * (non-None) routine returning something other than `NotImplemented`
 * determines the result. If all of the `__array_ufunc__` operations return
 * `NotImplemented` (or are None), a `TypeError` is raised.
 *
 * Returns 0 on success and 1 on exception. On success, *result contains the
 * result of the operation, if any. If *result is NULL, there is no override.
 */
NPY_NO_EXPORT int
PyUFunc_CheckOverride(PyUFuncObject *ufunc, char *method,
                      PyObject *args, PyObject *kwds,
                      PyObject **result)
{
    int i;
    int j;
    int status;

    int noa;
    PyObject *with_override[NPY_MAXARGS];

    PyObject *obj;
    PyObject *other_obj;
    PyObject *out;

    PyObject *method_name = NULL;
    PyObject *normal_args = NULL; /* normal_* holds normalized arguments. */
    PyObject *normal_kwds = NULL;

    PyObject *override_args = NULL;
    Py_ssize_t len;

    /*
     * Check inputs for overrides
     */
    noa = PyUFunc_WithOverride(args, kwds, with_override);
    /* No overrides, bail out.*/
    if (noa == 0) {
        *result = NULL;
        return 0;
    }

    /*
     * Normalize ufunc arguments.
     */

    /* Build new kwds */
    if (kwds && PyDict_CheckExact(kwds)) {

        /* ensure out is always a tuple */
        normal_kwds = PyDict_Copy(kwds);
        out = PyDict_GetItemString(normal_kwds, "out");
        if (out != NULL) {
            int nout = ufunc->nout;

            if (PyTuple_Check(out)) {
                int all_none = 1;

                if (PyTuple_GET_SIZE(out) != nout) {
                    PyErr_Format(PyExc_TypeError,
                                 "The 'out' tuple must have exactly "
                                 "%d entries: one per ufunc output", nout);
                    goto fail;
                }
                for (i = 0; i < PyTuple_GET_SIZE(out); i++) {
                    all_none = (PyTuple_GET_ITEM(out, i) == Py_None);
                    if (!all_none) {
                        break;
                    }
                }
                if (all_none) {
                    PyDict_DelItemString(normal_kwds, "out");
                }
            }
            else {
                /* not a tuple */
                if (nout > 1 && DEPRECATE("passing a single argument to the "
                                          "'out' keyword argument of a "
                                          "ufunc with\n"
                                          "more than one output will "
                                          "result in an error in the "
                                          "future") < 0) {
                    /*
                     * If the deprecation is removed, also remove the loop
                     * below setting tuple items to None (but keep this future
                     * error message.)
                     */
                    PyErr_SetString(PyExc_TypeError,
                                    "'out' must be a tuple of arguments");
                    goto fail;
                }
                if (out != Py_None) {
                    /* not already a tuple and not None */
                    PyObject *out_tuple = PyTuple_New(nout);

                    if (out_tuple == NULL) {
                        goto fail;
                    }
                    for (i = 1; i < nout; i++) {
                        Py_INCREF(Py_None);
                        PyTuple_SET_ITEM(out_tuple, i, Py_None);
                    }
                    /* out was borrowed ref; make it permanent */
                    Py_INCREF(out);
                    /* steals reference */
                    PyTuple_SET_ITEM(out_tuple, 0, out);
                    PyDict_SetItemString(normal_kwds, "out", out_tuple);
                    Py_DECREF(out_tuple);
                }
                else {
                    /* out=None; remove it */
                    PyDict_DelItemString(normal_kwds, "out");
                }
            }
        }
    }
    else {
        normal_kwds = PyDict_New();
    }
    if (normal_kwds == NULL) {
        goto fail;
    }

    /* decide what to do based on the method. */

    /* ufunc.__call__ */
    if (strcmp(method, "__call__") == 0) {
        status = normalize___call___args(ufunc, args, &normal_args,
                                         &normal_kwds);
    }
    /* ufunc.reduce */
    else if (strcmp(method, "reduce") == 0) {
        status = normalize_reduce_args(ufunc, args, &normal_args,
                                       &normal_kwds);
    }
    /* ufunc.accumulate */
    else if (strcmp(method, "accumulate") == 0) {
        status = normalize_accumulate_args(ufunc, args, &normal_args,
                                           &normal_kwds);
    }
    /* ufunc.reduceat */
    else if (strcmp(method, "reduceat") == 0) {
        status = normalize_reduceat_args(ufunc, args, &normal_args,
                                         &normal_kwds);
    }
    /* ufunc.outer */
    else if (strcmp(method, "outer") == 0) {
        status = normalize_outer_args(ufunc, args, &normal_args, &normal_kwds);
    }
    /* ufunc.at */
    else if (strcmp(method, "at") == 0) {
        status = normalize_at_args(ufunc, args, &normal_args, &normal_kwds);
    }
    /* unknown method */
    else {
        PyErr_Format(PyExc_TypeError,
                     "Internal Numpy error: unknown ufunc method '%s' in call "
                     "to PyUFunc_CheckOverride", method);
        status = -1;
    }
    if (status != 0) {
        Py_XDECREF(normal_args);
        goto fail;
    }

    len = PyTuple_GET_SIZE(normal_args);
    override_args = PyTuple_New(len + 2);
    if (override_args == NULL) {
        goto fail;
    }

    Py_INCREF(ufunc);
    /* PyTuple_SET_ITEM steals reference */
    PyTuple_SET_ITEM(override_args, 0, (PyObject *)ufunc);
    method_name = PyUString_FromString(method);
    if (method_name == NULL) {
        goto fail;
    }
    Py_INCREF(method_name);
    PyTuple_SET_ITEM(override_args, 1, method_name);
    for (i = 0; i < len; i++) {
        PyObject *item = PyTuple_GET_ITEM(normal_args, i);

        Py_INCREF(item);
        PyTuple_SET_ITEM(override_args, i + 2, item);
    }
    Py_DECREF(normal_args);

    /* Call __array_ufunc__ functions in correct order */
    while (1) {
        PyObject *array_ufunc;
        PyObject *override_obj;

        override_obj = NULL;
        *result = NULL;

        /* Choose an overriding argument */
        for (i = 0; i < noa; i++) {
            obj = with_override[i];
            if (obj == NULL) {
                continue;
            }

            /* Get the first instance of an overriding arg.*/
            override_obj = obj;

            /* Check for sub-types to the right of obj. */
            for (j = i + 1; j < noa; j++) {
                other_obj = with_override[j];
                if (other_obj != NULL &&
                    PyObject_Type(other_obj) != PyObject_Type(obj) &&
                    PyObject_IsInstance(other_obj,
                                        PyObject_Type(override_obj))) {
                    override_obj = NULL;
                    break;
                }
            }

            /* override_obj had no subtypes to the right. */
            if (override_obj) {
                /* We won't call this one again */
                with_override[i] = NULL;
                break;
            }
        }

        /* Check if there is a method left to call */
        if (!override_obj) {
            /* No acceptable override found. */
            static PyObject *errmsg_formatter = NULL;
            PyObject *errmsg;

            npy_cache_import("numpy.core._internal",
                             "array_ufunc_errmsg_formatter",
                             &errmsg_formatter);
            if (errmsg_formatter != NULL) {
                errmsg = PyObject_Call(errmsg_formatter, override_args,
                                       normal_kwds);
                if (errmsg != NULL) {
                    PyErr_SetObject(PyExc_TypeError, errmsg);
                    Py_DECREF(errmsg);
                }
            }
            goto fail;
        }

        /* Access the override */
        array_ufunc = PyObject_GetAttrString(override_obj,
                                             "__array_ufunc__");
        if (array_ufunc == NULL) {
            goto fail;
        }

        /* If None, try next one (i.e., as if it returned NotImplemented) */
        if (array_ufunc == Py_None) {
            Py_DECREF(array_ufunc);
            continue;
        }

        *result = PyObject_Call(array_ufunc, override_args, normal_kwds);
        Py_DECREF(array_ufunc);

        if (*result == NULL) {
            /* Exception occurred */
            goto fail;
        }
        else if (*result == Py_NotImplemented) {
            /* Try the next one */
            Py_DECREF(*result);
            continue;
        }
        else {
            /* Good result. */
            break;
        }
    }

    /* Override found, return it. */
    Py_XDECREF(method_name);
    Py_XDECREF(normal_kwds);
    Py_DECREF(override_args);
    return 0;

fail:
    Py_XDECREF(method_name);
    Py_XDECREF(normal_kwds);
    Py_XDECREF(override_args);
    return 1;
}
