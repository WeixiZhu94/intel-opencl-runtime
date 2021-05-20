/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"
#include "test.h"

namespace NEO {
extern GMM_INIT_IN_ARGS passedInputArgs;
extern bool copyInputArgs;

TEST(OsInterfaceTest, whenOsInterfaceSetupsGmmInputArgsThenFileDescriptorIsSetWithValueOfAdapterBdf) {
    MockExecutionEnvironment executionEnvironment;
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    auto osInterface = new OSInterface();
    rootDeviceEnvironment->osInterface.reset(osInterface);

    auto drm = new DrmMock(fakeFd, *rootDeviceEnvironment);
    drm->setPciPath("01:23.4");

    osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    GMM_INIT_IN_ARGS gmmInputArgs = {};
    EXPECT_EQ(0u, gmmInputArgs.FileDescriptor);
    osInterface->getDriverModel()->setGmmInputArgs(&gmmInputArgs);
    EXPECT_NE(0u, gmmInputArgs.FileDescriptor);

    ADAPTER_BDF expectedAdapterBDF{};
    expectedAdapterBDF.Bus = 0x1;
    expectedAdapterBDF.Device = 0x23;
    expectedAdapterBDF.Function = 0x4;
    EXPECT_EQ(expectedAdapterBDF.Data, gmmInputArgs.FileDescriptor);
    EXPECT_EQ(GMM_CLIENT::GMM_OCL_VISTA, gmmInputArgs.ClientType);
}

TEST(GmmHelperTest, whenCreateGmmHelperWithoutOsInterfaceThenPassedFileDescriptorIsZeroed) {
    std::unique_ptr<GmmHelper> gmmHelper;
    VariableBackup<decltype(passedInputArgs)> passedInputArgsBackup(&passedInputArgs);
    VariableBackup<decltype(copyInputArgs)> copyInputArgsBackup(&copyInputArgs, true);

    uint32_t expectedFileDescriptor = 0u;

    gmmHelper.reset(new GmmHelper(nullptr, defaultHwInfo.get()));
    EXPECT_EQ(expectedFileDescriptor, passedInputArgs.FileDescriptor);
    EXPECT_EQ(GMM_CLIENT::GMM_OCL_VISTA, passedInputArgs.ClientType);
}
} // namespace NEO
