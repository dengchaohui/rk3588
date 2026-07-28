/*
 * Copyright (c) 2020 Rockchip Electronics Co., Ltd
 */
package com.android.server;

import android.content.Context;
import android.os.IRKBoxManagementService;
import android.util.Log;

/**
 * @hide
 */
class RKBoxManagementService extends IRKBoxManagementService.Stub {
    private static final String TAG = "RKBoxManagementService";

    private static native int nativeInit();

    /**
     * Binder context for this service
     */
    private Context mContext;

    public RKBoxManagementService(Context context) {
        mContext = context;
        Log.d(TAG, "Init RKBox service!");
        nativeInit();
    }

}
