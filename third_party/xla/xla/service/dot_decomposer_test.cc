/* Copyright 2019 The OpenXLA Authors.

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

#include "xla/service/dot_decomposer.h"

#include "xla/hlo/utils/hlo_matchers.h"
#include "xla/service/hlo_parser.h"
#include "xla/tests/hlo_test_base.h"
#include "xla/tests/test_utils.h"

namespace op = xla::testing::opcode_matchers;

namespace xla {
namespace {

using DotDecomposerTest = HloTestBase;

TEST_F(DotDecomposerTest, CanonicalizeMultipleNonContractingDims) {
  absl::string_view module_string = R"(
  HloModule module

  ENTRY main {
    p0 = f32[64,63,512]{2,1,0} parameter(0)
    p1 = f32[512,512]{1,0} parameter(1)
    ROOT dot = f32[64,63,512]{2,1,0} dot(p0, p1), lhs_contracting_dims={2},
                                                  rhs_contracting_dims={0}
  })";

  TF_ASSERT_OK_AND_ASSIGN(std::unique_ptr<HloModule> module,
                          ParseAndReturnVerifiedModule(module_string));
  TF_ASSERT_OK_AND_ASSIGN(bool canonicalized,
                          DotDecomposer().Run(module.get()));
  EXPECT_TRUE(canonicalized);
  EXPECT_THAT(module->entry_computation()->root_instruction(),
              op::Reshape(AllOf(op::Dot(op::Reshape(), op::Reshape(),
                                        /*lhs_contracting_dim=*/1,
                                        /*rhs_contracting_dim=*/0),
                                op::Shape("f32[4032,512]"))));
}

TEST_F(DotDecomposerTest, DontCanonicalizeIfNoNoncontractingDims) {
  absl::string_view module_string = R"(
  HloModule module

  ENTRY main {
    p0 = f32[64,4]{1,0} parameter(0)
    p1 = f32[64,4]{1,0} parameter(1)
    ROOT dot = f32[64]{0} dot(p0, p1), lhs_batch_dims={0},
                                       lhs_contracting_dims={1},
                                       rhs_batch_dims={0},
                                       rhs_contracting_dims={1}
  })";

  TF_ASSERT_OK_AND_ASSIGN(std::unique_ptr<HloModule> module,
                          ParseAndReturnVerifiedModule(module_string));
  TF_ASSERT_OK_AND_ASSIGN(bool canonicalized,
                          DotDecomposer().Run(module.get()));
  EXPECT_FALSE(canonicalized);
}

TEST_F(DotDecomposerTest, DontAddLhsNonContractingDimIfOne) {
  absl::string_view module_string = R"(
  HloModule module

  ENTRY main {
    p0 = f32[64,4]{1,0} parameter(0)
    p1 = f32[64,4,2,1]{3,2,1,0} parameter(1)
    ROOT dot = f32[64,2,1]{2,1,0} dot(p0, p1), lhs_batch_dims={0},
                                               lhs_contracting_dims={1},
                                               rhs_batch_dims={0},
                                               rhs_contracting_dims={1}
  })";

  TF_ASSERT_OK_AND_ASSIGN(std::unique_ptr<HloModule> module,
                          ParseAndReturnVerifiedModule(module_string));
  TF_ASSERT_OK_AND_ASSIGN(bool canonicalized,
                          DotDecomposer().Run(module.get()));
  EXPECT_TRUE(canonicalized);
  EXPECT_THAT(module->entry_computation()->root_instruction(),
              op::Reshape(AllOf(op::Dot(op::Reshape(), op::Reshape(),
                                        /*lhs_contracting_dim=*/1,
                                        /*rhs_contracting_dim=*/1),
                                op::Shape("f32[64,2]"))));
}

TEST_F(DotDecomposerTest, DontAddRhsNonContractingDimIfOne) {
  absl::string_view module_string = R"(
  HloModule module

  ENTRY main {
    p0 = f32[64,4,2,1]{3,2,1,0} parameter(0)
    p1 = f32[64,4]{1,0} parameter(1)
    ROOT dot = f32[64,2,1]{2,1,0} dot(p0, p1), lhs_batch_dims={0},
                                               lhs_contracting_dims={1},
                                               rhs_batch_dims={0},
                                               rhs_contracting_dims={1}
  })";

  TF_ASSERT_OK_AND_ASSIGN(std::unique_ptr<HloModule> module,
                          ParseAndReturnVerifiedModule(module_string));
  TF_ASSERT_OK_AND_ASSIGN(bool canonicalized,
                          DotDecomposer().Run(module.get()));
  EXPECT_TRUE(canonicalized);
  EXPECT_THAT(module->entry_computation()->root_instruction(),
              op::Reshape(AllOf(op::Dot(op::Reshape(), op::Reshape(),
                                        /*lhs_contracting_dim=*/2,
                                        /*rhs_contracting_dim=*/1),
                                op::Shape("f32[64,2]"))));
}

}  // namespace
}  // namespace xla
