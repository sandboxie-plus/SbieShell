#include "json.h"
#include <stdlib.h>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <climits>
#include <string.h>
#include <functional>
#include <cctype>
#include <stack>
#include <cerrno>

#include <windows.h>

#ifndef WIN32
#define _tcsicmp strcasecmp
#endif

#ifdef _MSC_VER
#define snprintf _stprintf_s
#endif

using namespace json;

namespace json
{
enum StackDepthType {
    InObject,
    InArray
};
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helper functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static string_t Trim(const string_t &str)
{
    string_t s = str;

    // remove white space in front
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));

    // remove trailing white space
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());

    return s;
}

// Finds the position of the first " character that is NOT preceeded immediately by a \ character.
// In JSON, \" is valid and has a different meaning than the escaped " character.
static size_t GetQuotePos(const string_t &str, size_t start_pos = 0)
{
    bool found_slash = false;
    for (size_t i = start_pos; i < str.length(); i++) {
        TCHAR c = str[i];
        if ((c == TEXT('\\')) && !found_slash) {
            found_slash = true;
            continue;
        } else if ((c == TEXT('\"')) && !found_slash)
            return i;

        found_slash = false;
    }

    return string_t::npos;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Value::Value(const Value &v) : mValueType(v.mValueType)
{
    switch (mValueType) {
    case StringVal      : mStringVal = v.mStringVal; break;
    case IntVal         : mIntVal = v.mIntVal; mFloatVal = (float)v.mIntVal; mDoubleVal = (double)v.mIntVal; break;
    case FloatVal       : mFloatVal = v.mFloatVal; mIntVal = (int)v.mFloatVal; mDoubleVal = (double)v.mDoubleVal; break;
    case DoubleVal      : mDoubleVal = v.mDoubleVal; mIntVal = (int)v.mDoubleVal; mFloatVal = (float)v.mDoubleVal; break;
    case BoolVal        : mBoolVal = v.mBoolVal; break;
    case ObjectVal      : mObjectVal = v.mObjectVal; break;
    case ArrayVal       : mArrayVal = v.mArrayVal; break;
    default             : break;
    }
}

Value &Value::operator =(const Value &v)
{
    if (&v == this)
        return *this;

    mValueType = v.mValueType;

    switch (mValueType) {
    case StringVal      : mStringVal = v.mStringVal; break;
    case IntVal         : mIntVal = v.mIntVal; mFloatVal = (float)v.mIntVal; mDoubleVal = (double)v.mIntVal; break;
    case FloatVal       : mFloatVal = v.mFloatVal; mIntVal = (int)v.mFloatVal; mDoubleVal = (double)v.mDoubleVal; break;
    case DoubleVal      : mDoubleVal = v.mDoubleVal; mIntVal = (int)v.mDoubleVal; mFloatVal = (float)v.mDoubleVal; break;
    case BoolVal        : mBoolVal = v.mBoolVal; break;
    case ObjectVal      : mObjectVal = v.mObjectVal; break;
    case ArrayVal       : mArrayVal = v.mArrayVal; break;
    default             : break;
    }

    return *this;
}

Value &Value::operator [](size_t idx)
{
    if (mValueType != ArrayVal)
        throw std::runtime_error("json mValueType==ArrayVal required");

    return mArrayVal[idx];
}

const Value &Value::operator [](size_t idx) const
{
    if (mValueType != ArrayVal)
        throw std::runtime_error("json mValueType==ArrayVal required");

    return mArrayVal[idx];
}

Value &Value::operator [](const string_t &key)
{
    if (mValueType != ObjectVal)
        throw std::runtime_error("json mValueType==ObjectVal required");

    return mObjectVal[key];
}

Value &Value::operator [](const TCHAR *key)
{
    if (mValueType != ObjectVal)
        throw std::runtime_error("json mValueType==ObjectVal required");

    return mObjectVal[key];
}

const Value &Value::operator [](const TCHAR *key) const
{
    if (mValueType != ObjectVal)
        throw std::runtime_error("json mValueType==ObjectVal required");

    return mObjectVal[key];
}

const Value &Value::operator [](const string_t &key) const
{
    if (mValueType != ObjectVal)
        throw std::runtime_error("json mValueType==ObjectVal required");

    return mObjectVal[key];
}

void Value::Clear()
{
    mValueType = NULLVal;
}

size_t Value::size() const
{
    if ((mValueType != ObjectVal) && (mValueType != ArrayVal))
        return 1;

    return mValueType == ObjectVal ? mObjectVal.size() : mArrayVal.size();
}

bool Value::HasKey(const string_t &key) const
{
    if (mValueType != ObjectVal)
        throw std::runtime_error("json mValueType==ObjectVal required");

    return mObjectVal.HasKey(key);
}

int Value::HasKeys(const std::vector<string_t> &keys) const
{
    if (mValueType != ObjectVal)
        throw std::runtime_error("json mValueType==ObjectVal required");

    return mObjectVal.HasKeys(keys);
}

int Value::HasKeys(const TCHAR **keys, int key_count) const
{
    if (mValueType != ObjectVal)
        throw std::runtime_error("json mValueType==ObjectVal required");

    return mObjectVal.HasKeys(keys, key_count);
}

int Value::ToInt() const
{
    if (!IsNumeric())
        throw std::runtime_error("json mValueType==IsNumeric() required");

    return mIntVal;
}

float Value::ToFloat() const
{
    if (!IsNumeric())
        throw std::runtime_error("json mValueType==IsNumeric() required");

    return mFloatVal;
}

double Value::ToDouble() const
{
    if (!IsNumeric())
        throw std::runtime_error("json mValueType==IsNumeric() required");

    return mDoubleVal;
}

bool Value::ToBool() const
{
    if (mValueType != BoolVal)
        throw std::runtime_error("json mValueType==BoolVal required");

    return mBoolVal;
}

const string_t &Value::ToString() const
{
    if (mValueType != StringVal)
        throw std::runtime_error("json mValueType==StringVal required");

    return mStringVal;
}

Object *Value::RefObject()
{
    if (mValueType != ObjectVal)
        throw std::runtime_error("json mValueType==ObjectVal required");

    return &mObjectVal;
}

Object Value::ToObject() const
{
    if (mValueType != ObjectVal)
        throw std::runtime_error("json mValueType==ObjectVal required");

    return mObjectVal;
}

Array Value::ToArray() const
{
    if (mValueType != ArrayVal)
        throw std::runtime_error("json mValueType==ArrayVal required");

    return mArrayVal;
}

Value::operator int() const
{
    if (!IsNumeric())
        throw std::runtime_error("json mValueType==IsNumeric() required");

    return mIntVal;
}

Value::operator float() const
{
    if (!IsNumeric())
        throw std::runtime_error("json mValueType==IsNumeric() required");

    return mFloatVal;
}

Value::operator double() const
{
    if (!IsNumeric())
        throw std::runtime_error("json mValueType==IsNumeric() required");

    return mDoubleVal;
}

Value::operator bool() const
{
    if (mValueType != BoolVal)
        throw std::runtime_error("json mValueType==BoolVal required");

    return mBoolVal;
}

Value::operator string_t() const
{
    if (mValueType != StringVal)
        throw std::runtime_error("json mValueType==StringVal required");

    return mStringVal;
}

Value::operator Object() const
{
    if (mValueType != ObjectVal)
        throw std::runtime_error("json mValueType==ObjectVal required");

    return mObjectVal;
}

Value::operator Array() const
{
    if (mValueType != ArrayVal)
        throw std::runtime_error("json mValueType==ArrayVal required");

    return mArrayVal;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Array::Array()
{
}

Array::Array(const Array &a) : mValues(a.mValues)
{
}

Array &Array::operator =(const Array &a)
{
    if (&a == this)
        return *this;

    Clear();
    mValues = a.mValues;

    return *this;
}

Value &Array::operator [](size_t i)
{
    return mValues[i];
}

const Value &Array::operator [](size_t i) const
{
    return mValues[i];
}


Array::ValueVector::const_iterator Array::begin() const
{
    return mValues.begin();
}

Array::ValueVector::const_iterator Array::end() const
{
    return mValues.end();
}

Array::ValueVector::iterator Array::begin()
{
    return mValues.begin();
}

Array::ValueVector::iterator Array::end()
{
    return mValues.end();
}

void Array::push_back(const Value &v)
{
    mValues.push_back(v);
}

void Array::insert(size_t index, const Value &v)
{
    mValues.insert(mValues.begin() + index, v);
}

size_t Array::size() const
{
    return mValues.size();
}

void Array::Clear()
{
    mValues.clear();
}

Array::ValueVector::iterator Array::find(const Value &v)
{
    return std::find(mValues.begin(), mValues.end(), v);
}

Array::ValueVector::const_iterator Array::find(const Value &v) const
{
    return std::find(mValues.begin(), mValues.end(), v);
}

bool Array::HasValue(const Value &v) const
{
    return find(v) != end();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Object::Object()
{
}

Object::Object(const Object &obj) : mValues(obj.mValues)
{

}

Object &Object::operator =(const Object &obj)
{
    if (&obj == this)
        return *this;

    Clear();
    mValues = obj.mValues;

    return *this;
}

Value &Object::operator [](const string_t &key)
{
    return mValues[key];
}

const Value &Object::operator [](const string_t &key) const
{
    ValueMap::const_iterator it = mValues.find(key);
    return it->second;
}

Value &Object::operator [](const TCHAR *key)
{
    return mValues[key];
}

const Value &Object::operator [](const TCHAR *key) const
{
    ValueMap::const_iterator it = mValues.find(key);
    return it->second;
}

Object::ValueMap::const_iterator Object::begin() const
{
    return mValues.begin();
}

Object::ValueMap::const_iterator Object::end() const
{
    return mValues.end();
}

Object::ValueMap::iterator Object::begin()
{
    return mValues.begin();
}

Object::ValueMap::iterator Object::end()
{
    return mValues.end();
}

Object::ValueMap::iterator Object::find(const string_t &key)
{
    return mValues.find(key);
}

Object::ValueMap::const_iterator Object::find(const string_t &key) const
{
    return mValues.find(key);
}

bool Object::HasKey(const string_t &key) const
{
    return find(key) != end();
}

int Object::HasKeys(const std::vector<string_t> &keys) const
{
    for (size_t i = 0; i < keys.size(); i++) {
        if (!HasKey(keys[i]))
            return (int)i;
    }

    return -1;
}

int Object::HasKeys(const TCHAR **keys, int key_count) const
{
    for (int i = 0; i < key_count; i++)
        if (!HasKey(keys[i]))
            return i;

    return -1;
}

void Object::Clear()
{
    mValues.clear();
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
string_t SerializeArray(const Array &a);

string_t SerializeValue(const Value &v)
{
    string_t str;

    static const int BUFF_SZ = 500;
    TCHAR buff[BUFF_SZ];
    switch (v.GetType()) {
    case IntVal         : snprintf(buff, BUFF_SZ, TEXT("%d"), (int)v); str = buff; break;
    case FloatVal       : snprintf(buff, BUFF_SZ, TEXT("%f"), (float)v); str = buff; break;
    case DoubleVal      : snprintf(buff, BUFF_SZ, TEXT("%f"), (double)v); str = buff; break;
    case BoolVal        : str = v ? TEXT("true") : TEXT("false"); break;
    case NULLVal        : str = TEXT("null"); break;
    case ObjectVal      : str = Serialize(v); break;
    case ArrayVal       : str = SerializeArray(v); break;
    case StringVal      : str = string_t(TEXT("\"")) + (string_t)v + string_t(TEXT("\"")); break;
    }

    return str;
}

string_t SerializeArray(const Array &a)
{
    string_t str = TEXT("[");

    bool first = true;
    for (size_t i = 0; i < a.size(); i++) {
        const Value &v = a[i];
        if (!first)
            str += string_t(TEXT(","));

        str += SerializeValue(v);

        first = false;
    }

    str += TEXT("]");
    return str;
}

string_t json::Serialize(const Value &v)
{
    string_t str;

    bool first = true;

    if (v.GetType() == ObjectVal) {
        str = TEXT("{");
        Object obj = v.ToObject();
        for (Object::ValueMap::const_iterator it = obj.begin(); it != obj.end(); ++it) {
            if (!first)
                str += string_t(TEXT(","));

            str += string_t(TEXT("\"")) + it->first + string_t(TEXT("\":")) + SerializeValue(it->second);
            first = false;
        }

        str += TEXT("}");
    } else if (v.GetType() == ArrayVal) {
        str = TEXT("[");
        Array a = v.ToArray();
        for (Array::ValueVector::const_iterator it = a.begin(); it != a.end(); ++it) {
            if (!first)
                str += string_t(TEXT(","));

            str += SerializeValue(*it);
            first = false;
        }

        str += TEXT("]");

    }
    // else it's not valid JSON, as a JSON data structure must be an array or an object. We'll return an empty string.


    return str;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static Value DeserializeArray(string_t &str, std::stack<StackDepthType> &depth_stack);
static Value DeserializeObj(const string_t &_str, std::stack<StackDepthType> &depth_stack);

static Value DeserializeInternal(const string_t &_str, std::stack<StackDepthType> &depth_stack)
{
    Value v;

    string_t str = Trim(_str);
    if (str[0] == TEXT('{')) {
        // Error: Began with a { but doesn't end with one
        if (str[str.length() - 1] != TEXT('}'))
            return Value();

        depth_stack.push(InObject);
        v = DeserializeObj(str, depth_stack);
        if ((v.GetType() == NULLVal) || (depth_stack.top() != InObject))
            return v;

        depth_stack.pop();
    } else if (str[0] == TEXT('[')) {
        // Error: Began with a [ but doesn't end with one
        if (str[str.length() - 1] != TEXT(']'))
            return Value();

        depth_stack.push(InArray);
        v = DeserializeArray(str, depth_stack);
        if ((v.GetType() == NULLVal) || (depth_stack.top() != InArray))
            return v;

        depth_stack.pop();
    } else {
        // Will never get here unless _str is not valid JSON
        return Value();
    }

    return v;
}

static size_t GetEndOfArrayOrObj(const string_t &str, std::stack<StackDepthType> &depth_stack)
{
    size_t i = 1;
    bool in_quote = false;
    size_t original_count = depth_stack.size();

    for (; i < str.length(); i++) {
        if (str[i] == TEXT('\"')) {
            if (str[i - 1] != TEXT('\\'))
                in_quote = !in_quote;
        } else if (!in_quote) {
            if (str[i] == TEXT('['))
                depth_stack.push(InArray);
            else if (str[i] == TEXT('{'))
                depth_stack.push(InObject);
            else if (str[i] == TEXT(']')) {
                StackDepthType t = depth_stack.top();
                if (t != InArray) {
                    // expected to be closing an array but instead we're inside an object block.
                    // Example problem: {]}
                    return string_t::npos;
                }

                size_t count = depth_stack.size();
                depth_stack.pop();
                if (count == original_count)
                    break;
            } else if (str[i] == TEXT('}')) {
                StackDepthType t = depth_stack.top();
                if (t != InObject) {
                    // expected to be closing an object but instead we're inside an array.
                    // Example problem: [}]
                    return string_t::npos;
                }

                size_t count = depth_stack.size();
                depth_stack.pop();
                if (count == original_count)
                    break;
            }
        }
    }

    return i;
}

static string_t UnescapeJSONString(const string_t &str)
{
    string_t s = TEXT("");

    for (string_t::size_type i = 0; i < str.length(); i++) {
        TCHAR c = str[i];
        if ((c == TEXT('\\')) && (i + 1 < str.length())) {
            int skip_ahead = 1;
            unsigned int hex;
            string_t hex_str;

            switch (str[i + 1]) {
            case TEXT('"') :  s.push_back(TEXT('\"')); break;
            case TEXT('\\'):  s.push_back(TEXT('\\')); break;
            case TEXT('/') :  s.push_back(TEXT('/')); break;
            case TEXT('t') :  s.push_back(TEXT('\t')); break;
            case TEXT('n') :  s.push_back(TEXT('\n')); break;
            case TEXT('r') :  s.push_back(TEXT('\r')); break;
            case TEXT('b') :  s.push_back(TEXT('\b')); break;
            case TEXT('f') :  s.push_back(TEXT('\f')); break;
            case TEXT('u') :  skip_ahead = 5;
                hex_str = str.substr(i + 4, 2);
                hex = (unsigned int)std::_tcstoul(hex_str.c_str(), nullptr, 16);
                s.push_back((TCHAR)hex);
                break;

            default: break;
            }

            i += skip_ahead;
        } else
            s.push_back(c);
    }

    return Trim(s);
}

static Value DeserializeValue(string_t &str, bool *had_error, std::stack<StackDepthType> &depth_stack)
{
    Value v;

    *had_error = false;
    str = Trim(str);

    if (str.length() == 0)
        return v;

    if (str[0] == TEXT('[')) {
        // This value is an array, determine the end of it and then deserialize the array
        depth_stack.push(InArray);
        size_t i = GetEndOfArrayOrObj(str, depth_stack);
        if (i == string_t::npos) {
            *had_error = true;
            return Value();
        }

        string_t array_str = str.substr(0, i + 1);
        v = Value(DeserializeArray(array_str, depth_stack));
        str = str.substr(i + 1, str.length());
    } else if (str[0] == TEXT('{')) {
        // This value is an object, determine the end of it and then deserialize the object
        depth_stack.push(InObject);
        size_t i = GetEndOfArrayOrObj(str, depth_stack);

        if (i == string_t::npos) {
            *had_error = true;
            return Value();
        }

        string_t obj_str = str.substr(0, i + 1);
        v = Value(DeserializeInternal(obj_str, depth_stack));
        str = str.substr(i + 1, str.length());
    } else if (str[0] == TEXT('\"')) {
        // This value is a string
        size_t end_quote = GetQuotePos(str, 1);
        if (end_quote == string_t::npos) {
            *had_error = true;
            return Value();
        }

        v = Value(UnescapeJSONString(str.substr(1, end_quote - 1)));
        str = str.substr(end_quote + 1, str.length());
    } else {
        // it's not an object, string, or array so it's either a boolean or a number or null.
        // Numbers can contain an exponent indicator ('e') or a decimal point.
        bool has_dot = false;
        bool has_e = false;
        string_t temp_val;
        size_t i = 0;
        bool found_digit = false;
        bool found_first_valid_char = false;

        for (; i < str.length(); i++) {
            if (str[i] == '.') {
                if (!found_digit) {
                    // As per JSON standards, there must be a digit preceding a decimal point
                    *had_error = true;
                    return Value();
                }

                has_dot = true;
            } else if ((str[i] == TEXT('e')) || (str[i] == TEXT('E'))) {
                if ((_tcsicmp(temp_val.c_str(), TEXT("fals")) != 0) && (_tcsicmp(temp_val.c_str(), TEXT("tru")) != 0)) {
                    // it's not a boolean, check for scientific notation validity. This will also trap booleans with extra 'e' characters like falsee/truee
                    if (!found_digit) {
                        // As per JSON standards, a digit must precede the 'e' notation
                        *had_error = true;
                        return Value();
                    } else if (has_e) {
                        // multiple 'e' characters not allowed
                        *had_error = true;
                        return Value();
                    }

                    has_e = true;
                }
            } else if (str[i] == TEXT(']')) {
                if (depth_stack.empty() || (depth_stack.top() != InArray)) {
                    *had_error = true;
                    return Value();
                }

                depth_stack.pop();
            } else if (str[i] == TEXT('}')) {
                if (depth_stack.empty() || (depth_stack.top() != InObject)) {
                    *had_error = true;
                    return Value();
                }

                depth_stack.pop();
            } else if (str[i] == TEXT(','))
                break;
            else if ((str[i] == TEXT('[')) || (str[i] == TEXT('{'))) {
                // error, we're supposed to be processing things besides arrays/objects in here
                *had_error = true;
                return Value();
            }

            if (!std::isspace(str[i])) {
                if (std::isdigit(str[i]))
                    found_digit = true;

                found_first_valid_char = true;
                temp_val += str[i];
            }
        }

        // store all floating point as doubles. This will also set the float and int values as well.
        if (_tcsicmp(temp_val.c_str(), TEXT("true")) == 0)
            v = Value(true);
        else if (_tcsicmp(temp_val.c_str(), TEXT("false")) == 0)
            v = Value(false);
        else if (has_e || has_dot) {
            TCHAR *end_char;
            errno = 0;
            double d = _tcstod(temp_val.c_str(), &end_char);
            if ((errno != 0) || (*end_char != TEXT('\0'))) {
                // invalid conversion or out of range
                *had_error = true;
                return Value();
            }

            v = Value(d);
        } else if (_tcsicmp(temp_val.c_str(), TEXT("null")) == 0)
            v = Value();
        else {
            // Check if the value is beyond the size of an int and if so, store it as a double
            TCHAR *end_char;
            errno = 0;
            long int ival = _tcstol(temp_val.c_str(), &end_char, 10);
            if (*end_char != TEXT('\0')) {
                // invalid character sequence, not a number
                *had_error = true;
                return Value();
            } else if ((errno == ERANGE) && ((ival == LONG_MAX) || (ival == LONG_MIN))) {
                // value is out of range for a long int, should be a double then. See if we can convert it correctly.
                errno = 0;
                double dval = _tcstod(temp_val.c_str(), &end_char);
                if ((errno != 0) || (*end_char != TEXT('\0'))) {
                    // error in conversion or it's too big for a double
                    *had_error = true;
                    return Value();
                }

                v = Value(dval);
            } else if ((ival >= INT_MIN) && (ival <= INT_MAX)) {
                // valid integer range
                v = Value((int)ival);
            } else {
                // probably running on a very old OS since this block implies that long isn't the same size as int.
                // int is guaranteed to be at least 16 bits and long 32 bits...however nowadays they're almost
                // always the same 32 bit size. But it's possible someone is running this on a very old architecture
                // so for correctness, we'll error out here
                *had_error = true;
                return Value();
            }
        }

        str = str.substr(i, str.length());
    }

    return v;
}

static Value DeserializeArray(string_t &str, std::stack<StackDepthType> &depth_stack)
{
    Array a;
    bool had_error = false;

    str = Trim(str);

    // Arrays begin and end with [], so if we don't find one, it's an error
    if ((str[0] == TEXT('[')) && (str[str.length() - 1] == TEXT(']')))
        str = str.substr(1, str.length() - 2);
    else
        return Value();

    // extract out all values from the array (remember, a value can also be an array or an object)
    while (str.length() > 0) {
        string_t tmp;

        size_t i = 0;
        for (; i < str.length(); i++) {
            // If we get to an object or array, parse it:
            if ((str[i] == TEXT('{')) || (str[i] == TEXT('['))) {
                Value v = DeserializeValue(str, &had_error, depth_stack);
                if (had_error)
                    return Value();

                if (v.GetType() != NULLVal)
                    a.push_back(v);

                break;
            }

            bool terminate_parsing = false;

            if ((str[i] == TEXT(',')) || (str[i] == TEXT(']')))
                terminate_parsing = true;           // hit the end of a value, parse it in the next block
            else {
                // keep grabbing chars to build up the value
                tmp += str[i];
                if (i == str.length() - 1)
                    terminate_parsing = true; // end of string, finish parsing
            }

            if (terminate_parsing) {
                Value v = DeserializeValue(tmp, &had_error, depth_stack);
                if (had_error)
                    return Value();

                if (v.GetType() != NULLVal)
                    a.push_back(v);

                str = str.substr(i + 1, str.length());
                break;
            }
        }
    }

    return a;
}

static Value DeserializeObj(const string_t &_str, std::stack<StackDepthType> &depth_stack)
{
    Object obj;

    string_t str = Trim(_str);

    // Objects begin and end with {} so if we don't find a pair, it's an error
    if ((str[0] != TEXT('{')) && (str[str.length() - 1] != TEXT('}')))
        return Value();
    else
        str = str.substr(1, str.length() - 2);

    // Get all key/value pairs in this object...
    while (str.length() > 0) {
        // Get the key name
        size_t start_quote_idx = GetQuotePos(str);
        size_t end_quote_idx = GetQuotePos(str, start_quote_idx + 1);
        size_t colon_idx = str.find(':', end_quote_idx);

        if ((start_quote_idx == string_t::npos) || (end_quote_idx == string_t::npos) || (colon_idx == string_t::npos))
            return Value(); // can't find key name

        string_t key = str.substr(start_quote_idx + 1, end_quote_idx - start_quote_idx - 1);
        if (key.length() == 0)
            return Value();

        bool had_error = false;
        str = str.substr(colon_idx + 1, str.length());

        // We have the key, now extract the value from the string
        obj[key] = DeserializeValue(str, &had_error, depth_stack);
        if (had_error)
            return Value();
    }

    return obj;
}

Value json::Deserialize(const string_t &str)
{
    std::stack<StackDepthType> depth_stack;
    return DeserializeInternal(str, depth_stack);
}

