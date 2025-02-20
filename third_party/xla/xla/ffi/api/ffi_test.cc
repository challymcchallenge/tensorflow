/* Copyright 2023 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "xla/ffi/api/ffi.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/log/check.h"
#include "xla/ffi/call_frame.h"
#include "xla/ffi/ffi_api.h"
#include "xla/stream_executor/device_memory.h"
#include "xla/xla_data.pb.h"
#include "tsl/lib/core/status_test_util.h"
#include "tsl/platform/test.h"
#include "tsl/platform/test_benchmark.h"

namespace xla::ffi {

TEST(FfiTest, DataTypeEnumValue) {
  // Verify that xla::PrimitiveType and xla::ffi::DataType use the same
  // integer value for encoding data types.
  auto encoded = [](auto value) { return static_cast<uint8_t>(value); };

  EXPECT_EQ(encoded(PrimitiveType::PRED), encoded(DataType::PRED));

  EXPECT_EQ(encoded(PrimitiveType::S8), encoded(DataType::S8));
  EXPECT_EQ(encoded(PrimitiveType::S16), encoded(DataType::S16));
  EXPECT_EQ(encoded(PrimitiveType::S32), encoded(DataType::S32));
  EXPECT_EQ(encoded(PrimitiveType::S64), encoded(DataType::S64));

  EXPECT_EQ(encoded(PrimitiveType::U8), encoded(DataType::U8));
  EXPECT_EQ(encoded(PrimitiveType::U16), encoded(DataType::U16));
  EXPECT_EQ(encoded(PrimitiveType::U32), encoded(DataType::U32));
  EXPECT_EQ(encoded(PrimitiveType::U64), encoded(DataType::U64));

  EXPECT_EQ(encoded(PrimitiveType::F16), encoded(DataType::F16));
  EXPECT_EQ(encoded(PrimitiveType::F32), encoded(DataType::F32));
  EXPECT_EQ(encoded(PrimitiveType::F64), encoded(DataType::F64));

  EXPECT_EQ(encoded(PrimitiveType::BF16), encoded(DataType::BF16));
}

TEST(FfiTest, BufferArgument) {
  std::vector<float> storage(4, 0.0f);
  se::DeviceMemoryBase memory(storage.data(), 4 * sizeof(float));

  CallFrameBuilder builder;
  builder.AddBufferArg(memory, PrimitiveType::F32, /*dims=*/{2, 2});
  auto call_frame = builder.Build();

  auto fn = [&](BufferBase<DataType::F32> buffer) {
    EXPECT_EQ(buffer.data, storage.data());
    EXPECT_EQ(buffer.dimensions.size(), 2);
    return Error::Success();
  };

  auto handler = Ffi::Bind().Arg<BufferBase<DataType::F32>>().To(fn);
  auto status = Call(*handler, call_frame);

  TF_ASSERT_OK(status);
}

//===----------------------------------------------------------------------===//
// Performance benchmarks are below.
//===----------------------------------------------------------------------===//

static CallFrameBuilder WithBufferArgs(size_t num_args, size_t rank = 4) {
  se::DeviceMemoryBase memory;
  std::vector<int64_t> dims(4, 1);

  CallFrameBuilder builder;
  for (size_t i = 0; i < num_args; ++i) {
    builder.AddBufferArg(memory, PrimitiveType::F32, dims);
  }
  return builder;
}

//===----------------------------------------------------------------------===//
// BM_BufferArgX1
//===----------------------------------------------------------------------===//

void BM_BufferArgX1(benchmark::State& state) {
  auto call_frame = WithBufferArgs(1).Build();

  auto fn = [](BufferBase<DataType::F32> buffer) {
    benchmark::DoNotOptimize(buffer);
    return Error::Success();
  };

  auto handler = Ffi::Bind().Arg<BufferBase<DataType::F32>>().To(fn);
  for (auto _ : state) {
    CHECK_OK(Call(*handler, call_frame));
  }
}

BENCHMARK(BM_BufferArgX1);

//===----------------------------------------------------------------------===//
// BM_BufferArgX4
//===----------------------------------------------------------------------===//

void BM_BufferArgX4(benchmark::State& state) {
  auto call_frame = WithBufferArgs(4).Build();

  auto fn = [](BufferBase<DataType::F32> b0, BufferBase<DataType::F32> b1,
               BufferBase<DataType::F32> b2, BufferBase<DataType::F32> b3) {
    benchmark::DoNotOptimize(b0);
    benchmark::DoNotOptimize(b1);
    benchmark::DoNotOptimize(b2);
    benchmark::DoNotOptimize(b3);
    return Error::Success();
  };

  auto handler = Ffi::Bind()
                     .Arg<BufferBase<DataType::F32>>()
                     .Arg<BufferBase<DataType::F32>>()
                     .Arg<BufferBase<DataType::F32>>()
                     .Arg<BufferBase<DataType::F32>>()
                     .To(fn);

  for (auto _ : state) {
    CHECK_OK(Call(*handler, call_frame));
  }
}

BENCHMARK(BM_BufferArgX4);

//===----------------------------------------------------------------------===//
// BM_TupleOfI32Attrs
//===----------------------------------------------------------------------===//

struct TupleOfI32 {
  int64_t i32_0;
  int64_t i32_1;
  int64_t i32_2;
  int64_t i32_3;
};

XLA_FFI_REGISTER_STRUCT_ATTR_DECODING(TupleOfI32,
                                      StructMember<int32_t>("i32_0"),
                                      StructMember<int32_t>("i32_1"),
                                      StructMember<int32_t>("i32_2"),
                                      StructMember<int32_t>("i32_3"));

void BM_TupleOfI32Attrs(benchmark::State& state) {
  CallFrameBuilder::AttributesBuilder attrs;
  attrs.Insert("i32_0", 1);
  attrs.Insert("i32_1", 2);
  attrs.Insert("i32_2", 3);
  attrs.Insert("i32_3", 4);

  CallFrameBuilder builder;
  builder.AddAttributes(attrs.Build());
  auto call_frame = builder.Build();

  auto fn = [](TupleOfI32 tuple) {
    benchmark::DoNotOptimize(tuple);
    return Error::Success();
  };

  auto handler = Ffi::Bind().Attrs<TupleOfI32>().To(fn);

  for (auto _ : state) {
    CHECK_OK(Call(*handler, call_frame));
  }
}

BENCHMARK(BM_TupleOfI32Attrs);

}  // namespace xla::ffi
