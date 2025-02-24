// Copyright 2024 Tencent Inc. All rights reserved.
//

#include "HdMap/tx_hd_map_cache_concurrent.h"
#include <gtest/gtest.h>
#include <thread>
#include "utils/dylib.h"
#if USE_TBB
#  include "tbb/task_scheduler_init.h"
#endif

namespace HdMap {

class HadmapCacheConCurrentTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() { scheduler_init_ = std::make_shared<tbb::task_scheduler_init>(1); }

 protected:
  static std::shared_ptr<tbb::task_scheduler_init> scheduler_init_;
};

std::shared_ptr<tbb::task_scheduler_init> HadmapCacheConCurrentTest::scheduler_init_ = nullptr;

TEST_F(HadmapCacheConCurrentTest, Initialize) {
  HadmapCacheConCurrent::InitParams_t param;
  param.strHdMapFilePath = "/saturnv/datas/maps/SH1.sqlite";
  param.SceneOriginGPS = hadmap::txPoint(121.12873077392578, 31.26028060913086, 0);
  HadmapCacheConCurrent::Initialize(param);
}
}  // namespace HdMap