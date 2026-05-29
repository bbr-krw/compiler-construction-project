#include "interpreter.hpp"

thread_local Heap* g_heap = nullptr;

DValue DValue::make_array(std::map<long long, DValue> m) {
    DValue d;
    d.type = Type::Array;
    d.href = g_heap->alloc_array(std::move(m));
    return d;
}

DValue DValue::make_tuple(std::vector<TupleElem> e) {
    DValue d;
    d.type = Type::Tuple;
    d.href = g_heap->alloc_tuple(std::move(e));
    return d;
}

Heap::Heap(size_t threshold) : threshold_{threshold} {}

GcHandle Heap::alloc_array(std::map<long long, DValue> m) {
    auto h = static_cast<GcHandle>(from_.size());
    from_.push_back(HeapObj{ObjKind::Array, GC_NULL, std::move(m), {}});
    return h;
}

GcHandle Heap::alloc_tuple(std::vector<TupleElem> t) {
    auto h = static_cast<GcHandle>(from_.size());
    from_.push_back(HeapObj{ObjKind::Tuple, GC_NULL, {}, std::move(t)});
    return h;
}

GcHandle Heap::evacuate(GcHandle h) {
    if (h == GC_NULL)
        return GC_NULL;
    HeapObj& obj = from_[h];
    if (obj.fwd != GC_NULL)
        return obj.fwd; // already forwarded

    auto new_h = static_cast<GcHandle>(to_.size());
    to_.push_back(std::move(obj)); // move object to to-space
    to_.back().fwd = GC_NULL;      // clear fwd in the to-space copy
    obj.fwd        = new_h;        // install forwarding pointer in from-space
    return new_h;
}

void Heap::scan_value(DValue& v) {
    if (v.type == DValue::Type::Array || v.type == DValue::Type::Tuple) {
        v.href = evacuate(v.href);
    } else if (v.type == DValue::Type::Func && v.fval) {
        for (auto& frame : v.fval->captured_env) {
            if (!gc_visited_frames_.insert(frame.get()).second)
                continue; // frame already scanned this cycle
            for (auto& [name, val] : *frame)
                scan_value(val);
        }
    }
}

void Heap::scan_obj(GcHandle to_idx) {
    if (to_[to_idx].kind == ObjKind::Array) {
        for (auto& [k, v] : to_[to_idx].arr)
            scan_value(v);
    } else {
        for (auto& elem : to_[to_idx].tup)
            scan_value(elem.value);
    }
}

void Heap::collect(std::vector<DValue*> roots, std::vector<Frame*> env_frames) {
    to_.clear();
    to_.reserve(from_.size());
    gc_visited_frames_.clear();

    for (Frame* f : env_frames)
        gc_visited_frames_.insert(f);

    for (DValue* v : roots)
        scan_value(*v);

    for (GcHandle scan = 0; scan < static_cast<GcHandle>(to_.size()); ++scan)
        scan_obj(scan);

    from_ = std::move(to_);
    to_.clear();
    gc_visited_frames_.clear();
    ++gc_count_;
}
