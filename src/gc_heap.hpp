#include "bytecode.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <print>
#include <vector>

namespace gc {

using namespace sm;

struct HeapObject {
    Type type{Type::None};
    HeapObject* fwd;
    HeapObject() = default;
    explicit HeapObject(Type type) : type(type), fwd(nullptr) {}
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
        : HeapObject(Type::ArrayData),
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
    struct SemiSpace {
        std::uint8_t *begin, *end;
        std::uint8_t* ptr;

        std::uint8_t* allocate(size_t size) {
            ptr += align_size(size);
            if (ptr > end) {
                throw std::runtime_error("out of memory");
            }
            return ptr - align_size(size);
        }

        bool contains(HeapObject* obj) const {
            return reinterpret_cast<std::uint8_t*>(obj) >= begin &&
                   reinterpret_cast<std::uint8_t*>(obj) < end;
        }
    };

public:
    std::uint8_t* arena;
    size_t size;
    SemiSpace from_space;
    SemiSpace to_space;
    HeapObject* none;

    std::vector<HeapObject**> roots;

    explicit Heap(size_t size) : arena(new std::uint8_t[size]), size(size), roots() {
        from_space = {reinterpret_cast<std::uint8_t*>(arena),
                      reinterpret_cast<std::uint8_t*>(arena + size / 2),
                      reinterpret_cast<std::uint8_t*>(arena)};
        to_space   = {reinterpret_cast<std::uint8_t*>(arena + size / 2),
                      reinterpret_cast<std::uint8_t*>(arena + size),
                      reinterpret_cast<std::uint8_t*>(arena + size / 2)};
        none       = new (allocate(sizeof(HeapObject))) HeapObject(Type::None);
        roots.push_back(&none);
    }
    ~Heap() { delete[] arena; }

    std::uint8_t* allocate(size_t size) {
        if (from_space.ptr + align_size(size) > from_space.end) {
            collect();
        }
        return from_space.allocate(size);
    }

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

    static size_t align_size(size_t size) { return (size + 7) & ~7; }

    static size_t get_object_size(HeapObject* obj) {
        switch (obj->type) {
        case Type::None:
            return sizeof(HeapObject);
        case Type::Int:
            return sizeof(DInt);
        case Type::Real:
            return sizeof(DReal);
        case Type::Bool:
            return sizeof(DBool);
        case Type::String: {
            DString* dstr = reinterpret_cast<DString*>(obj);
            return (sizeof(DString) + dstr->length + 1); // align to 8 bytes
        }
        case Type::Array: {
            return sizeof(DArray);
        }
        case Type::ArrayData: {
            DArrayData* darr_data = reinterpret_cast<DArrayData*>(obj);
            return sizeof(DArrayData) + darr_data->size * sizeof(std::pair<size_t, DRef*>);
        }
        case Type::Tuple: {
            DTuple* dtup = reinterpret_cast<DTuple*>(obj);
            return sizeof(DTuple) + dtup->length * sizeof(std::pair<size_t, DRef*>);
        }
        case Type::Func: {
            DFunc* dfunc = reinterpret_cast<DFunc*>(obj);
            return sizeof(DFunc) + dfunc->scheme->capture.size() * sizeof(DRef*);
        }
        case Type::Ref:
            return sizeof(DRef);
        default:
            __builtin_unreachable();
        }
    }

    static size_t get_total_size(HeapObject* obj) { return align_size(get_object_size(obj)); }

    void collect() {
        std::println(std::cerr, "GC: free of {}", (size_t)(from_space.end - from_space.ptr) * 8);
        std::uint8_t* scanned = to_space.begin;
        for (HeapObject** root : roots) {
            process_ref(root);
        }
        while (scanned < to_space.ptr) {
            HeapObject* obj = reinterpret_cast<HeapObject*>(scanned);
            scanned += get_total_size(obj);
            switch (obj->type) {
            case Type::Ref: {
                DRef* dref = reinterpret_cast<DRef*>(obj);
                process_ref(&dref->ref);
                break;
            }
            case Type::Array: {
                DArray* darr = reinterpret_cast<DArray*>(obj);
                process_ref((HeapObject**)&darr->data);
                break;
            }
            case Type::ArrayData: {
                DArrayData* data = reinterpret_cast<DArrayData*>(obj);
                for (size_t i = 0; i < data->size; ++i) {
                    process_ref((HeapObject**)(&data->elements[i].second));
                }
                break;
            }
            case Type::Tuple: {
                DTuple* dtup = reinterpret_cast<DTuple*>(obj);
                for (size_t i = 0; i < dtup->length; ++i) {
                    process_ref((HeapObject**)(&dtup->elements[i].second));
                }
                break;
            }
            case Type::Func: {
                DFunc* dfunc = reinterpret_cast<DFunc*>(obj);
                for (size_t i = 0; i < dfunc->scheme->capture.size(); ++i) {
                    process_ref((HeapObject**)(&dfunc->capture[i]));
                }
                break;
            }
            default:
                break;
            }
        }
        std::swap(from_space, to_space);
        to_space.ptr = to_space.begin;
        none         = new (allocate(sizeof(HeapObject))) HeapObject(Type::None);
        std::println(std::cerr, "GC: free of {}", (size_t)(from_space.end - from_space.ptr) * 8);
    }

    HeapObject* evacuate(HeapObject* obj) {
        size_t obj_size     = get_total_size(obj);
        HeapObject* new_obj = reinterpret_cast<HeapObject*>(to_space.allocate(obj_size));
        std::memcpy(new_obj, obj, obj_size); // copy to new location
        obj->fwd = new_obj;                  // set forwarding pointer
        return new_obj;
    }

    void process_ref(HeapObject** ref) {
        HeapObject* obj = *ref;
        if (from_space.contains(obj)) {
            HeapObject* new_obj = obj->fwd ? obj->fwd : evacuate(obj);
            *ref                = new_obj; // update reference to point to new location
        }
    }
};
} // namespace gc