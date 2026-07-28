package com.example.myapplication;

import android.content.Intent;
import android.os.Bundle;
import androidx.appcompat.app.AppCompatActivity;

import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

public class SettingActivity extends AppCompatActivity {

    private ListView listview;
    private List<String> listdata;
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
        listdata = new ArrayList<String>();
        listdata.add("环视设置");
        listdata.add("行车记录");
        listdata.add("工程模式");
        listdata.add("U盘升级");
        listdata.add("四分屏");
        ArrayAdapter<String> arrayAdapter = new ArrayAdapter<String>(this,R.layout.list_item,listdata);
        listview.setAdapter(arrayAdapter);

        layout.addView(listview, params); // Modify this
        setContentView(layout);

        listview.setOnItemClickListener(new AdapterView.OnItemClickListener(){
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                if(listdata.get(position) == "工程模式"){
                    Intent intent = new Intent();
                    intent.setClass(SettingActivity.this, FactoryActivity.class);
                    SettingActivity.this.startActivity(intent);
                    //SettingActivity.this.finish();
                }
            }
        });
    }
}