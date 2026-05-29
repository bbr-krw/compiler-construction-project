#include "sm_runtime.hpp"

#include <gtest/gtest.h>
#include <span>
#include <vector>

using namespace sm;

TEST(SMGC, CollectKeepsReachableObjects) {
    Runtime runtime{};
    auto& heap = runtime.heap;

    auto* i1 = heap.make_int(1);
    auto* i2 = heap.make_int(2);

    auto* r1 = heap.make_ref(i1);
    auto* r2 = heap.make_ref(i2);

    // keep only r1 as a root
    heap.roots.push_back((HeapObject**)&r1);

    heap.collect();

    EXPECT_TRUE(heap.from_space.contains((HeapObject*)r1));
    EXPECT_FALSE(heap.from_space.contains((HeapObject*)r2));

    HeapObject* resolved = resolve_ref((HeapObject*)r1);
    ASSERT_EQ(resolved->type, Type::Int);
    EXPECT_EQ(reinterpret_cast<DInt*>(resolved)->value, 1);

    heap.roots.pop_back();
}

TEST(SMGC, CollectEvacuatesNestedObjects) {
    Runtime runtime{};
    auto& heap = runtime.heap;

    auto* inner = heap.make_int(42);
    auto* inner_ref = heap.make_ref(inner);

    std::vector<std::pair<size_t, DRef*>> elems;
    elems.emplace_back(0, inner_ref);
    auto* arr = heap.make_array(std::span(elems));

    auto* arr_ref = heap.make_ref(arr);
    heap.roots.push_back((HeapObject**)&arr_ref);

    heap.collect();

    HeapObject* arr_obj = resolve_ref((HeapObject*)arr_ref);
    ASSERT_EQ(arr_obj->type, Type::Array);
    DArray* darr = reinterpret_cast<DArray*>(arr_obj);
    DRef* stored = darr->data->elements[0].second;

    HeapObject* inner_res = resolve_ref((HeapObject*)stored);
    ASSERT_EQ(inner_res->type, Type::Int);
    EXPECT_EQ(reinterpret_cast<DInt*>(inner_res)->value, 42);

    heap.roots.pop_back();
}

TEST(SMGC, UnrootedObjectsAreNotEvacuated) {
    Runtime runtime{};
    auto& heap = runtime.heap;

    auto* a = heap.make_int(7);
    auto* b = heap.make_int(9);
    auto* ra = heap.make_ref(a);
    auto* rb = heap.make_ref(b);

    heap.roots.push_back((HeapObject**)&ra);

    heap.collect();

    EXPECT_TRUE(heap.from_space.contains((HeapObject*)ra));
    EXPECT_TRUE(heap.to_space.contains((HeapObject*)rb));

    heap.roots.pop_back();
}

TEST(SMGC, UsedSpaceReducesAfterCollection) {
    Runtime runtime{};
    auto& heap = runtime.heap;

    size_t before_count = heap.count_objects_in_from();

    constexpr size_t N = 8;
    std::vector<DRef*> refs;
    refs.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        auto* v = heap.make_int(static_cast<int>(i));
        refs.push_back(heap.make_ref(v));
    }

    size_t after_alloc_count = heap.count_objects_in_from();
    EXPECT_GE(after_alloc_count, before_count + N * 2);

    // root only first 3
    for (size_t i = 0; i < 3; ++i) {
        heap.roots.push_back((HeapObject**)&refs[i]);
    }

    heap.collect();

    size_t after_collect_count = heap.count_objects_in_from();
    EXPECT_LT(after_collect_count, after_alloc_count);
    EXPECT_GE(after_collect_count, 1 + 2 * 3);

    for (size_t i = 3; i < N; ++i) {
        EXPECT_FALSE(heap.from_space.contains((HeapObject*)refs[i]));
    }

    for (size_t i = 0; i < 3; ++i)
        heap.roots.pop_back();
}

TEST(SMGC, ClosuresAndMultipleFrames) {
    Runtime runtime{};
    auto& heap = runtime.heap;

    auto* val = heap.make_int(555);
    auto* var_ref = heap.make_ref(val);

    std::vector<DRef*> captureVec{var_ref};

    FunctionScheme scheme;
    scheme.args_number = 0;
    scheme.locals_number = 0;
    scheme.capture = std::vector<Location>(captureVec.size(), Location{CAPTURED, 0});

    Frame frame1(&runtime, &scheme, 0, {}, captureVec);
    Frame frame2(&runtime, &scheme, 0, {}, captureVec);

    auto capture_span = std::span<DRef*>(captureVec.data(), captureVec.size());
    DFunc* func = heap.make_function(&scheme, capture_span);
    heap.roots.push_back((HeapObject**)&func);

    EXPECT_EQ(func->capture[0], frame1.captured[0]);
    EXPECT_EQ(frame2.captured[0], frame1.captured[0]);

    heap.collect();

    EXPECT_EQ(func->capture[0], frame1.captured[0]);
    EXPECT_EQ(frame2.captured[0], frame1.captured[0]);

    HeapObject* resolved = resolve_ref((HeapObject*)frame1.captured[0]);
    ASSERT_EQ(resolved->type, Type::Int);
    EXPECT_EQ(reinterpret_cast<DInt*>(resolved)->value, 555);

    heap.roots.pop_back();
}
