/*******************************************************************************
 * Copyright (c) 2024, Tencent Inc.
 * All rights reserved.
 * Project:  hadmap_server
 * Modify history:
 ******************************************************************************/

#include <gtest/gtest.h>
#include <json/value.h>
#include <json/writer.h>
#include <iostream>
#include "engine/config.h"

#include "server_map_cache/map_data_cache.h"
using namespace std;

TEST(testMapCacheCase, testNormal) {
  std::string req("test23.sqlite");
  std::wstring wstrParams = CEngineConfig::Instance().MBStr2WStr(req.c_str());

  std::wstring status = CMapDataCache::Instance().LoadHadMap(wstrParams.c_str());

  std::wstring m_wstrSuccess = L"true";

  EXPECT_EQ(status, m_wstrSuccess);

  // target.erase(std::remove(target.begin(), target.end(), ' '),
  // target.end());

  // EXPECT_EQ(target, strRes);
  // EXPECT_EQ(add(2, 3), 5);
}


/*******************************************************************************
 * Copyright (c) 2024, Tencent Inc.
 * All rights reserved.
 * Project:  hadmap_server
 * Modify history:
 ******************************************************************************/

// #include <scene_wrapper.h>
// #include <iostream>

// int testLoad(const wchar_t* strHadmap) {
//   loadHadmap(strHadmap);
//   getRoadData(strHadmap);
//   getLaneData(strHadmap);
//   getLaneBoundaryData(strHadmap);
//   getLaneLinkData(strHadmap);
//   getMapObjectData(strHadmap);
//   return 0;
// }

// int main(int argc, char** argv) {
//   std::cout << "scenario server test service started!" << std::endl;

//   init(L"C:\\Users\\wangheng\\AppData\\Roaming\\Electron\\scenario");

//   testInfo(L"scenario server test started");

//   int nLoadCount = 10;
//   for (int i = 0; i < nLoadCount; ++i) {
//     std::cout << "loop: " << i << std::endl;
//     testLoad(L"d2d_20190726.xodr");
//     testLoad(L"1001-1-101-180324-v0.0.1.sqlite");
//     testLoad(L"geely.sqlite");
//   }

//   testInfo(L"scenario server test end!");

//   deinit();
//   std::cout << "scenario server test service exited!" << std::endl;

//   std::cout << "press any key to exit ..." << std::endl;

//   std::string strInfo;
//   std::cin >> strInfo;

//   return 0;
// }
