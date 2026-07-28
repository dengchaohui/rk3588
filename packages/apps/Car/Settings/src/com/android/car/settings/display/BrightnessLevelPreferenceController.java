/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.car.settings.display;

import static android.os.UserManager.DISALLOW_CONFIG_BRIGHTNESS;

import static com.android.car.settings.enterprise.ActionDisabledByAdminDialogFragment.DISABLED_BY_ADMIN_CONFIRM_DIALOG_TAG;
import static com.android.car.settings.enterprise.EnterpriseUtils.hasUserRestrictionByDpm;
import static com.android.car.settings.enterprise.EnterpriseUtils.hasUserRestrictionByUm;
import static com.android.settingslib.display.BrightnessUtils.GAMMA_SPACE_MAX;
import static com.android.settingslib.display.BrightnessUtils.convertGammaToLinearFloat;
import static com.android.settingslib.display.BrightnessUtils.convertLinearToGammaFloat;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.database.ContentObserver;
import android.hardware.display.BrightnessInfo;
import android.hardware.display.DisplayManager;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.os.UserHandle;
import android.provider.Settings;
import android.util.MathUtils;
import android.widget.Toast;

import com.android.car.settings.R;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.Logger;
import com.android.car.settings.common.PreferenceController;
import com.android.car.settings.common.SeekBarPreference;
import com.android.car.settings.enterprise.EnterpriseUtils;

import com.android.internal.display.BrightnessSynchronizer;

/** Business logic for changing the brightness of the display. */
public class BrightnessLevelPreferenceController extends PreferenceController<SeekBarPreference> {

    private static final Logger LOG = new Logger(BrightnessLevelPreferenceController.class);
    private static final Uri BRIGHTNESS_URI = Settings.System.getUriFor(
            Settings.System.SCREEN_BRIGHTNESS);
    private float mMaximumBacklight;
    private float mMinimumBacklight;
    private DisplayManager mDisplayManager;

    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private final ContentObserver mBrightnessObserver = new ContentObserver(mHandler) {
        @Override
        public void onChange(boolean selfChange) {
            refreshUi();
        }
    };

    public BrightnessLevelPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);

        PowerManager powerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        mMaximumBacklight = PowerManager.BRIGHTNESS_MAX;
        mMinimumBacklight = PowerManager.BRIGHTNESS_MIN;

    }

    @Override
    protected Class<SeekBarPreference> getPreferenceType() {
        return SeekBarPreference.class;
    }

    @Override
    protected void onCreateInternal() {
        super.onCreateInternal();
        mDisplayManager = getContext().getSystemService(DisplayManager.class);
        setClickableWhileDisabled(getPreference(), /* clickable= */ true, p -> {
            if (hasUserRestrictionByDpm(getContext(), DISALLOW_CONFIG_BRIGHTNESS)) {
                showActionDisabledByAdminDialog();
            } else {
                Toast.makeText(getContext(),
                        getContext().getString(R.string.action_unavailable),
                        Toast.LENGTH_LONG).show();
            }
        });
    }

    @Override
    protected void onStartInternal() {
        super.onStartInternal();
        getContext().getContentResolver().registerContentObserver(BRIGHTNESS_URI,
                /* notifyForDescendants= */ false, mBrightnessObserver);
    }

    @Override
    protected void onStopInternal() {
        super.onStopInternal();
        getContext().getContentResolver().unregisterContentObserver(mBrightnessObserver);
    }

    @Override
    protected void updateState(SeekBarPreference preference) {
        preference.setMax(GAMMA_SPACE_MAX);
        preference.setValue(getSeekbarValue());
        preference.setContinuousUpdate(true);
    }

    @Override
    protected boolean handlePreferenceChanged(SeekBarPreference preference, Object newValue) {
        int gamma = (Integer) newValue;
        float linear = MathUtils.min(convertGammaToLinearFloat(gamma, mMinimumBacklight, mMaximumBacklight), mMaximumBacklight);
        int metric = BrightnessSynchronizer.brightnessFloatToInt(linear);
        LOG.d("linear = " + linear + ", gamma = " + gamma + ", metric = " + metric);
        mDisplayManager.setTemporaryBrightness(getContext().getDisplay().getDisplayId(), linear);
        mDisplayManager.setBrightness(getContext().getDisplay().getDisplayId(), linear);
        return true;
    }

    private int getSeekbarValue() {
        int gamma = GAMMA_SPACE_MAX;
        BrightnessInfo info = getContext().getDisplay().getBrightnessInfo();
        if (info == null) {
            LOG.e("info == null");
            return gamma;
        }
        mMaximumBacklight = info.brightnessMaximum;
        mMinimumBacklight = info.brightnessMinimum;
        float brightness = info.brightness;

        float min = mMinimumBacklight;
        float max = mMaximumBacklight;
        if (BrightnessSynchronizer.floatEquals(brightness,
                convertGammaToLinearFloat(getPreference().getValue(), min, max))) {
            // If the value in the slider is equal to the value on the current brightness
            // then the slider does not need to animate, since the brightness will not change.
            gamma = convertLinearToGammaFloat(brightness, min, max);
            LOG.e("convertLinearToGammaFloat mMaximumBacklight = " + mMaximumBacklight + ", mMinimumBacklight = " + mMinimumBacklight
                    + ", brightness = " + info.brightness + ", gamma = " + gamma + ", getValue = " + getPreference()
                            .getValue());
            return gamma;
        }
        gamma = convertLinearToGammaFloat(brightness, min, max);
        LOG.e("mMaximumBacklight = " + mMaximumBacklight + ", mMinimumBacklight = " + mMinimumBacklight
                + ", brightness = " + info.brightness + ", gamma = " + gamma);
        return gamma;
    }

    @Override
    public int getAvailabilityStatus() {
        if (hasUserRestrictionByUm(getContext(), DISALLOW_CONFIG_BRIGHTNESS)
                || hasUserRestrictionByDpm(getContext(), DISALLOW_CONFIG_BRIGHTNESS)) {
            return AVAILABLE_FOR_VIEWING;
        }
        return AVAILABLE;
    }

    private void showActionDisabledByAdminDialog() {
        getFragmentController().showDialog(
                EnterpriseUtils.getActionDisabledByAdminDialog(getContext(),
                        DISALLOW_CONFIG_BRIGHTNESS),
                DISABLED_BY_ADMIN_CONFIRM_DIALOG_TAG);
    }

}
