/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/gmm_helper/gmm_lib.h"

#include "External/Common/GmmPageTableMgr.h"

#include <functional>
#include <memory>

namespace NEO {
class Gmm;
class LinearStream;
class GmmPageTableMngr {
  public:
    MOCKABLE_VIRTUAL ~GmmPageTableMngr();

    static GmmPageTableMngr *create(unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb);

    MOCKABLE_VIRTUAL void setCsrHandle(void *csrHandle);

    bool updateAuxTable(uint64_t gpuVa, Gmm *gmm, bool map);
    void initPageTableManagerRegisters();

    bool initialized = false;

  protected:
    GmmPageTableMngr() = default;

    MOCKABLE_VIRTUAL GMM_STATUS updateAuxTable(const GMM_DDI_UPDATEAUXTABLE *ddiUpdateAuxTable) {
        return pageTableManager->UpdateAuxTable(ddiUpdateAuxTable);
    }

    MOCKABLE_VIRTUAL GMM_STATUS initContextAuxTableRegister(HANDLE initialBBHandle, GMM_ENGINE_TYPE engineType) {
        return pageTableManager->InitContextAuxTableRegister(initialBBHandle, engineType);
    }

    GmmPageTableMngr(unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb);
    GMM_CLIENT_CONTEXT *clientContext = nullptr;
    GMM_PAGETABLE_MGR *pageTableManager = nullptr;
};
} // namespace NEO
