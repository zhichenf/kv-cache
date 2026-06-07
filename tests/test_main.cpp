#include <gtest/gtest.h>
#include "test_helper.h"

int main(int argc, char** argv) {
    // 初始化 Logger（读取 config.txt）
    InitTestLogger();
    
    // 初始化 Google Test
    testing::InitGoogleTest(&argc, argv);
    
    return RUN_ALL_TESTS();
}
