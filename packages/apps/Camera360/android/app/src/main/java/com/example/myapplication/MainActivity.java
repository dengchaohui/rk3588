package com.example.myapplication;

import android.Manifest;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.graphics.Paint;
import android.os.Bundle;

import java.io.DataOutputStream;
import java.util.Timer;
import java.util.TimerTask;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import android.util.AttributeSet;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;

import androidx.appcompat.widget.Toolbar;
import androidx.core.app.ActivityCompat;
import androidx.navigation.ui.AppBarConfiguration;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.SystemClock;
import android.util.Log;
import com.example.myapplication.databinding.ActivityMainBinding;
import com.google.android.material.snackbar.Snackbar;

import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.*;

public class MainActivity extends AppCompatActivity{

    private AppBarConfiguration appBarConfiguration;
    private Timer fAnimationTimer;
    Bitmap fSkiaBitmap;
    private LinearLayout AdjLayout;
    private boolean machine_yaxun = false;
    private boolean algo_running = false;
    private static final boolean use_algo = true;
    private ActivityMainBinding binding;
    private SkiaDrawView my_draw_view;
    private int bitmp_w, bitmp_h;
    private int main_algo_type;
    private int front_state,back_state,left_state,right_state,lr_state,top_state;
    private native void drawIntoBitmap(Bitmap image, long elapsedTime);
    private native void startAlgo();
    private native void stopAlgo();
    private native void setDiaplaySize(int width, int height);
    private native void setAlgoType(int type);
    private native void onScreenTouch(int sx, int sy, int mx, int my);
    private native void setViewAdj(int code);
    private native void setViewAdjEn(int en);

    private Button btn_front;
    private Button btn_back;
    private Button btn_left;
    private Button btn_right;
    private Button btn_lr;
    private Button btn_top;
    private Button button_set;

    private int adj_mode = 0;
    private Button btn_adj1;
    private Button btn_adj2;
    private Button btn_adj3;
    private Button btn_adj4;
    private Button btn_adj5;
    private Button btn_adj6;
    private TextView adjtext;

/*
    public void setDebugPattern_java(int type){
        if(use_algo)
            setDebugPattern(type);
    }

    public void startCalib_java(){
        if(use_algo)
            startCalib();
    }
*/
    public void setAlgoType_java(int type){
        if(use_algo)
            setAlgoType(type);
    }
    public void ResetButton(){
        btn_front.setBackgroundColor(0xdcdcdc);
        btn_back.setBackgroundColor(0xdcdcdc);
        btn_left.setBackgroundColor(0xdcdcdc);
        btn_right.setBackgroundColor(0xdcdcdc);
        btn_lr.setBackgroundColor(0xdcdcdc);
        btn_top.setBackgroundColor(0xdcdcdc);
        button_set.setBackgroundColor(0xdcdcdc);
        btn_front.setText("前摄");
        btn_back.setText("后摄");
        btn_left.setText("左摄");
        btn_right.setText("右摄");
        btn_lr.setText("左右摄");
        btn_top.setText("环绕");
        button_set.setText("设置");
    }
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if(use_algo) {
            try {
                System.loadLibrary("camera360");
            } catch (UnsatisfiedLinkError e) {
                Log.d("MyApplication", "Link Error: " + e);
                return;
            }
            Log.d("MyApplication", "Load success. ");
        }

        //Log.d("MyApplication", getApplicationContext().getFilesDir().getPath());
        front_state = 0;
        back_state = 0;
        left_state = 0;
        right_state = 0;
        lr_state = 0;
        top_state = 0;


        if(machine_yaxun) {
            setContentView(R.layout.activity_main_yaxon);
        }else{

            setContentView(R.layout.activity_main);
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        }

        AdjLayout = (LinearLayout) findViewById(R.id.linearlay_3);
        AdjLayout.setVisibility(View.INVISIBLE);
        adjtext = findViewById(R.id.adjtext);

        Toolbar toolbar = findViewById(R.id.toolbar);
        //主标题
        toolbar.setTitle("Rockchip avm demo");
        //副标题
        //toolbar.setSubtitle("App design by isp");

        toolbar.inflateMenu(R.menu.menu_main);

        toolbar.setOnMenuItemClickListener(new Toolbar.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem menuItem) {
                switch (menuItem.getItemId()) {
                    case R.id.adj1:
                        adj_mode = 1;
                        adjtext.setText("平移");
                        setViewAdjEn(1);
                        AdjLayout.setVisibility(View.VISIBLE);
                        break;
                    case R.id.adj2:
                        adj_mode = 2;
                        adjtext.setText("旋转");
                        setViewAdjEn(2);
                        AdjLayout.setVisibility(View.VISIBLE);
                        break;
                    case R.id.adj3:
                        adj_mode = 3;
                        adjtext.setText("双视顶视");
                        setViewAdjEn(3);
                        AdjLayout.setVisibility(View.VISIBLE);
                        break;
                    case R.id.adj4:
                        adj_mode = 4;
                        adjtext.setText("车模比例");
                        setViewAdjEn(4);
                        AdjLayout.setVisibility(View.VISIBLE);
                        break;
                    case R.id.adj5:
                        adj_mode = 0;
                        setViewAdjEn(5);
                        AdjLayout.setVisibility(View.INVISIBLE);
                        break;
                }
                return false;
            }
        });

        LinearLayout layout=(LinearLayout) findViewById(R.id.linearlay_1);

        // Set generic layout parameters
        //LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.MATCH_PARENT);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.MATCH_PARENT);

        my_draw_view = new SkiaDrawView(this);
        layout.addView(my_draw_view, params); // Of course, this too

        //setContentView(my_draw_view);
        //PrepareCamera();

        btn_front = (Button) this.findViewById(R.id.button_front);
        btn_front.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //Snackbar.make(v, "start", Snackbar.LENGTH_LONG)
                //        .setAction("Action", null).show();

                ResetButton();
                btn_front.setBackgroundColor(0xFF4169E1);
                /*
                if(front_state == 0 || front_state == 4) {
                    btn_front.setText("INVALID");
                    front_state = 1;
                }else if(front_state == 1){
                    btn_front.setText("顶视\n前视远视野");
                    front_state = 2;
                }else
                */

                if(machine_yaxun == false) {
                    if (front_state == 0 || front_state == 4 || front_state == 2) {
                        btn_front.setText("顶视\n前视鱼眼");
                        front_state = 3;
                    } else if (front_state == 3) {
                        btn_front.setText("前视3D");
                        front_state = 4;
                    }

                    if (front_state != 1)
                        setAlgoType_java(front_state - 1);
                }else{
                    btn_front.setText("顶视\n前视鱼眼");
                    setAlgoType_java(2);
                }
            }
        });

        btn_back = (Button) this.findViewById(R.id.button_back);
        btn_back.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //Snackbar.make(v, "start", Snackbar.LENGTH_LONG)
                //        .setAction("Action", null).show();

                ResetButton();
                btn_back.setBackgroundColor(0xFF4169E1);
                /*
                if(back_state == 0 || back_state == 4) {
                    btn_back.setText("INVALID");
                    back_state = 1;
                }else if(back_state == 1){
                    btn_back.setText("顶视\n后视远视野");
                    back_state = 2;
                }else
                 */
                if(machine_yaxun == false) {
                    if (back_state == 0 || back_state == 2 || back_state == 4) {
                        btn_back.setText("顶视\n后视鱼眼");
                        back_state = 3;
                    } else if (back_state == 3) {
                        btn_back.setText("后视3D");
                        back_state = 4;
                    }

                    setAlgoType_java(back_state + 3);
                }else{
                    btn_back.setText("顶视\n后视鱼眼");
                    setAlgoType_java(6);
                }
            }
        });

        btn_left = (Button) this.findViewById(R.id.button_left);
        btn_left.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //Snackbar.make(v, "start", Snackbar.LENGTH_LONG)
                //        .setAction("Action", null).show();

                ResetButton();
                btn_left.setBackgroundColor(0xFF4169E1);
                if(machine_yaxun == false) {
                    if(left_state == 0 || left_state == 2) {
                        btn_left.setText("顶视\n左视2D");
                        left_state = 1;
                    }else if(left_state == 1){
                        btn_left.setText("顶视\n左视3D");
                        left_state = 2;
                    }

                     setAlgoType_java(left_state + 7);
                }else{
                    btn_left.setText("顶视\n左视2D");
                    setAlgoType_java(8);
                }
            }
        });

        btn_right = (Button) this.findViewById(R.id.button_right);
        btn_right.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //Snackbar.make(v, "start", Snackbar.LENGTH_LONG)
                //        .setAction("Action", null).show();

                ResetButton();
                btn_right.setBackgroundColor(0xFF4169E1);
                if(machine_yaxun == false) {
                    if(right_state == 0 || right_state == 2) {
                        btn_right.setText("顶视\n右视2D");
                        right_state = 1;
                    }else if(right_state == 1){
                        btn_right.setText("顶视\n右视3D");
                        right_state = 2;
                    }

                    setAlgoType_java(right_state + 10);
                }else{
                    btn_right.setText("顶视\n右视2D");
                    setAlgoType_java(10);
                }
            }
        });

        btn_lr = (Button) this.findViewById(R.id.button_lr);
        btn_lr.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //Snackbar.make(v, "start", Snackbar.LENGTH_LONG)
                //        .setAction("Action", null).show();

                ResetButton();
                btn_lr.setBackgroundColor(0xFF4169E1);
                if(machine_yaxun == false) {
                    if (lr_state == 0 || lr_state == 2) {
                        btn_lr.setText("顶视\n左右2D");
                        lr_state = 1;
                    } else if (lr_state == 1) {
                        btn_lr.setText("顶视\n左右3D");
                        lr_state = 2;
                    }
                    setAlgoType_java(13 + lr_state);
                }else{

                    btn_lr.setText("顶视\n左右2D");
                    setAlgoType_java(12);
                }
            }
        });

        btn_top = (Button) this.findViewById(R.id.button_top);
        btn_top.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                //Snackbar.make(v, "start", Snackbar.LENGTH_LONG)
                //        .setAction("Action", null).show();

                ResetButton();
                btn_top.setBackgroundColor(0xFF4169E1);
                    if (top_state == 0 || top_state == 2) {
                        btn_top.setText("环绕车3D");
                        top_state = 1;
                    } else if (top_state == 1) {
                        btn_top.setText("顶视\n环绕3D");
                        top_state = 2;
                    }

                    setAlgoType_java(15 + top_state);
            }
        });

        button_set = (Button) this.findViewById(R.id.button_setting);
        button_set.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
               // ResetButton();
               // Intent intent = new Intent();
               // intent.setClass(MainActivity.this, SettingActivity.class);
               // MainActivity.this.startActivity(intent);
               // MainActivity.this.finish();
            }
        });
        ResetButton();


        btn_adj1 = (Button) this.findViewById(R.id.button_adj1);
        btn_adj1.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(adj_mode == 1)
                    setViewAdj(1);
                if(adj_mode == 2)
                    setViewAdj(7);
                if(adj_mode == 3)
                    setViewAdj(13);
                if(adj_mode == 4)
                    setViewAdj(19);
            }
        });
        btn_adj2 = (Button) this.findViewById(R.id.button_adj2);
        btn_adj2.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(adj_mode == 1)
                    setViewAdj(2);
                if(adj_mode == 2)
                    setViewAdj(8);
                if(adj_mode == 3)
                    setViewAdj(14);
                if(adj_mode == 4)
                    setViewAdj(20);
            }
        });
        btn_adj3 = (Button) this.findViewById(R.id.button_adj3);
        btn_adj3.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(adj_mode == 1)
                    setViewAdj(3);
                if(adj_mode == 2)
                    setViewAdj(9);
                if(adj_mode == 3)
                    setViewAdj(15);
                if(adj_mode == 4)
                    setViewAdj(21);
            }
        });
        btn_adj4 = (Button) this.findViewById(R.id.button_adj4);
        btn_adj4.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(adj_mode == 1)
                    setViewAdj(4);
                if(adj_mode == 2)
                    setViewAdj(10);
                if(adj_mode == 3)
                    setViewAdj(16);
                if(adj_mode == 4)
                    setViewAdj(22);
            }
        });
        btn_adj5 = (Button) this.findViewById(R.id.button_adj5);
        btn_adj5.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(adj_mode == 1)
                    setViewAdj(5);
                if(adj_mode == 2)
                    setViewAdj(11);
                if(adj_mode == 3)
                    setViewAdj(17);
                if(adj_mode == 4)
                    setViewAdj(23);
            }
        });
        btn_adj6 = (Button) this.findViewById(R.id.button_adj6);
        btn_adj6.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(adj_mode == 1)
                    setViewAdj(6);
                if(adj_mode == 2)
                    setViewAdj(12);
                if(adj_mode == 3)
                    setViewAdj(18);
                if(adj_mode == 4)
                    setViewAdj(24);
            }
        });
        // Set a timer that will periodically request an update of the SkiaDrawView
        fAnimationTimer = new Timer();
        fAnimationTimer.schedule(new TimerTask() {
            public void run()
            {
                // This will request an update of the SkiaDrawView, even from other threads
                my_draw_view.postInvalidate();
            }
        }, 0, 5); // 0 means no delay before the timer starts; 5 means repeat every 5 milliseconds

        //String debug = null;
        //debug = String.format("w %d, h %d",bitmp_w,bitmp_h);
        //Snackbar.make(binding.getRoot(), debug, Snackbar.LENGTH_LONG)
        //        .setAction("Action", null).show();
    }


    @Override
    public void onBackPressed() {
        super.onBackPressed();
        finish();
    }
    @Override
    protected void onDestroy() {
        Log.d("MyApplication", "onDestroy: ");
        fAnimationTimer.cancel();
        algo_running = false;

        fSkiaBitmap.recycle();
        if(use_algo)
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
            if(use_algo) {
                if(algo_running == false){
                    algo_running = true;
                    startAlgo();
                    setDiaplaySize(bitmp_w, bitmp_h);
                }
            }
        }

        @Override
        protected void onDraw(Canvas canvas) {
            // Call into our C++ code that renders to the bitmap using Skia
            if(use_algo) {
                if(algo_running){
                    drawIntoBitmap(fSkiaBitmap, SystemClock.elapsedRealtime());
                    // Present the bitmap on the screen
                    canvas.drawBitmap(fSkiaBitmap, 0, 0, null);

                }
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

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN://手指按下
                    //按下的时候获取手指触摸的坐标
                    mX = (int) event.getRawX();
                    mY = (int) event.getRawY();
                    break;
                case MotionEvent.ACTION_MOVE://手指滑动
                    moveX = (int) event.getRawX();
                    moveY = (int) event.getRawY();

                    if(use_algo)
                        onScreenTouch(mX,mY,moveX,moveY);
                    break;
                case MotionEvent.ACTION_UP://手指松开
                    break;
            }
            return true;
        }
    }
    /*
    public void RunAsRoot(String[] cmds){
        try{
            Process p;
            p = Runtime.getRuntime().exec("su");
            DataOutputStream os = new DataOutputStream(p.getOutputStream());
            for (String tmpCmd : cmds) {
                os.writeBytes(tmpCmd+"\n");
            }
            os.writeBytes("exit\n");
            os.flush();
        }catch (IOException e) {
            Log.d("MyApplication", "IOException: " + e);
            return;
        }

    }
    public void PrepareCamera(){
        String[] cmds = {"chmod -R 777 /dev/video*",
                "echo 0 > /sys/fs/selinux/enforce"
        };
        RunAsRoot(cmds);
    }
     */
}

