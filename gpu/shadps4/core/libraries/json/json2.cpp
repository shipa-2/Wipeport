// SPDX-License-Identifier: GPL-2.0-only
// wocport: libSceJson2 (sce::Json), which shadPS4 does not implement and WipEout imports. After
// Ryty's (github.com/aarvsn/Ryty, GPL-2.0, core/libs/prx/libSceJson2/Export.cpp): every class is
// one pointer to host data (the title allocates the SDK's sizes, which are at least that, and
// never looks inside), an object member is {String key, 8 bytes, Value value} as titles read it
// through an iterator. Changed from Ryty's: no C++ exception leaves a call (it would unwind
// through the title's frames): misuse is logged and answered with an empty value; a Value the
// title never constructed is constructed on first use; the members WipEout needs that Ryty lacks
// (Object::find, the iterators' ->, ==, copy, Array's copy, Value::getValue/clear/
// setNullAccessCallBack, String::size, MemAllocator::notifyError) are added.
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <list>
#include <mutex>
#include <string>

#include "common/logging/log.h"
#include "core/libraries/libs.h"

namespace Libraries::Json2 {
namespace {

enum ValueType : std::int32_t {
    TypeNull = 0,
    TypeBoolean = 1,
    TypeInteger = 2,
    TypeUInteger = 3,
    TypeReal = 4,
    TypeString = 5,
    TypeArray = 6,
    TypeObject = 7,
};

constexpr int JsonErrorParse = static_cast<int>(0x80920101);
constexpr int JsonErrorInvalidArgument = static_cast<int>(0x80920102);

struct Node;
struct Value {
    Node* node;
};
struct String {
    std::string* text;
};
struct Array {
    std::list<Value>* items;
};
struct Pair;
struct Object {
    std::list<Pair>* items;
};
struct ArrayIterator {
    std::list<Value>::iterator it;
};
struct ObjectIterator {
    std::list<Pair>::iterator it;
};
struct Pair {
    String key;
    std::uint64_t reserved;
    Value value;
};
static_assert(sizeof(Value) == 8 && sizeof(String) == 8 && sizeof(Array) == 8 && sizeof(Object) == 8);
static_assert(sizeof(ArrayIterator) == 8 && sizeof(ObjectIterator) == 8);
static_assert(offsetof(Pair, value) == 0x10);

struct Node {
    ValueType type = TypeNull;
    bool boolean = false;
    std::int64_t integer = 0;
    std::uint64_t uinteger = 0;
    double real = 0;
    String string{};
    Array array{};
    Object object{};
};

using NullAccessCallback = const Value& (PS4_SYSV_ABI*)(ValueType, const Value*, void*);

struct Globals {
    std::mutex mutex;
    NullAccessCallback callback = nullptr;
    void* context = nullptr;
};

Globals& State() {
    static Globals globals;
    return globals;
}

void Construct(String& s, const std::string& text) {
    s.text = new std::string(text);
}
void Construct(Array& a) {
    a.items = new std::list<Value>();
}
void Construct(Object& o) {
    o.items = new std::list<Pair>();
}
void Construct(Value& v) {
    v.node = new Node();
}
void Destroy(Value& v);
void CopyInto(Value& destination, const Value& source);

void Destroy(String& s) {
    delete s.text;
    s.text = nullptr;
}
void Destroy(Array& a) {
    if (!a.items) {
        return;
    }
    for (auto& item : *a.items) {
        Destroy(item);
    }
    delete a.items;
    a.items = nullptr;
}
void Destroy(Object& o) {
    if (!o.items) {
        return;
    }
    for (auto& pair : *o.items) {
        Destroy(pair.key);
        Destroy(pair.value);
    }
    delete o.items;
    o.items = nullptr;
}

void Clear(Node& n) {
    if (n.type == TypeString) {
        Destroy(n.string);
    }
    if (n.type == TypeArray) {
        Destroy(n.array);
    }
    if (n.type == TypeObject) {
        Destroy(n.object);
    }
    n = Node{};
}

void Destroy(Value& v) {
    if (!v.node) {
        return;
    }
    Clear(*v.node);
    delete v.node;
    v.node = nullptr;
}

// A Value the title zero-initialized instead of constructing (a static one) is constructed here.
Node& NodeOf(const Value& v) {
    auto& value = const_cast<Value&>(v);
    if (!value.node) {
        LOG_WARNING(Lib_Json2, "sce::Json::Value used before construction: constructed now");
        Construct(value);
    }
    return *value.node;
}
std::string& TextOf(const String& s) {
    auto& string = const_cast<String&>(s);
    if (!string.text) {
        string.text = new std::string();
    }
    return *string.text;
}
std::list<Value>& ItemsOf(const Array& a) {
    auto& array = const_cast<Array&>(a);
    if (!array.items) {
        Construct(array);
    }
    return *array.items;
}
std::list<Pair>& ItemsOf(const Object& o) {
    auto& object = const_cast<Object&>(o);
    if (!object.items) {
        Construct(object);
    }
    return *object.items;
}

void CopyArray(Array& destination, const Array& source) {
    Construct(destination);
    for (const auto& item : ItemsOf(source)) {
        Value copy{};
        CopyInto(copy, item);
        destination.items->push_back(copy);
    }
}

void CopyObject(Object& destination, const Object& source) {
    Construct(destination);
    for (const auto& pair : ItemsOf(source)) {
        Pair copy{};
        Construct(copy.key, TextOf(pair.key));
        CopyInto(copy.value, pair.value);
        destination.items->push_back(copy);
    }
}

void AssignNode(Node& destination, const Node& source) {
    Node copy{};
    copy.type = source.type;
    copy.boolean = source.boolean;
    copy.integer = source.integer;
    copy.uinteger = source.uinteger;
    copy.real = source.real;
    if (source.type == TypeString) {
        Construct(copy.string, TextOf(source.string));
    }
    if (source.type == TypeArray) {
        CopyArray(copy.array, source.array);
    }
    if (source.type == TypeObject) {
        CopyObject(copy.object, source.object);
    }
    Clear(destination);
    destination = copy;
}

void CopyInto(Value& destination, const Value& source) {
    Construct(destination);
    AssignNode(*destination.node, NodeOf(source));
}

void SetType(Node& n, ValueType type) {
    if (type < TypeNull || type > TypeObject) {
        LOG_ERROR(Lib_Json2, "sce::Json::Value::set: invalid value type {}", static_cast<int>(type));
        type = TypeNull;
    }
    Clear(n);
    n.type = type;
    if (type == TypeString) {
        Construct(n.string, std::string());
    }
    if (type == TypeArray) {
        Construct(n.array);
    }
    if (type == TypeObject) {
        Construct(n.object);
    }
}

const Value& Defaults(ValueType type) {
    static Value values[8]{};
    static std::once_flag once;
    std::call_once(once, [] {
        for (int i = 0; i < 8; ++i) {
            Construct(values[i]);
            SetType(*values[i].node, static_cast<ValueType>(i));
        }
    });
    return values[type >= TypeNull && type <= TypeObject ? type : TypeNull];
}

// The library's answer when a value is read as a type it is not: the title's callback, else an
// empty value of that type (as the SDK documents).
const Value& NullAccess(ValueType requested, const Value* parent) {
    auto& state = State();
    NullAccessCallback callback;
    void* context;
    {
        std::lock_guard lock(state.mutex);
        callback = state.callback;
        context = state.context;
    }
    if (callback) {
        const Value& result = callback(requested, parent, context);
        if (result.node && (requested == TypeNull || result.node->type == requested)) {
            return result;
        }
    }
    return Defaults(requested);
}

const Node& Typed(const Value* self, ValueType requested) {
    const Node& n = NodeOf(*self);
    if (n.type == requested) {
        return n;
    }
    return NodeOf(NullAccess(requested, self));
}

void AppendEscaped(std::string& out, const std::string& text) {
    out += '"';
    for (const unsigned char c : text) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20) {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                out += buffer;
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    out += '"';
}

// JSON has no NaN or infinity: written as null, not refused.
std::string RealText(double value) {
    if (!std::isfinite(value)) {
        return "null";
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.17g", value);
    return buffer;
}

void Serialize(std::string& out, const Node& n) {
    switch (n.type) {
    case TypeNull:
        out += "null";
        return;
    case TypeBoolean:
        out += n.boolean ? "true" : "false";
        return;
    case TypeInteger:
        out += std::to_string(n.integer);
        return;
    case TypeUInteger:
        out += std::to_string(n.uinteger);
        return;
    case TypeReal:
        out += RealText(n.real);
        return;
    case TypeString:
        AppendEscaped(out, TextOf(n.string));
        return;
    case TypeArray: {
        out += '[';
        bool first = true;
        for (const auto& item : ItemsOf(n.array)) {
            if (!first) {
                out += ',';
            }
            first = false;
            Serialize(out, NodeOf(item));
        }
        out += ']';
        return;
    }
    case TypeObject: {
        out += '{';
        bool first = true;
        for (const auto& pair : ItemsOf(n.object)) {
            if (!first) {
                out += ',';
            }
            first = false;
            AppendEscaped(out, TextOf(pair.key));
            out += ':';
            Serialize(out, NodeOf(pair.value));
        }
        out += '}';
        return;
    }
    }
    out += "null";
}

class Parser {
public:
    Parser(const char* text, std::size_t size) : text_(text), end_(text + size) {}

    bool Parse(Node& out) {
        Skip();
        if (!ParseValue(out)) {
            return false;
        }
        Skip();
        return text_ == end_ || *text_ == '\0';
    }

private:
    const char* text_;
    const char* end_;

    void Skip() {
        while (text_ < end_ && (*text_ == ' ' || *text_ == '\t' || *text_ == '\n' || *text_ == '\r')) {
            ++text_;
        }
    }
    bool Literal(const char* word) {
        const auto length = std::strlen(word);
        if (static_cast<std::size_t>(end_ - text_) < length || std::memcmp(text_, word, length) != 0) {
            return false;
        }
        text_ += length;
        return true;
    }
    static void AppendUtf8(std::string& out, std::uint32_t code) {
        if (code < 0x80) {
            out += static_cast<char>(code);
        } else if (code < 0x800) {
            out += static_cast<char>(0xc0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3f));
        } else if (code < 0x10000) {
            out += static_cast<char>(0xe0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (code & 0x3f));
        } else {
            out += static_cast<char>(0xf0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3f));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (code & 0x3f));
        }
    }
    bool Hex4(std::uint32_t& code) {
        if (end_ - text_ < 4) {
            return false;
        }
        code = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = *text_++;
            code <<= 4;
            if (c >= '0' && c <= '9') {
                code |= static_cast<std::uint32_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                code |= static_cast<std::uint32_t>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                code |= static_cast<std::uint32_t>(c - 'A' + 10);
            } else {
                return false;
            }
        }
        return true;
    }
    bool ParseString(std::string& out) {
        if (text_ >= end_ || *text_ != '"') {
            return false;
        }
        ++text_;
        while (text_ < end_ && *text_ != '"') {
            const char c = *text_++;
            if (static_cast<unsigned char>(c) < 0x20) {
                return false;
            }
            if (c != '\\') {
                out += c;
                continue;
            }
            if (text_ >= end_) {
                return false;
            }
            switch (*text_++) {
            case '"':
                out += '"';
                break;
            case '\\':
                out += '\\';
                break;
            case '/':
                out += '/';
                break;
            case 'b':
                out += '\b';
                break;
            case 'f':
                out += '\f';
                break;
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            case 'u': {
                std::uint32_t code = 0;
                if (!Hex4(code)) {
                    return false;
                }
                if (code >= 0xd800 && code < 0xdc00) {
                    std::uint32_t low = 0;
                    if (!Literal("\\u") || !Hex4(low) || low < 0xdc00 || low >= 0xe000) {
                        return false;
                    }
                    code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
                } else if (code >= 0xdc00 && code < 0xe000) {
                    return false;
                }
                AppendUtf8(out, code);
                break;
            }
            default:
                return false;
            }
        }
        if (text_ >= end_) {
            return false;
        }
        ++text_;
        return true;
    }
    bool ParseNumber(Node& out) {
        const char* start = text_;
        if (text_ < end_ && *text_ == '-') {
            ++text_;
        }
        if (text_ >= end_ || *text_ < '0' || *text_ > '9') {
            return false;
        }
        if (*text_ == '0') {
            ++text_;
        } else {
            while (text_ < end_ && *text_ >= '0' && *text_ <= '9') {
                ++text_;
            }
        }
        bool integral = true;
        if (text_ < end_ && *text_ == '.') {
            integral = false;
            ++text_;
            if (text_ >= end_ || *text_ < '0' || *text_ > '9') {
                return false;
            }
            while (text_ < end_ && *text_ >= '0' && *text_ <= '9') {
                ++text_;
            }
        }
        if (text_ < end_ && (*text_ == 'e' || *text_ == 'E')) {
            integral = false;
            ++text_;
            if (text_ < end_ && (*text_ == '+' || *text_ == '-')) {
                ++text_;
            }
            if (text_ >= end_ || *text_ < '0' || *text_ > '9') {
                return false;
            }
            while (text_ < end_ && *text_ >= '0' && *text_ <= '9') {
                ++text_;
            }
        }
        const std::string text(start, text_);
        if (integral) {
            errno = 0;
            if (text[0] == '-') {
                const long long value = std::strtoll(text.c_str(), nullptr, 10);
                if (errno == 0) {
                    out.type = TypeInteger;
                    out.integer = value;
                    return true;
                }
            } else {
                const unsigned long long value = std::strtoull(text.c_str(), nullptr, 10);
                if (errno == 0) {
                    if (value <= static_cast<unsigned long long>(INT64_MAX)) {
                        out.type = TypeInteger;
                        out.integer = static_cast<std::int64_t>(value);
                    } else {
                        out.type = TypeUInteger;
                        out.uinteger = value;
                    }
                    return true;
                }
            }
        }
        out.type = TypeReal;
        out.real = std::strtod(text.c_str(), nullptr);
        return true;
    }
    bool ParseValue(Node& out) {
        if (text_ >= end_) {
            return false;
        }
        switch (*text_) {
        case 'n':
            if (!Literal("null")) {
                return false;
            }
            SetType(out, TypeNull);
            return true;
        case 't':
            if (!Literal("true")) {
                return false;
            }
            SetType(out, TypeBoolean);
            out.boolean = true;
            return true;
        case 'f':
            if (!Literal("false")) {
                return false;
            }
            SetType(out, TypeBoolean);
            return true;
        case '"': {
            std::string text;
            if (!ParseString(text)) {
                return false;
            }
            SetType(out, TypeString);
            *out.string.text = std::move(text);
            return true;
        }
        case '[': {
            ++text_;
            SetType(out, TypeArray);
            Skip();
            if (text_ < end_ && *text_ == ']') {
                ++text_;
                return true;
            }
            for (;;) {
                Value item{};
                Construct(item);
                out.array.items->push_back(item);
                Skip();
                if (!ParseValue(*item.node)) {
                    return false;
                }
                Skip();
                if (text_ < end_ && *text_ == ',') {
                    ++text_;
                    continue;
                }
                if (text_ < end_ && *text_ == ']') {
                    ++text_;
                    return true;
                }
                return false;
            }
        }
        case '{': {
            ++text_;
            SetType(out, TypeObject);
            Skip();
            if (text_ < end_ && *text_ == '}') {
                ++text_;
                return true;
            }
            for (;;) {
                Skip();
                std::string key;
                if (!ParseString(key)) {
                    return false;
                }
                Skip();
                if (text_ >= end_ || *text_ != ':') {
                    return false;
                }
                ++text_;
                Skip();
                Pair pair{};
                Construct(pair.key, key);
                Construct(pair.value);
                out.object.items->push_back(pair);
                if (!ParseValue(*pair.value.node)) {
                    return false;
                }
                Skip();
                if (text_ < end_ && *text_ == ',') {
                    ++text_;
                    continue;
                }
                if (text_ < end_ && *text_ == '}') {
                    ++text_;
                    return true;
                }
                return false;
            }
        }
        default:
            return ParseNumber(out);
        }
    }
};

// sce::Json::MemAllocator is abstract: the title's subclass replaces this table in its own
// constructor. A call through it is the title's bug; logged, it answers nothing.
void* PS4_SYSV_ABI JsonMemAllocatorPureVirtual() {
    LOG_ERROR(Lib_Json2, "sce::Json::MemAllocator: pure virtual function called");
    return nullptr;
}

void* const JsonMemAllocatorVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual),
    reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual),
    reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual),
    reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual),
    reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual),
    reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual),
};

Value& ObjectEntry(Object& object, const std::string& key) {
    for (auto& pair : ItemsOf(object)) {
        if (TextOf(pair.key) == key) {
            return pair.value;
        }
    }
    Pair pair{};
    Construct(pair.key, key);
    Construct(pair.value);
    object.items->push_back(pair);
    return object.items->back().value;
}

const Value* FindMember(const Value* self, const std::string& key) {
    const Node& n = NodeOf(*self);
    if (n.type == TypeObject) {
        for (const auto& pair : ItemsOf(n.object)) {
            if (TextOf(pair.key) == key) {
                return &pair.value;
            }
        }
    }
    return nullptr;
}

// ---- sce::Json::Initializer ----
int PS4_SYSV_ABI InitializerCtor(void*) {
    return 0;
}
int PS4_SYSV_ABI InitializerDtor(void*) {
    return 0;
}
int PS4_SYSV_ABI InitializerInitialize(void*, const void* parameter) {
    return parameter ? 0 : JsonErrorInvalidArgument;
}
int PS4_SYSV_ABI InitializerTerminate(void*) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.callback = nullptr;
    state.context = nullptr;
    return 0;
}
int PS4_SYSV_ABI InitializerSetGlobalNullAccessCallBack(void*, NullAccessCallback callback, void* context) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.callback = callback;
    state.context = context;
    return 0;
}

// ---- sce::Json::MemAllocator ----
void PS4_SYSV_ABI MemAllocatorCtor(void* self) {
    *static_cast<void* const**>(self) = &JsonMemAllocatorVtable[2];
}
void PS4_SYSV_ABI MemAllocatorDtor(void* self) {
    *static_cast<void* const**>(self) = &JsonMemAllocatorVtable[2];
}
// The library tells the allocator an allocation failed; it has none of its own to fail.
void PS4_SYSV_ABI MemAllocatorNotifyError(void*, int error, std::size_t size, void*) {
    LOG_ERROR(Lib_Json2, "sce::Json::MemAllocator::notifyError({:#x}, {})", static_cast<u32>(error), size);
}

// ---- sce::Json::String ----
void PS4_SYSV_ABI StringCtor(String* self) {
    Construct(*self, std::string());
}
void PS4_SYSV_ABI StringCtorText(String* self, const char* text) {
    Construct(*self, text ? text : "");
}
void PS4_SYSV_ABI StringCopy(String* self, const String* other) {
    Construct(*self, TextOf(*other));
}
void PS4_SYSV_ABI StringDtor(String* self) {
    Destroy(*self);
}
String* PS4_SYSV_ABI StringAssign(String* self, const String* other) {
    if (self != other) {
        TextOf(*self) = TextOf(*other);
    }
    return self;
}
const char* PS4_SYSV_ABI StringCStr(const String* self) {
    return TextOf(*self).c_str();
}
bool PS4_SYSV_ABI StringEmpty(const String* self) {
    return TextOf(*self).empty();
}
std::size_t PS4_SYSV_ABI StringLength(const String* self) {
    return TextOf(*self).size();
}
bool PS4_SYSV_ABI StringEqualsText(const String* self, const char* text) {
    return text != nullptr && TextOf(*self) == text;
}

// ---- sce::Json::Array ----
void PS4_SYSV_ABI ArrayCtor(Array* self) {
    Construct(*self);
}
void PS4_SYSV_ABI ArrayCopy(Array* self, const Array* other) {
    CopyArray(*self, *other);
}
void PS4_SYSV_ABI ArrayDtor(Array* self) {
    Destroy(*self);
}
void PS4_SYSV_ABI ArrayPushBack(Array* self, const Value* value) {
    Value copy{};
    CopyInto(copy, *value);
    ItemsOf(*self).push_back(copy);
}
ArrayIterator* PS4_SYSV_ABI ArrayBegin(ArrayIterator* result, const Array* self) {
    result->it = ItemsOf(*self).begin();
    return result;
}
ArrayIterator* PS4_SYSV_ABI ArrayEnd(ArrayIterator* result, const Array* self) {
    result->it = ItemsOf(*self).end();
    return result;
}
bool PS4_SYSV_ABI ArrayEmpty(const Array* self) {
    return ItemsOf(*self).empty();
}
std::size_t PS4_SYSV_ABI ArraySize(const Array* self) {
    return ItemsOf(*self).size();
}
const Value* PS4_SYSV_ABI ArrayBack(const Array* self) {
    if (ItemsOf(*self).empty()) {
        LOG_ERROR(Lib_Json2, "sce::Json::Array::back: empty array");
        return &NullAccess(TypeNull, nullptr);
    }
    return &self->items->back();
}
void PS4_SYSV_ABI ArrayIteratorDtor(ArrayIterator*) {}
ArrayIterator* PS4_SYSV_ABI ArrayIteratorNext(ArrayIterator* self) {
    ++self->it;
    return self;
}
Value* PS4_SYSV_ABI ArrayIteratorDeref(const ArrayIterator* self) {
    return &*self->it;
}
bool PS4_SYSV_ABI ArrayIteratorNotEqual(const ArrayIterator* self, const ArrayIterator* other) {
    return self->it != other->it;
}

// ---- sce::Json::Object ----
void PS4_SYSV_ABI ObjectCtor(Object* self) {
    Construct(*self);
}
void PS4_SYSV_ABI ObjectCopy(Object* self, const Object* other) {
    CopyObject(*self, *other);
}
void PS4_SYSV_ABI ObjectDtor(Object* self) {
    Destroy(*self);
}
Object* PS4_SYSV_ABI ObjectAssign(Object* self, const Object* other) {
    if (self == other) {
        return self;
    }
    Object copy{};
    CopyObject(copy, *other);
    Destroy(*self);
    *self = copy;
    return self;
}
void PS4_SYSV_ABI ObjectClear(Object* self) {
    for (auto& pair : ItemsOf(*self)) {
        Destroy(pair.key);
        Destroy(pair.value);
    }
    self->items->clear();
}
Value* PS4_SYSV_ABI ObjectIndex(Object* self, const String* key) {
    return &ObjectEntry(*self, TextOf(*key));
}
ObjectIterator* PS4_SYSV_ABI ObjectBegin(ObjectIterator* result, const Object* self) {
    result->it = ItemsOf(*self).begin();
    return result;
}
ObjectIterator* PS4_SYSV_ABI ObjectEnd(ObjectIterator* result, const Object* self) {
    result->it = ItemsOf(*self).end();
    return result;
}
ObjectIterator* PS4_SYSV_ABI ObjectFind(ObjectIterator* result, const Object* self, const String* key) {
    auto& items = ItemsOf(*self);
    result->it = items.end();
    const std::string& wanted = TextOf(*key);
    for (auto it = items.begin(); it != items.end(); ++it) {
        if (TextOf(it->key) == wanted) {
            result->it = it;
            break;
        }
    }
    return result;
}
void PS4_SYSV_ABI ObjectIteratorCopy(ObjectIterator* self, const ObjectIterator* other) {
    self->it = other->it;
}
void PS4_SYSV_ABI ObjectIteratorDtor(ObjectIterator*) {}
ObjectIterator* PS4_SYSV_ABI ObjectIteratorNext(ObjectIterator* self) {
    ++self->it;
    return self;
}
Pair* PS4_SYSV_ABI ObjectIteratorDeref(const ObjectIterator* self) {
    return &*self->it;
}
bool PS4_SYSV_ABI ObjectIteratorNotEqual(const ObjectIterator* self, const ObjectIterator* other) {
    return self->it != other->it;
}
bool PS4_SYSV_ABI ObjectIteratorEqual(const ObjectIterator* self, const ObjectIterator* other) {
    return self->it == other->it;
}

// ---- sce::Json::Value ----
void PS4_SYSV_ABI ValueCtor(Value* self) {
    Construct(*self);
}
void PS4_SYSV_ABI ValueCtorBool(Value* self, bool value) {
    Construct(*self);
    self->node->type = TypeBoolean;
    self->node->boolean = value;
}
void PS4_SYSV_ABI ValueCtorInteger(Value* self, std::int64_t value) {
    Construct(*self);
    self->node->type = TypeInteger;
    self->node->integer = value;
}
void PS4_SYSV_ABI ValueCtorUInteger(Value* self, std::uint64_t value) {
    Construct(*self);
    self->node->type = TypeUInteger;
    self->node->uinteger = value;
}
void PS4_SYSV_ABI ValueCtorReal(Value* self, double value) {
    Construct(*self);
    self->node->type = TypeReal;
    self->node->real = value;
}
void PS4_SYSV_ABI ValueCtorText(Value* self, const char* text) {
    Construct(*self);
    SetType(*self->node, TypeString);
    *self->node->string.text = text ? text : "";
}
void PS4_SYSV_ABI ValueCtorString(Value* self, const String* text) {
    Construct(*self);
    SetType(*self->node, TypeString);
    *self->node->string.text = TextOf(*text);
}
void PS4_SYSV_ABI ValueCtorArray(Value* self, const Array* array) {
    Construct(*self);
    self->node->type = TypeArray;
    CopyArray(self->node->array, *array);
}
void PS4_SYSV_ABI ValueCtorObject(Value* self, const Object* object) {
    Construct(*self);
    self->node->type = TypeObject;
    CopyObject(self->node->object, *object);
}
void PS4_SYSV_ABI ValueCopy(Value* self, const Value* other) {
    CopyInto(*self, *other);
}
void PS4_SYSV_ABI ValueDtor(Value* self) {
    Destroy(*self);
}
Value* PS4_SYSV_ABI ValueAssign(Value* self, const Value* other) {
    if (self != other) {
        AssignNode(NodeOf(*self), NodeOf(*other));
    }
    return self;
}
void PS4_SYSV_ABI ValueClear(Value* self) {
    SetType(NodeOf(*self), TypeNull);
}
int PS4_SYSV_ABI ValueSetType(Value* self, ValueType type) {
    SetType(NodeOf(*self), type);
    return 0;
}
int PS4_SYSV_ABI ValueSetBool(Value* self, bool value) {
    SetType(NodeOf(*self), TypeBoolean);
    self->node->boolean = value;
    return 0;
}
int PS4_SYSV_ABI ValueSetInteger(Value* self, std::int64_t value) {
    SetType(NodeOf(*self), TypeInteger);
    self->node->integer = value;
    return 0;
}
int PS4_SYSV_ABI ValueSetUInteger(Value* self, std::uint64_t value) {
    SetType(NodeOf(*self), TypeUInteger);
    self->node->uinteger = value;
    return 0;
}
int PS4_SYSV_ABI ValueSetReal(Value* self, double value) {
    SetType(NodeOf(*self), TypeReal);
    self->node->real = value;
    return 0;
}
int PS4_SYSV_ABI ValueSetText(Value* self, const char* text) {
    if (!text) {
        return JsonErrorInvalidArgument;
    }
    const std::string copy(text);
    SetType(NodeOf(*self), TypeString);
    *self->node->string.text = copy;
    return 0;
}
int PS4_SYSV_ABI ValueSetString(Value* self, const String* text) {
    const std::string copy(TextOf(*text));
    SetType(NodeOf(*self), TypeString);
    *self->node->string.text = copy;
    return 0;
}
int PS4_SYSV_ABI ValueSetArray(Value* self, const Array* array) {
    Array copy{};
    CopyArray(copy, *array);
    Clear(NodeOf(*self));
    self->node->type = TypeArray;
    self->node->array = copy;
    return 0;
}
int PS4_SYSV_ABI ValueSetObject(Value* self, const Object* object) {
    Object copy{};
    CopyObject(copy, *object);
    Clear(NodeOf(*self));
    self->node->type = TypeObject;
    self->node->object = copy;
    return 0;
}
int PS4_SYSV_ABI ValueSetValue(Value* self, const Value* other) {
    if (self != other) {
        AssignNode(NodeOf(*self), NodeOf(*other));
    }
    return 0;
}
// A static member: the callback for reads of a missing or other-typed value.
int PS4_SYSV_ABI ValueSetNullAccessCallBack(NullAccessCallback callback, void* context) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.callback = callback;
    state.context = context;
    return 0;
}
ValueType PS4_SYSV_ABI ValueGetType(const Value* self) {
    return NodeOf(*self).type;
}
const bool* PS4_SYSV_ABI ValueGetBoolean(const Value* self) {
    return &Typed(self, TypeBoolean).boolean;
}
const std::int64_t* PS4_SYSV_ABI ValueGetInteger(const Value* self) {
    Node& n = NodeOf(*self);
    if (n.type == TypeUInteger && n.uinteger <= static_cast<std::uint64_t>(INT64_MAX)) {
        n.integer = static_cast<std::int64_t>(n.uinteger);
        return &n.integer;
    }
    return &Typed(self, TypeInteger).integer;
}
const std::uint64_t* PS4_SYSV_ABI ValueGetUInteger(const Value* self) {
    Node& n = NodeOf(*self);
    if (n.type == TypeInteger && n.integer >= 0) {
        n.uinteger = static_cast<std::uint64_t>(n.integer);
        return &n.uinteger;
    }
    return &Typed(self, TypeUInteger).uinteger;
}
const double* PS4_SYSV_ABI ValueGetReal(const Value* self) {
    Node& n = NodeOf(*self);
    if (n.type == TypeInteger || n.type == TypeUInteger) {
        n.real = n.type == TypeInteger ? static_cast<double>(n.integer) : static_cast<double>(n.uinteger);
        return &n.real;
    }
    return &Typed(self, TypeReal).real;
}
const String* PS4_SYSV_ABI ValueGetString(const Value* self) {
    return &Typed(self, TypeString).string;
}
const Array* PS4_SYSV_ABI ValueGetArray(const Value* self) {
    return &Typed(self, TypeArray).array;
}
const Object* PS4_SYSV_ABI ValueGetObject(const Value* self) {
    return &Typed(self, TypeObject).object;
}
std::size_t PS4_SYSV_ABI ValueCount(const Value* self) {
    const Node& n = NodeOf(*self);
    if (n.type == TypeArray) {
        return ItemsOf(n.array).size();
    }
    if (n.type == TypeObject) {
        return ItemsOf(n.object).size();
    }
    return 0;
}
const Value* PS4_SYSV_ABI ValueIndexText(const Value* self, const char* key) {
    const Value* found = key ? FindMember(self, key) : nullptr;
    return found ? found : &NullAccess(TypeNull, self);
}
const Value* PS4_SYSV_ABI ValueGetValue(const Value* self, const String* key) {
    const Value* found = FindMember(self, TextOf(*key));
    return found ? found : &NullAccess(TypeNull, self);
}
const Value* PS4_SYSV_ABI ValueIndexNumber(const Value* self, std::size_t index) {
    const Node& n = NodeOf(*self);
    if (n.type == TypeArray && index < ItemsOf(n.array).size()) {
        auto it = n.array.items->begin();
        std::advance(it, static_cast<std::ptrdiff_t>(index));
        return &*it;
    }
    return &NullAccess(TypeNull, self);
}
Value* PS4_SYSV_ABI ValueReferValue(Value* self, const String* key) {
    return const_cast<Value*>(FindMember(self, TextOf(*key)));
}
Array* PS4_SYSV_ABI ValueReferArray(Value* self) {
    Node& n = NodeOf(*self);
    if (n.type == TypeNull) {
        SetType(n, TypeArray);
    }
    if (n.type != TypeArray) {
        LOG_ERROR(Lib_Json2, "sce::Json::Value::referArray: value is not an array");
        return nullptr;
    }
    return &n.array;
}
Object* PS4_SYSV_ABI ValueReferObject(Value* self) {
    Node& n = NodeOf(*self);
    if (n.type == TypeNull) {
        SetType(n, TypeObject);
    }
    if (n.type != TypeObject) {
        LOG_ERROR(Lib_Json2, "sce::Json::Value::referObject: value is not an object");
        return nullptr;
    }
    return &n.object;
}
int PS4_SYSV_ABI ValueSerialize(Value* self, String* out) {
    std::string text;
    Serialize(text, NodeOf(*self));
    TextOf(*out) = std::move(text);
    return 0;
}
int PS4_SYSV_ABI ValueToString(const Value* self, String* out) {
    const Node& n = NodeOf(*self);
    if (n.type == TypeString) {
        TextOf(*out) = TextOf(n.string);
        return 0;
    }
    std::string text;
    Serialize(text, n);
    TextOf(*out) = std::move(text);
    return 0;
}

// ---- sce::Json::Parser ----
int PS4_SYSV_ABI ParserParse(Value* out, const char* text, std::size_t size) {
    if (!text || !out) {
        return JsonErrorInvalidArgument;
    }
    Node parsed{};
    Parser parser(text, size);
    if (!parser.Parse(parsed)) {
        Clear(parsed);
        return JsonErrorParse;
    }
    Clear(NodeOf(*out));
    *out->node = parsed;
    return 0;
}

} // namespace

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    // NIDs of the mangled names (scripts/prepare.py nid()).
    LIB_FUNCTION("cK6bYHf-Q5E", "libSceJson2", 1, "libSceJson", InitializerCtor); // _ZN3sce4Json11InitializerC1Ev
    LIB_FUNCTION("RujUxbr3haM", "libSceJson2", 1, "libSceJson", InitializerDtor); // _ZN3sce4Json11InitializerD1Ev
    LIB_FUNCTION("Cxwy7wHq4J0", "libSceJson2", 1, "libSceJson", InitializerInitialize); // _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE
    LIB_FUNCTION("PR5k1penBLM", "libSceJson2", 1, "libSceJson", InitializerTerminate); // _ZN3sce4Json11Initializer9terminateEv
    LIB_FUNCTION("00oCq0RwSAY", "libSceJson2", 1, "libSceJson", InitializerSetGlobalNullAccessCallBack); // _ZN3sce4Json11Initializer27setGlobalNullAccessCallBackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_
    LIB_FUNCTION("-hJRce8wn1U", "libSceJson2", 1, "libSceJson", MemAllocatorCtor); // _ZN3sce4Json12MemAllocatorC2Ev
    LIB_FUNCTION("OcAgPxcq5Vk", "libSceJson2", 1, "libSceJson", MemAllocatorDtor); // _ZN3sce4Json12MemAllocatorD2Ev
    LIB_FUNCTION("i-XwZjw0OOY", "libSceJson2", 1, "libSceJson", MemAllocatorNotifyError); // _ZN3sce4Json12MemAllocator11notifyErrorEimPv
    LIB_FUNCTION("qSmqLXXCPas", "libSceJson2", 1, "libSceJson", StringCtor); // _ZN3sce4Json6StringC1Ev
    LIB_FUNCTION("9KUZFjI1IxA", "libSceJson2", 1, "libSceJson", StringCtorText); // _ZN3sce4Json6StringC1EPKc
    LIB_FUNCTION("0CAesfH963Q", "libSceJson2", 1, "libSceJson", StringCopy); // _ZN3sce4Json6StringC1ERKS1_
    LIB_FUNCTION("cG1VE2HMl6c", "libSceJson2", 1, "libSceJson", StringDtor); // _ZN3sce4Json6StringD1Ev
    LIB_FUNCTION("cn9svYGWKDQ", "libSceJson2", 1, "libSceJson", StringAssign); // _ZN3sce4Json6StringaSERKS1_
    LIB_FUNCTION("L1KAkYWml-M", "libSceJson2", 1, "libSceJson", StringCStr); // _ZNK3sce4Json6String5c_strEv
    LIB_FUNCTION("wM4LO2iK3s8", "libSceJson2", 1, "libSceJson", StringEmpty); // _ZNK3sce4Json6String5emptyEv
    LIB_FUNCTION("EUH+EmT-v9E", "libSceJson2", 1, "libSceJson", StringLength); // _ZNK3sce4Json6String6lengthEv
    LIB_FUNCTION("UVBkipSD2sw", "libSceJson2", 1, "libSceJson", StringLength); // _ZNK3sce4Json6String4sizeEv
    LIB_FUNCTION("VbFjEs--uiA", "libSceJson2", 1, "libSceJson", StringEqualsText); // _ZNK3sce4Json6StringeqEPKc
    LIB_FUNCTION("JP-PtKMiI1E", "libSceJson2", 1, "libSceJson", ArrayCtor); // _ZN3sce4Json5ArrayC1Ev
    LIB_FUNCTION("bI5AGFMydrA", "libSceJson2", 1, "libSceJson", ArrayCopy); // _ZN3sce4Json5ArrayC1ERKS1_
    LIB_FUNCTION("HJ8GpRT1aiw", "libSceJson2", 1, "libSceJson", ArrayDtor); // _ZN3sce4Json5ArrayD1Ev
    LIB_FUNCTION("zQtLRTqceMY", "libSceJson2", 1, "libSceJson", ArrayPushBack); // _ZN3sce4Json5Array9push_backERKNS0_5ValueE
    LIB_FUNCTION("bcH5EnFE2xY", "libSceJson2", 1, "libSceJson", ArrayBegin); // _ZNK3sce4Json5Array5beginEv
    LIB_FUNCTION("WXF2ihRF+B8", "libSceJson2", 1, "libSceJson", ArrayEnd); // _ZNK3sce4Json5Array3endEv
    LIB_FUNCTION("9uP25i6ipno", "libSceJson2", 1, "libSceJson", ArrayEmpty); // _ZNK3sce4Json5Array5emptyEv
    LIB_FUNCTION("rQGJeNjOuUk", "libSceJson2", 1, "libSceJson", ArraySize); // _ZNK3sce4Json5Array4sizeEv
    LIB_FUNCTION("bAM9Qwofus0", "libSceJson2", 1, "libSceJson", ArrayBack); // _ZNK3sce4Json5Array4backEv
    LIB_FUNCTION("9yLjn46Ypfs", "libSceJson2", 1, "libSceJson", ArrayIteratorDtor); // _ZN3sce4Json5Array8iteratorD1Ev
    LIB_FUNCTION("w5+VCznos5E", "libSceJson2", 1, "libSceJson", ArrayIteratorNext); // _ZN3sce4Json5Array8iteratorppEv
    LIB_FUNCTION("wcgr5mte7T8", "libSceJson2", 1, "libSceJson", ArrayIteratorDeref); // _ZNK3sce4Json5Array8iteratordeEv
    LIB_FUNCTION("iAIYn4oAWvI", "libSceJson2", 1, "libSceJson", ArrayIteratorDeref); // _ZNK3sce4Json5Array8iteratorptEv
    LIB_FUNCTION("5AZPp99ogrc", "libSceJson2", 1, "libSceJson", ArrayIteratorNotEqual); // _ZNK3sce4Json5Array8iteratorneERKS2_
    LIB_FUNCTION("OJPTonqdg0I", "libSceJson2", 1, "libSceJson", ObjectCtor); // _ZN3sce4Json6ObjectC1Ev
    LIB_FUNCTION("a+W7HHlwpBs", "libSceJson2", 1, "libSceJson", ObjectCopy); // _ZN3sce4Json6ObjectC1ERKS1_
    LIB_FUNCTION("5JmzZt8twAo", "libSceJson2", 1, "libSceJson", ObjectDtor); // _ZN3sce4Json6ObjectD1Ev
    LIB_FUNCTION("urOpESTBZmo", "libSceJson2", 1, "libSceJson", ObjectAssign); // _ZN3sce4Json6ObjectaSERKS1_
    LIB_FUNCTION("oH8aBmLU+fc", "libSceJson2", 1, "libSceJson", ObjectClear); // _ZN3sce4Json6Object5clearEv
    LIB_FUNCTION("ERuf9y0DY84", "libSceJson2", 1, "libSceJson", ObjectIndex); // _ZN3sce4Json6ObjectixERKNS0_6StringE
    LIB_FUNCTION("xhAcaIwnrgk", "libSceJson2", 1, "libSceJson", ObjectBegin); // _ZNK3sce4Json6Object5beginEv
    LIB_FUNCTION("ivMCitpSQNk", "libSceJson2", 1, "libSceJson", ObjectEnd); // _ZNK3sce4Json6Object3endEv
    LIB_FUNCTION("ehN1Biw3tw8", "libSceJson2", 1, "libSceJson", ObjectFind); // _ZNK3sce4Json6Object4findERKNS0_6StringE
    LIB_FUNCTION("JATlaeBu+lU", "libSceJson2", 1, "libSceJson", ObjectIteratorCopy); // _ZN3sce4Json6Object8iteratorC1ERKS2_
    LIB_FUNCTION("hoINmSMlYjI", "libSceJson2", 1, "libSceJson", ObjectIteratorDtor); // _ZN3sce4Json6Object8iteratorD1Ev
    LIB_FUNCTION("DlWmn2ZQuWY", "libSceJson2", 1, "libSceJson", ObjectIteratorNext); // _ZN3sce4Json6Object8iteratorppEv
    LIB_FUNCTION("ZCd6IYoD3Bc", "libSceJson2", 1, "libSceJson", ObjectIteratorDeref); // _ZNK3sce4Json6Object8iteratordeEv
    LIB_FUNCTION("EnesV4gXDtE", "libSceJson2", 1, "libSceJson", ObjectIteratorDeref); // _ZNK3sce4Json6Object8iteratorptEv
    LIB_FUNCTION("+isUKw4zud4", "libSceJson2", 1, "libSceJson", ObjectIteratorNotEqual); // _ZNK3sce4Json6Object8iteratorneERKS2_
    LIB_FUNCTION("q3YO1c0RqFo", "libSceJson2", 1, "libSceJson", ObjectIteratorEqual); // _ZNK3sce4Json6Object8iteratoreqERKS2_
    LIB_FUNCTION("qBMjqyBn3OM", "libSceJson2", 1, "libSceJson", ValueCtor); // _ZN3sce4Json5ValueC1Ev
    LIB_FUNCTION("UeuWT+yNdCQ", "libSceJson2", 1, "libSceJson", ValueCtorBool); // _ZN3sce4Json5ValueC1Eb
    LIB_FUNCTION("0lLK8+kDqmE", "libSceJson2", 1, "libSceJson", ValueCtorInteger); // _ZN3sce4Json5ValueC1El
    LIB_FUNCTION("x4AUdbhpRB0", "libSceJson2", 1, "libSceJson", ValueCtorUInteger); // _ZN3sce4Json5ValueC1Em
    LIB_FUNCTION("sOmU4vnx3s0", "libSceJson2", 1, "libSceJson", ValueCtorReal); // _ZN3sce4Json5ValueC1Ed
    LIB_FUNCTION("b9V6fmppLXY", "libSceJson2", 1, "libSceJson", ValueCtorText); // _ZN3sce4Json5ValueC1EPKc
    LIB_FUNCTION("sZIoMRGO+jk", "libSceJson2", 1, "libSceJson", ValueCtorString); // _ZN3sce4Json5ValueC1ERKNS0_6StringE
    LIB_FUNCTION("iZeYfOxtMRg", "libSceJson2", 1, "libSceJson", ValueCtorArray); // _ZN3sce4Json5ValueC1ERKNS0_5ArrayE
    LIB_FUNCTION("3xUXnmUkXfo", "libSceJson2", 1, "libSceJson", ValueCtorObject); // _ZN3sce4Json5ValueC1ERKNS0_6ObjectE
    LIB_FUNCTION("fSb2oQTNrgA", "libSceJson2", 1, "libSceJson", ValueCopy); // _ZN3sce4Json5ValueC1ERKS1_
    LIB_FUNCTION("WTtYf+cNnXI", "libSceJson2", 1, "libSceJson", ValueDtor); // _ZN3sce4Json5ValueD1Ev
    LIB_FUNCTION("4zrm6VrgIAw", "libSceJson2", 1, "libSceJson", ValueAssign); // _ZN3sce4Json5ValueaSERKS1_
    LIB_FUNCTION("FIjXN2TkuTs", "libSceJson2", 1, "libSceJson", ValueClear); // _ZN3sce4Json5Value5clearEv
    LIB_FUNCTION("IKQimvG9Wqs", "libSceJson2", 1, "libSceJson", ValueSetType); // _ZN3sce4Json5Value3setENS0_9ValueTypeE
    LIB_FUNCTION("5yHuiWXo2gg", "libSceJson2", 1, "libSceJson", ValueSetBool); // _ZN3sce4Json5Value3setEb
    LIB_FUNCTION("QxVVYhP-mvg", "libSceJson2", 1, "libSceJson", ValueSetInteger); // _ZN3sce4Json5Value3setEl
    LIB_FUNCTION("SIe1ZmW7e7s", "libSceJson2", 1, "libSceJson", ValueSetUInteger); // _ZN3sce4Json5Value3setEm
    LIB_FUNCTION("BSmWDIkV4w4", "libSceJson2", 1, "libSceJson", ValueSetReal); // _ZN3sce4Json5Value3setEd
    LIB_FUNCTION("n6FC+l9DU70", "libSceJson2", 1, "libSceJson", ValueSetText); // _ZN3sce4Json5Value3setEPKc
    LIB_FUNCTION("6l3Bv2gysNc", "libSceJson2", 1, "libSceJson", ValueSetString); // _ZN3sce4Json5Value3setERKNS0_6StringE
    LIB_FUNCTION("195ad-jAsTU", "libSceJson2", 1, "libSceJson", ValueSetArray); // _ZN3sce4Json5Value3setERKNS0_5ArrayE
    LIB_FUNCTION("dFCphqnd+a4", "libSceJson2", 1, "libSceJson", ValueSetObject); // _ZN3sce4Json5Value3setERKNS0_6ObjectE
    LIB_FUNCTION("XL8+BUqjB1w", "libSceJson2", 1, "libSceJson", ValueSetValue); // _ZN3sce4Json5Value3setERKS1_
    LIB_FUNCTION("Xbl-LYVFNEE", "libSceJson2", 1, "libSceJson", ValueSetNullAccessCallBack); // _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_
    LIB_FUNCTION("SHtAad20YYM", "libSceJson2", 1, "libSceJson", ValueGetType); // _ZNK3sce4Json5Value7getTypeEv
    LIB_FUNCTION("zTwZdI8AZ5Y", "libSceJson2", 1, "libSceJson", ValueGetBoolean); // _ZNK3sce4Json5Value10getBooleanEv
    LIB_FUNCTION("DIxvoy7Ngvk", "libSceJson2", 1, "libSceJson", ValueGetInteger); // _ZNK3sce4Json5Value10getIntegerEv
    LIB_FUNCTION("sn4HNCtNRzY", "libSceJson2", 1, "libSceJson", ValueGetUInteger); // _ZNK3sce4Json5Value11getUIntegerEv
    LIB_FUNCTION("3qrge7L-AU4", "libSceJson2", 1, "libSceJson", ValueGetReal); // _ZNK3sce4Json5Value7getRealEv
    LIB_FUNCTION("epJ6x2LV0kU", "libSceJson2", 1, "libSceJson", ValueGetString); // _ZNK3sce4Json5Value9getStringEv
    LIB_FUNCTION("ONT8As5R1ug", "libSceJson2", 1, "libSceJson", ValueGetArray); // _ZNK3sce4Json5Value8getArrayEv
    LIB_FUNCTION("IlsmvBtMkak", "libSceJson2", 1, "libSceJson", ValueGetObject); // _ZNK3sce4Json5Value9getObjectEv
    LIB_FUNCTION("RBw+4NukeGQ", "libSceJson2", 1, "libSceJson", ValueCount); // _ZNK3sce4Json5Value5countEv
    LIB_FUNCTION("HwDt5lD9Bfo", "libSceJson2", 1, "libSceJson", ValueIndexText); // _ZNK3sce4Json5ValueixEPKc
    LIB_FUNCTION("XlWbvieLj2M", "libSceJson2", 1, "libSceJson", ValueIndexNumber); // _ZNK3sce4Json5ValueixEm
    LIB_FUNCTION("MsMOdxWfbwQ", "libSceJson2", 1, "libSceJson", ValueGetValue); // _ZNK3sce4Json5Value8getValueERKNS0_6StringE
    LIB_FUNCTION("wLsJlmgEIaI", "libSceJson2", 1, "libSceJson", ValueReferValue); // _ZN3sce4Json5Value10referValueERKNS0_6StringE
    LIB_FUNCTION("nM5XqdeXFPw", "libSceJson2", 1, "libSceJson", ValueReferArray); // _ZN3sce4Json5Value10referArrayEv
    LIB_FUNCTION("-NxEk7XLkDY", "libSceJson2", 1, "libSceJson", ValueReferObject); // _ZN3sce4Json5Value11referObjectEv
    LIB_FUNCTION("R7FDWtcN6f8", "libSceJson2", 1, "libSceJson", ValueSerialize); // _ZN3sce4Json5Value9serializeERNS0_6StringE
    LIB_FUNCTION("Ncel8t2Rrpc", "libSceJson2", 1, "libSceJson", ValueToString); // _ZNK3sce4Json5Value8toStringERNS0_6StringE
    LIB_FUNCTION("S5JxQnoGF3E", "libSceJson2", 1, "libSceJson", ParserParse); // _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm
}

} // namespace Libraries::Json2
