/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/helpers/hw_helper_tests.h"

using HwHelperTestGen12Lp = HwHelperTest;

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpA0WhenAdjustDefaultEngineTypeCalledThenRcsIsReturned) {
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = REVISION_A0;

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);
}

TGLLPTEST_F(HwHelperTestGen12Lp, givenTgllpA0WhenAdjustDefaultEngineTypeCalledThenCcsIsReturned) {
    hardwareInfo.featureTable.ftrCCSNode = true;
    hardwareInfo.platform.usRevId = REVISION_B;

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_CCS, hardwareInfo.capabilityTable.defaultEngineType);
}
