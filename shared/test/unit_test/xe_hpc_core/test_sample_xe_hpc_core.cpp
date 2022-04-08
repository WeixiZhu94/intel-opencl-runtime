/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using PvcOnlyTest = Test<DeviceFixture>;

PVCTEST_F(PvcOnlyTest, WhenGettingHardwareInfoThenPvcIsReturned) {
    EXPECT_EQ(IGFX_PVC, pDevice->getHardwareInfo().platform.eProductFamily);
}

using XeHpcCoreOnlyTest = Test<DeviceFixture>;

XE_HPC_CORETEST_F(XeHpcCoreOnlyTest, WhenGettingRenderCoreFamilyThenOnlyHpcCoreIsReturned) {
    EXPECT_EQ(IGFX_XE_HPC_CORE, pDevice->getRenderCoreFamily());
}
