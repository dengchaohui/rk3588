/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.systemui.car.systembar;

import android.content.Context;
import android.util.ArrayMap;
import android.util.Log;
import java.util.Objects;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;

import com.android.car.ui.FocusParkingView;
import com.android.systemui.R;
import com.android.systemui.car.statusicon.ui.QuickControlsEntryPointsController;
import com.android.systemui.car.statusicon.ui.ReadOnlyIconsController;
import com.android.systemui.dagger.SysUISingleton;
import com.android.systemui.flags.FeatureFlags;

import javax.inject.Inject;

/** A factory that creates and caches views for navigation bars. */
@SysUISingleton
public class CarSystemBarViewFactory {

    private static final String TAG = CarSystemBarViewFactory.class.getSimpleName();
    private static final ArrayMap<Type, Integer> sLayoutMap = setupLayoutMapping();

    private static ArrayMap<Type, Integer> setupLayoutMapping() {
        ArrayMap<Type, Integer> map = new ArrayMap<>();
        map.put(Type.TOP, R.layout.car_top_system_bar);
        map.put(Type.TOP_UNPROVISIONED, R.layout.car_top_system_bar_unprovisioned);
        map.put(Type.BOTTOM, R.layout.car_bottom_system_bar);
        map.put(Type.BOTTOM_UNPROVISIONED, R.layout.car_bottom_system_bar_unprovisioned);
        map.put(Type.LEFT, R.layout.car_left_system_bar);
        map.put(Type.LEFT_UNPROVISIONED, R.layout.car_left_system_bar_unprovisioned);
        map.put(Type.RIGHT, R.layout.car_right_system_bar);
        map.put(Type.RIGHT_UNPROVISIONED, R.layout.car_right_system_bar_unprovisioned);
        map.put(Type.BOTTOM2, R.layout.car_secondary_bottom_system_bar);
        map.put(Type.BOTTOM2_UNPROVISIONED, R.layout.car_bottom_system_bar_unprovisioned);
        return map;
    }

    private final Context mContext;
    private final ArrayMap<Integer /*Context,Type*/, CarSystemBarView> mCachedViewMap = new ArrayMap<>();
    private final ArrayMap<Integer /*Context,Type*/, ViewGroup> mCachedContainerMap = new ArrayMap<>();
    private final FeatureFlags mFeatureFlags;
    private final QuickControlsEntryPointsController mQuickControlsEntryPointsController;
    private final ReadOnlyIconsController mReadOnlyIconsController;

    /** Type of navigation bar to be created. */
    private enum Type {
        TOP,
        TOP_UNPROVISIONED,
        BOTTOM,
        BOTTOM_UNPROVISIONED,
        LEFT,
        LEFT_UNPROVISIONED,
        RIGHT,
        RIGHT_UNPROVISIONED,
        BOTTOM2,
        BOTTOM2_UNPROVISIONED,
    }

    @Inject
    public CarSystemBarViewFactory(
            Context context,
            FeatureFlags featureFlags,
            QuickControlsEntryPointsController quickControlsEntryPointsController,
            ReadOnlyIconsController readOnlyIconsController
    ) {
        mContext = context;
        mFeatureFlags = featureFlags;
        mQuickControlsEntryPointsController = quickControlsEntryPointsController;
        mReadOnlyIconsController = readOnlyIconsController;
    }

    /** Gets the top window. */
    public ViewGroup getTopWindow() {
        return getWindowCached(mContext, Type.TOP);
    }

    /** Gets the bottom window. */
    public ViewGroup getBottomWindow() {
        return getWindowCached(mContext, Type.BOTTOM);
    }

    /** Gets the left window. */
    public ViewGroup getLeftWindow() {
        return getWindowCached(mContext, Type.LEFT);
    }

    /** Gets the right window. */
    public ViewGroup getRightWindow() {
        return getWindowCached(mContext, Type.RIGHT);
    }

    /** Gets the bottom window. */
    public ViewGroup getSecondaryBottomWindow(Context context) {
        return getWindowCached(context, Type.BOTTOM2);
    }


    /** Gets the top bar. */
    public CarSystemBarView getTopBar(boolean isSetUp) {
        return getBar(mContext, isSetUp, Type.TOP, Type.TOP_UNPROVISIONED);
    }

    /** Gets the bottom bar. */
    public CarSystemBarView getBottomBar(boolean isSetUp) {
        return getBar(mContext, isSetUp, Type.BOTTOM, Type.BOTTOM_UNPROVISIONED);
    }

    /** Gets the left bar. */
    public CarSystemBarView getLeftBar(boolean isSetUp) {
        return getBar(mContext, isSetUp, Type.LEFT, Type.LEFT_UNPROVISIONED);
    }

    /** Gets the right bar. */
    public CarSystemBarView getRightBar(boolean isSetUp) {
        return getBar(mContext, isSetUp, Type.RIGHT, Type.RIGHT_UNPROVISIONED);
    }

    /** Gets the bottom bar. */
    public CarSystemBarView getSecondaryBottomBar(Context context, boolean isSetUp) {
        return getBar(context, isSetUp, Type.BOTTOM2, Type.BOTTOM2_UNPROVISIONED);
    }

    private ViewGroup getWindowCached(Context context, Type type) {
        int key = Objects.hash(context, type);
        if (mCachedContainerMap.containsKey(key)) {
            return mCachedContainerMap.get(key);
        }
        ViewGroup window = (ViewGroup) View.inflate(context,
                R.layout.navigation_bar_window, /* root= */ null);
        mCachedContainerMap.put(key, window);
        return mCachedContainerMap.get(key);
    }

    private CarSystemBarView getBar(Context context, boolean isSetUp, Type provisioned, Type unprovisioned) {
        CarSystemBarView view = getBarCached(context, isSetUp, provisioned, unprovisioned);

        if (view == null) {
            String name = isSetUp ? provisioned.name() : unprovisioned.name();
            Log.e(TAG, "CarStatusBar failed inflate for " + name);
            throw new RuntimeException(
                    "Unable to build " + name + " nav bar due to missing layout");
        }
        return view;
    }

    private CarSystemBarView getBarCached(Context context, boolean isSetUp, Type provisioned, Type unprovisioned) {
        Type type = isSetUp ? provisioned : unprovisioned;
        int key = Objects.hash(context, type);
        if (mCachedViewMap.containsKey(key)) {
            return mCachedViewMap.get(key);
        }

        @LayoutRes int barLayout = sLayoutMap.get(type);
        CarSystemBarView view = (CarSystemBarView) View.inflate(context, barLayout,
                /* root= */ null);

        view.setupHvacButton();
        view.setupQuickControlsEntryPoints(mQuickControlsEntryPointsController, isSetUp);
        view.setupReadOnlyIcons(mReadOnlyIconsController);

        // Include a FocusParkingView at the beginning. The rotary controller "parks" the focus here
        // when the user navigates to another window. This is also used to prevent wrap-around.
        view.addView(new FocusParkingView(context), 0);

        mCachedViewMap.put(key, view);
        return mCachedViewMap.get(key);
    }
}
