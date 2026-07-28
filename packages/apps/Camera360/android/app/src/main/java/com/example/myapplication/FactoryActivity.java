package com.example.myapplication;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.ListView;

import java.util.ArrayList;
import java.util.List;

public class FactoryActivity extends AppCompatActivity {
    private ListView listview;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        LinearLayout layout = new LinearLayout(this);
        // Define the LinearLayout's characteristics
        layout.setGravity(Gravity.TOP);
        layout.setOrientation(LinearLayout.VERTICAL);

        // Set generic layout parameters
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);

        listview = new ListView(this);
        List<String> listdata = new ArrayList<String>();
        listdata.add("拼接调试1");
        listdata.add("拼接调试2");
        listdata.add("视图调整");

        listdata.add("清除拼接参数");
        listdata.add("视图调整恢复默认");
        ArrayAdapter<String> arrayAdapter = new ArrayAdapter<String>(this,R.layout.list_item,listdata);
        listview.setAdapter(arrayAdapter);

        layout.addView(listview, params); // Modify this
        setContentView(layout);

        listview.setOnItemClickListener(new AdapterView.OnItemClickListener(){
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                if(listdata.get(position) == "拼接调试1"){
                    Intent intent = new Intent();
                    intent.setClass(FactoryActivity.this, DebugPreviewActivity.class);
                    FactoryActivity.this.startActivity(intent);
                    FactoryActivity.this.finish();
                }
                if(listdata.get(position) == "拼接调试2"){
                    Intent intent = new Intent();
                    intent.setClass(FactoryActivity.this, SplitDebugActivity.class);
                    FactoryActivity.this.startActivity(intent);
                    FactoryActivity.this.finish();
                }
                if(listdata.get(position) == "视图调整"){
                    Intent intent = new Intent();
                    intent.setClass(FactoryActivity.this, CalibActivity.class);
                    FactoryActivity.this.startActivity(intent);
                    FactoryActivity.this.finish();
                }

            }
        });


    }
}