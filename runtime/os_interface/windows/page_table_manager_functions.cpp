/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/page_table_mngr.h"
#include "runtime/platform/platform.h"

#include "gmm_client_context.h"

namespace NEO {
GmmPageTableMngr::GmmPageTableMngr(unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb) {
    clientContext = platform()->peekGmmClientContext()->getHandle();
    pageTableManager = clientContext->CreatePageTblMgrObject(translationTableCb, translationTableFlags);
}

void GmmPageTableMngr::setCsrHandle(void *csrHandle) {
    pageTableManager->GmmSetCsrHandle(csrHandle);
}
} // namespace NEO
