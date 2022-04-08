/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_fatbinary.h"

#include "shared/offline_compiler/source/ocloc_error_code.h"
#include "shared/offline_compiler/source/utilities/safety_caller.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_info.h"

#include "igfxfmid.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace NEO {
std::vector<PRODUCT_CONFIG> getAllMatchedConfigs(const std::string device, OclocArgHelper *argHelper) {
    std::vector<PRODUCT_CONFIG> allMatchedConfigs;
    auto numeration = argHelper->getMajorMinorRevision(device);
    if (numeration.empty()) {
        return {};
    }
    auto config = argHelper->getProductConfig(numeration);
    std::vector<PRODUCT_CONFIG> allConfigs = argHelper->getAllSupportedProductConfigs();
    uint32_t mask = argHelper->getMaskForConfig(numeration);

    for (auto &productConfig : allConfigs) {
        auto prod = static_cast<uint32_t>(productConfig) & mask;
        if (config == prod) {
            allMatchedConfigs.push_back(productConfig);
        }
    }

    return allMatchedConfigs;
}

bool requestedFatBinary(const std::vector<std::string> &args, OclocArgHelper *helper) {
    for (size_t argIndex = 1; argIndex < args.size(); argIndex++) {
        const auto &currArg = args[argIndex];
        const bool hasMoreArgs = (argIndex + 1 < args.size());
        if ((ConstStringRef("-device") == currArg) && hasMoreArgs) {
            ConstStringRef deviceArg(args[argIndex + 1]);
            auto products = getAllMatchedConfigs(deviceArg.str(), helper);
            if (products.size() > 1) {
                return true;
            }
            return deviceArg.contains("*") || deviceArg.contains("-") || deviceArg.contains(",") || helper->isGen(deviceArg.str());
        }
    }
    return false;
}

std::vector<PRODUCT_FAMILY> getAllSupportedTargetPlatforms() {
    return std::vector<PRODUCT_FAMILY>{ALL_SUPPORTED_PRODUCT_FAMILIES};
}

std::vector<ConstStringRef> toProductNames(const std::vector<PRODUCT_FAMILY> &productIds) {
    std::vector<ConstStringRef> ret;
    for (auto prodId : productIds) {
        ret.push_back(ConstStringRef(hardwarePrefix[prodId], strlen(hardwarePrefix[prodId])));
    }
    return ret;
}

PRODUCT_FAMILY asProductId(ConstStringRef product, const std::vector<PRODUCT_FAMILY> &allSupportedPlatforms) {
    for (auto &family : allSupportedPlatforms) {
        if (product == hardwarePrefix[family]) {
            return family;
        }
    }
    return IGFX_UNKNOWN;
}

std::vector<DeviceMapping> getProductConfigsForOpenRange(ConstStringRef openRange, OclocArgHelper *argHelper, bool rangeTo) {
    std::vector<DeviceMapping> requestedConfigs;
    std::vector<DeviceMapping> allSupportedDeviceConfigs = argHelper->getAllSupportedDeviceConfigs();

    if (argHelper->isGen(openRange.str())) {
        std::vector<GFXCORE_FAMILY> coreIdList;
        auto coreId = argHelper->returnIGFXforGen(openRange.str());
        coreIdList.push_back(static_cast<GFXCORE_FAMILY>(coreId));
        if (rangeTo) {
            auto coreId = coreIdList.back();
            unsigned int coreIt = IGFX_UNKNOWN_CORE;
            ++coreIt;
            while (coreIt <= static_cast<unsigned int>(coreId)) {
                argHelper->getProductConfigsForGfxCoreFamily(static_cast<GFXCORE_FAMILY>(coreIt), requestedConfigs);
                ++coreIt;
            }
        } else {
            unsigned int coreIt = coreIdList.front();
            while (coreIt < static_cast<unsigned int>(IGFX_MAX_CORE)) {
                argHelper->getProductConfigsForGfxCoreFamily(static_cast<GFXCORE_FAMILY>(coreIt), requestedConfigs);
                ++coreIt;
            }
        }
    } else {
        auto productConfig = argHelper->findConfigMatch(openRange.str(), !rangeTo);
        if (productConfig == PRODUCT_CONFIG::UNKNOWN_ISA) {
            argHelper->printf("Unknown device : %s\n", openRange.str().c_str());
            return {};
        }
        auto configIt = std::find_if(allSupportedDeviceConfigs.begin(),
                                     allSupportedDeviceConfigs.end(),
                                     [&cf = productConfig](const DeviceMapping &c) -> bool { return cf == c.config; });
        if (rangeTo) {
            for (auto &deviceConfig : allSupportedDeviceConfigs) {
                if (deviceConfig.config <= productConfig) {
                    requestedConfigs.push_back(deviceConfig);
                }
            }
        } else {
            requestedConfigs.insert(requestedConfigs.end(), configIt, allSupportedDeviceConfigs.end());
        }
    }
    return requestedConfigs;
}

std::vector<DeviceMapping> getProductConfigsForClosedRange(ConstStringRef rangeFrom, ConstStringRef rangeTo, OclocArgHelper *argHelper) {
    std::vector<DeviceMapping> requestedConfigs;
    std::vector<DeviceMapping> allSupportedDeviceConfigs = argHelper->getAllSupportedDeviceConfigs();

    if (argHelper->isGen(rangeFrom.str())) {
        if (false == argHelper->isGen(rangeTo.str())) {
            argHelper->printf("Ranges mixing configs and architecture is not supported, should be architectureFrom-architectureTo or configFrom-configTo\n");
            return {};
        }
        auto coreFrom = argHelper->returnIGFXforGen(rangeFrom.str());
        auto coreTo = argHelper->returnIGFXforGen(rangeTo.str());
        if (static_cast<GFXCORE_FAMILY>(coreFrom) > static_cast<GFXCORE_FAMILY>(coreTo)) {
            std::swap(coreFrom, coreTo);
        }
        while (coreFrom <= coreTo) {
            argHelper->getProductConfigsForGfxCoreFamily(static_cast<GFXCORE_FAMILY>(coreFrom), requestedConfigs);
            coreFrom = static_cast<GFXCORE_FAMILY>(static_cast<unsigned int>(coreFrom) + 1);
        }
    } else {
        auto configFrom = argHelper->findConfigMatch(rangeFrom.str(), true);
        if (configFrom == PRODUCT_CONFIG::UNKNOWN_ISA) {
            argHelper->printf("Unknown device range : %s\n", rangeFrom.str().c_str());
            return {};
        }

        auto configTo = argHelper->findConfigMatch(rangeTo.str(), false);
        if (configTo == PRODUCT_CONFIG::UNKNOWN_ISA) {
            argHelper->printf("Unknown device range : %s\n", rangeTo.str().c_str());
            return {};
        }

        if (configFrom > configTo) {
            configFrom = argHelper->findConfigMatch(rangeTo.str(), true);
            configTo = argHelper->findConfigMatch(rangeFrom.str(), false);
        }

        for (auto &deviceConfig : allSupportedDeviceConfigs) {
            if (deviceConfig.config >= configFrom && deviceConfig.config <= configTo) {
                requestedConfigs.push_back(deviceConfig);
            }
        }
    }

    return requestedConfigs;
}

std::vector<ConstStringRef> getPlatformsForClosedRange(ConstStringRef rangeFrom, ConstStringRef rangeTo, PRODUCT_FAMILY platformFrom, OclocArgHelper *argHelper) {
    std::vector<PRODUCT_FAMILY> requestedPlatforms;
    std::vector<PRODUCT_FAMILY> allSupportedPlatforms = getAllSupportedTargetPlatforms();

    auto platformTo = asProductId(rangeTo, allSupportedPlatforms);
    if (IGFX_UNKNOWN == platformTo) {
        argHelper->printf("Unknown device : %s\n", rangeTo.str().c_str());
        return {};
    }
    if (platformFrom > platformTo) {
        std::swap(platformFrom, platformTo);
    }

    auto from = std::find(allSupportedPlatforms.begin(), allSupportedPlatforms.end(), platformFrom);
    auto to = std::find(allSupportedPlatforms.begin(), allSupportedPlatforms.end(), platformTo) + 1;
    requestedPlatforms.insert(requestedPlatforms.end(), from, to);

    return toProductNames(requestedPlatforms);
}

std::vector<ConstStringRef> getPlatformsForOpenRange(ConstStringRef openRange, PRODUCT_FAMILY prodId, OclocArgHelper *argHelper, bool rangeTo) {
    std::vector<PRODUCT_FAMILY> requestedPlatforms;
    std::vector<PRODUCT_FAMILY> allSupportedPlatforms = getAllSupportedTargetPlatforms();

    auto prodIt = std::find(allSupportedPlatforms.begin(), allSupportedPlatforms.end(), prodId);
    assert(prodIt != allSupportedPlatforms.end());
    if (rangeTo) {
        requestedPlatforms.insert(requestedPlatforms.end(), allSupportedPlatforms.begin(), prodIt + 1);
    } else {
        requestedPlatforms.insert(requestedPlatforms.end(), prodIt, allSupportedPlatforms.end());
    }

    return toProductNames(requestedPlatforms);
}

std::vector<DeviceMapping> getProductConfigsForSpecificTargets(CompilerOptions::TokenizedString targets, OclocArgHelper *argHelper) {
    std::vector<DeviceMapping> requestedConfigs;
    std::vector<DeviceMapping> allSupportedDeviceConfigs = argHelper->getAllSupportedDeviceConfigs();

    for (auto &target : targets) {
        if (argHelper->isGen(target.str())) {
            auto coreId = argHelper->returnIGFXforGen(target.str());
            argHelper->getProductConfigsForGfxCoreFamily(static_cast<GFXCORE_FAMILY>(coreId), requestedConfigs);
        } else {
            auto configFirstEl = argHelper->findConfigMatch(target.str(), true);
            if (configFirstEl == PRODUCT_CONFIG::UNKNOWN_ISA) {
                argHelper->printf("Unknown device range : %s\n", target.str().c_str());
                return {};
            }

            auto configLastEl = argHelper->findConfigMatch(target.str(), false);
            for (auto &deviceConfig : allSupportedDeviceConfigs) {
                if (deviceConfig.config >= configFirstEl && deviceConfig.config <= configLastEl) {
                    requestedConfigs.push_back(deviceConfig);
                }
            }
        }
    }
    return requestedConfigs;
}

std::vector<ConstStringRef> getPlatformsForSpecificTargets(CompilerOptions::TokenizedString targets, OclocArgHelper *argHelper) {
    std::vector<PRODUCT_FAMILY> requestedPlatforms;
    std::vector<PRODUCT_FAMILY> allSupportedPlatforms = getAllSupportedTargetPlatforms();

    for (auto &target : targets) {
        auto prodId = asProductId(target, allSupportedPlatforms);
        if (IGFX_UNKNOWN == prodId) {
            argHelper->printf("Unknown device : %s\n", target.str().c_str());
            return {};
        }
        requestedPlatforms.push_back(prodId);
    }
    return toProductNames(requestedPlatforms);
}

bool isDeviceWithPlatformAbbreviation(ConstStringRef deviceArg, OclocArgHelper *argHelper) {
    std::vector<PRODUCT_FAMILY> allSupportedPlatforms = getAllSupportedTargetPlatforms();
    PRODUCT_FAMILY prodId = IGFX_UNKNOWN;
    auto sets = CompilerOptions::tokenize(deviceArg, ',');
    if (sets[0].contains("-")) {
        auto range = CompilerOptions::tokenize(deviceArg, '-');
        prodId = asProductId(range[0], allSupportedPlatforms);

    } else {
        prodId = asProductId(sets[0], allSupportedPlatforms);
    }
    return prodId != IGFX_UNKNOWN;
}

std::vector<ConstStringRef> getTargetPlatformsForFatbinary(ConstStringRef deviceArg, OclocArgHelper *argHelper) {
    std::vector<PRODUCT_FAMILY> allSupportedPlatforms = getAllSupportedTargetPlatforms();
    std::vector<ConstStringRef> retVal;

    auto sets = CompilerOptions::tokenize(deviceArg, ',');
    if (sets[0].contains("-")) {
        auto range = CompilerOptions::tokenize(deviceArg, '-');
        if (range.size() > 2) {
            argHelper->printf("Invalid range : %s - should be from-to or -to or from-\n", sets[0].str().c_str());
            return {};
        }
        auto prodId = asProductId(range[0], allSupportedPlatforms);
        if (range.size() == 1) {
            bool rangeTo = ('-' == sets[0][0]);
            retVal = getPlatformsForOpenRange(range[0], prodId, argHelper, rangeTo);
        } else {
            retVal = getPlatformsForClosedRange(range[0], range[1], prodId, argHelper);
        }
    } else {
        retVal = getPlatformsForSpecificTargets(sets, argHelper);
    }
    return retVal;
}

std::vector<DeviceMapping> getTargetConfigsForFatbinary(ConstStringRef deviceArg, OclocArgHelper *argHelper) {
    if (deviceArg == "*") {
        return argHelper->getAllSupportedDeviceConfigs();
    }
    std::vector<DeviceMapping> retVal;
    auto sets = CompilerOptions::tokenize(deviceArg, ',');
    if (sets[0].contains("-")) {
        auto range = CompilerOptions::tokenize(deviceArg, '-');
        if (range.size() > 2) {
            argHelper->printf("Invalid range : %s - should be from-to or -to or from-\n", sets[0].str().c_str());
            return {};
        }
        if (range.size() == 1) {
            bool rangeTo = ('-' == sets[0][0]);
            retVal = getProductConfigsForOpenRange(range[0], argHelper, rangeTo);

        } else {
            retVal = getProductConfigsForClosedRange(range[0], range[1], argHelper);
        }
    } else {
        retVal = getProductConfigsForSpecificTargets(sets, argHelper);
    }
    return retVal;
}

int buildFatBinaryForTarget(int retVal, std::vector<std::string> argsCopy, std::string pointerSize, Ar::ArEncoder &fatbinary, OfflineCompiler *pCompiler, OclocArgHelper *argHelper) {
    std::string product = hardwarePrefix[pCompiler->getHardwareInfo().platform.eProductFamily];
    auto stepping = pCompiler->getHardwareInfo().platform.usRevId;
    std::string deviceConfig = product + "." + std::to_string(stepping);

    if (retVal == 0) {
        retVal = buildWithSafetyGuard(pCompiler);
        std::string buildLog = pCompiler->getBuildLog();
        if (buildLog.empty() == false) {
            argHelper->printf("%s\n", buildLog.c_str());
        }
        if (retVal == 0) {
            if (!pCompiler->isQuiet())
                argHelper->printf("Build succeeded for : %s.\n", deviceConfig.c_str());
        } else {
            argHelper->printf("Build failed for : %s with error code: %d\n", deviceConfig.c_str(), retVal);
            argHelper->printf("Command was:");
            for (const auto &arg : argsCopy)
                argHelper->printf(" %s", arg.c_str());
            argHelper->printf("\n");
        }
    }
    if (retVal) {
        return retVal;
    }
    fatbinary.appendFileEntry(pointerSize + "." + deviceConfig, pCompiler->getPackedDeviceBinaryOutput());
    return retVal;
}

int buildFatBinary(const std::vector<std::string> &args, OclocArgHelper *argHelper) {
    std::string pointerSizeInBits = (sizeof(void *) == 4) ? "32" : "64";
    size_t deviceArgIndex = -1;
    std::string inputFileName = "";
    std::string outputFileName = "";
    std::string outputDirectory = "";

    std::vector<std::string> argsCopy(args);
    for (size_t argIndex = 1; argIndex < args.size(); argIndex++) {
        const auto &currArg = args[argIndex];
        const bool hasMoreArgs = (argIndex + 1 < args.size());
        if ((ConstStringRef("-device") == currArg) && hasMoreArgs) {
            deviceArgIndex = argIndex + 1;
            ++argIndex;
        } else if ((CompilerOptions::arch32bit == currArg) || (ConstStringRef("-32") == currArg)) {
            pointerSizeInBits = "32";
        } else if ((CompilerOptions::arch64bit == currArg) || (ConstStringRef("-64") == currArg)) {
            pointerSizeInBits = "64";
        } else if ((ConstStringRef("-file") == currArg) && hasMoreArgs) {
            inputFileName = args[argIndex + 1];
            ++argIndex;
        } else if ((ConstStringRef("-output") == currArg) && hasMoreArgs) {
            outputFileName = args[argIndex + 1];
            ++argIndex;
        } else if ((ConstStringRef("-out_dir") == currArg) && hasMoreArgs) {
            outputDirectory = args[argIndex + 1];
            ++argIndex;
        }
    }

    Ar::ArEncoder fatbinary(true);

    if (isDeviceWithPlatformAbbreviation(ConstStringRef(args[deviceArgIndex]), argHelper)) {
        std::vector<ConstStringRef> targetPlatforms;
        targetPlatforms = getTargetPlatformsForFatbinary(ConstStringRef(args[deviceArgIndex]), argHelper);
        if (targetPlatforms.empty()) {
            argHelper->printf("Failed to parse target devices from : %s\n", args[deviceArgIndex].c_str());
            return 1;
        }
        for (auto &targetPlatform : targetPlatforms) {
            int retVal = 0;
            argsCopy[deviceArgIndex] = targetPlatform.str();

            std::unique_ptr<OfflineCompiler> pCompiler{OfflineCompiler::create(argsCopy.size(), argsCopy, false, retVal, argHelper)};
            if (OclocErrorCode::SUCCESS != retVal) {
                argHelper->printf("Error! Couldn't create OfflineCompiler. Exiting.\n");
                return retVal;
            }

            retVal = buildFatBinaryForTarget(retVal, argsCopy, pointerSizeInBits, fatbinary, pCompiler.get(), argHelper);
            if (retVal) {
                return retVal;
            }
        }

    } else {
        std::vector<DeviceMapping> targetConfigs;
        targetConfigs = getTargetConfigsForFatbinary(ConstStringRef(args[deviceArgIndex]), argHelper);
        if (targetConfigs.empty()) {
            argHelper->printf("Failed to parse target devices from : %s\n", args[deviceArgIndex].c_str());
            return 1;
        }
        for (auto &targetConfig : targetConfigs) {
            int retVal = 0;

            argHelper->setFatbinary(true);
            argHelper->setDeviceInfoForFatbinaryTarget(targetConfig);
            std::unique_ptr<OfflineCompiler> pCompiler{OfflineCompiler::create(argsCopy.size(), argsCopy, false, retVal, argHelper)};
            if (OclocErrorCode::SUCCESS != retVal) {
                argHelper->printf("Error! Couldn't create OfflineCompiler. Exiting.\n");
                return retVal;
            }

            retVal = buildFatBinaryForTarget(retVal, argsCopy, pointerSizeInBits, fatbinary, pCompiler.get(), argHelper);
            if (retVal) {
                return retVal;
            }
        }
    }

    auto fatbinaryData = fatbinary.encode();
    std::string fatbinaryFileName = outputFileName;
    if (outputFileName.empty() && (false == inputFileName.empty())) {
        fatbinaryFileName = OfflineCompiler::getFileNameTrunk(inputFileName) + ".ar";
    }
    if (false == outputDirectory.empty()) {
        fatbinaryFileName = outputDirectory + "/" + outputFileName;
    }
    argHelper->saveOutput(fatbinaryFileName, fatbinaryData.data(), fatbinaryData.size());

    return 0;
}

} // namespace NEO
