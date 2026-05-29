#include "bytecode.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace gc {

using namespace sm;

struct HeapObject {
    Type type{Type::None};
    std::intptr_t fwd;
    HeapObject() = default;
    explicit HeapObject(Type type) : type(type), fwd(0) {}
    HeapObject(const HeapObject&)            = delete;
    HeapObject& operator=(const HeapObject&) = delete;
    HeapObject(HeapObject&&)                 = delete;
};

struct DRef : HeapObject {
    HeapObject* ref;
    explicit DRef(HeapObject* ref) : HeapObject(Type::Ref), ref(ref) {}
};

struct DBool : HeapObject {
    bool value;
    explicit DBool(bool value) : HeapObject(Type::Bool), value(value) {}
};

struct DInt : HeapObject {
    int value;
    explicit DInt(int value) : HeapObject(Type::Int), value(value) {}
};

struct DReal : HeapObject {
    float value;
    explicit DReal(float value) : HeapObject(Type::Real), value(value) {}
};

struct DString : HeapObject {
    size_t length;
    char data[0];
    explicit DString(const std::string& str) : HeapObject(Type::String), length(str.size()) {
        std::strcpy(data, str.c_str());
    }
};

struct DArrayData : HeapObject {
    size_t size;
    std::pair<size_t, DRef*> elements[0]; // <index, value>

    explicit DArrayData(std::span<std::pair<size_t, DRef*>> elems)
        : HeapObject(Type::Array),
          size(elems.size()) {
        std::copy(elems.begin(), elems.end(), elements);
    }
};

struct DArray : HeapObject {
    DArrayData* data;

    explicit DArray(DArrayData* data) : HeapObject(Type::Array), data(data) {}
};

struct DTuple : HeapObject {
    size_t length;
    std::pair<size_t, DRef*> elements[0]; // <string_table index, value>

    explicit DTuple(std::span<std::pair<size_t, DRef*>> elems)
        : HeapObject(Type::Tuple),
          length(elems.size()) {
        std::copy(elems.begin(), elems.end(), elements);
    }
};

struct DFunc : HeapObject {
    const FunctionScheme* scheme;
    DRef* capture[0];

    explicit DFunc(const FunctionScheme* scheme, std::span<DRef*> captured)
        : HeapObject(Type::Func),
          scheme(scheme) {
        std::copy(captured.begin(), captured.end(), capture);
    }
};

struct Heap {
public:
    std::uint8_t* arena;
    size_t arena_size;
    HeapObject* none;

    Heap(size_t size) : arena(new std::uint8_t[size]), arena_size(size) {
        none = new (allocate(sizeof(HeapObject))) HeapObject(Type::None);
    }
    ~Heap() { delete[] arena; }
    std::uint8_t* ptr = arena + arena_size;

    std::uint8_t* allocate(size_t size) { return ptr = reinterpret_cast<std::uint8_t*>(std::uintptr_t(ptr - size) & std::uintptr_t(~7)); }

    HeapObject* make_none() { return none; }

    DInt* make_int(int value) { return new (allocate(sizeof(DInt))) DInt(value); }

    DReal* make_real(float value) { return new (allocate(sizeof(DReal))) DReal(value); }

    DBool* make_bool(bool value) { return new (allocate(sizeof(DBool))) DBool(value); }

    DString* make_string(const std::string& str) {
        return new (allocate(sizeof(DString) + str.size() + 1)) DString(str);
    }

    DArray* make_array(std::span<std::pair<size_t, DRef*>> elements) {
        DArrayData* data =
            new (allocate(sizeof(DArrayData) + elements.size() * sizeof(std::pair<size_t, DRef*>)))
                DArrayData(elements);
        return new (allocate(sizeof(DArray))) DArray(data);
    }
    void replace_array_data(DArray* array, std::pair<size_t, DRef*> element) {
        std::vector<std::pair<size_t, DRef*>> new_elements(
            array->data->elements, array->data->elements + array->data->size);
        new_elements.insert(
            std::lower_bound(new_elements.begin(), new_elements.end(), element,
                             [](const auto& a, const auto& b) { return a.first < b.first; }),
            element);
        DArrayData* new_data = new (allocate(
            sizeof(DArrayData) + (array->data->size + 1) * sizeof(std::pair<size_t, DRef*>)))
            DArrayData(new_elements);
        array->data = new_data;
    }

    DTuple* make_tuple(std::span<std::pair<size_t, DRef*>> elements) {
        return new (allocate(sizeof(DTuple) + elements.size() * sizeof(std::pair<size_t, DRef*>)))
            DTuple(elements);
    }

    DFunc* make_function(const FunctionScheme* scheme, std::span<DRef*> captured) {
        return new (allocate(sizeof(DFunc) + captured.size() * sizeof(DRef*)))
            DFunc(scheme, captured);
    }

    DRef* make_ref(HeapObject* ref) { return new (allocate(sizeof(DRef))) DRef(ref); }
};
} // namespace gc