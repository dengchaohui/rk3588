package com.example.myapplication;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Context;
import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.widget.LinearLayout;

import java.util.Timer;
import java.util.TimerTask;

public class CalibActivity extends AppCompatActivity {
    private Timer fAnimationTimer;
    Bitmap fSkiaBitmap;
    private CalibActivity.SkiaDrawView my_draw_view;
    private int bitmp_w, bitmp_h;
    private boolean algo_running = false;
    private native void drawIntoBitmap(Bitmap image, long elapsedTime);
    private native void startAlgo();
    private native void stopAlgo();
    private native void setDiaplaySize(int width, int height);
    private native void setAlgoType(int type);
    private native void onScreenTouch(int sx, int sy, int mx, int my);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        System.loadLibrary("camera360");

        setContentView(R.layout.activity_calib);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        LinearLayout layout=(LinearLayout) findViewById(R.id.linearlay_1);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.MATCH_PARENT);

        my_draw_view = new CalibActivity.SkiaDrawView(this);
        layout.addView(my_draw_view, params); // Of course, this too

        fAnimationTimer = new Timer();
        fAnimationTimer.schedule(new TimerTask() {
            public void run()
            {
                // This will request an update of the SkiaDrawView, even from other threads
                my_draw_view.postInvalidate();
            }
        }, 0, 5); // 0 means no delay before the timer starts; 5 means repeat every 5 milliseconds

    }

    @Override
    public void onBackPressed() {
        super.onBackPressed();
        finish();
    }
    @Override
    protected void onDestroy() {
       // Log.d("MyApplication", "onDestroy: ");
        fAnimationTimer.cancel();
        algo_running = false;

        fSkiaBitmap.recycle();
        stopAlgo();
        super.onDestroy();
    }

    public class SkiaDrawView extends View {

        public SkiaDrawView(Context ctx) {
            super(ctx);
        }
        public SkiaDrawView(Context context, AttributeSet attrs){
            super(context, attrs);
        }

        private int mX,mY,moveX,moveY;

        @Override
        protected void onSizeChanged(int w, int h, int oldw, int oldh)
        {
            // Create a bitmap for skia to draw into
            bitmp_w = w;
            bitmp_h = h;

            fSkiaBitmap = Bitmap.createBitmap(bitmp_w, bitmp_h, Bitmap.Config.RGB_565);
            String debug = null;
            debug = String.format("onSizeChanged w %d, h %d oldw %d oldh %d",w,h,oldw,oldh);
            Log.d("MyApplication", debug);

            if(algo_running == false){
                algo_running = true;
                startAlgo();
                setDiaplaySize(bitmp_w, bitmp_h);
            }
        }

        @Override
        protected void onDraw(Canvas canvas) {
            // Call into our C++ code that renders to the bitmap using Skia

            if(algo_running){
                drawIntoBitmap(fSkiaBitmap, SystemClock.elapsedRealtime());
                // Present the bitmap on the screen
                canvas.drawBitmap(fSkiaBitmap, 0, 0, null);
            }
/*
            Paint mPaint = new Paint();
            //设置画笔颜色
            mPaint.setColor(0xffFF4500);
            //设置画笔为空心     如果将这里改为Style.STROKE  这个图中的实线圆柱体就变成了空心的圆柱体
            mPaint.setStyle(Paint.Style.FILL);
            //绘制直线
            //canvas.drawColor(0xdcdcdcff);
            canvas.drawLine(mX, mY, moveX, moveY, mPaint);
*/
        }

    }
}