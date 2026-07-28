package com.example.myapplication.ui.main;

import android.os.Bundle;
import android.text.Editable;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.Observer;
import androidx.lifecycle.ViewModelProvider;

import com.example.myapplication.MainActivity;
import com.example.myapplication.R;
import com.example.myapplication.databinding.FragmentSplitDebugBinding;

/**
 * A placeholder fragment containing a simple view.
 */
public class PlaceholderFragment extends Fragment {

    private static final String ARG_SECTION_NUMBER = "section_number";
    private PageViewModel pageViewModel;
    private FragmentSplitDebugBinding binding;
    private EditText et_ab;
    private EditText et_bc;
    private EditText et_ds;

    private native void startCalib(int ab, int bc, int ds);

    public static PlaceholderFragment newInstance(int index) {
        PlaceholderFragment fragment = new PlaceholderFragment();
        Bundle bundle = new Bundle();
        bundle.putInt(ARG_SECTION_NUMBER, index);
        fragment.setArguments(bundle);
        return fragment;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        pageViewModel = new ViewModelProvider(this).get(PageViewModel.class);
        int index = 1;
        if (getArguments() != null) {
            index = getArguments().getInt(ARG_SECTION_NUMBER);
        }
        pageViewModel.setIndex(index);
    }

    @Override
    public View onCreateView(
            @NonNull LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {

        binding = FragmentSplitDebugBinding.inflate(inflater, container, false);
        View root = binding.getRoot();

        final TextView textView = binding.sectionLabel;
        pageViewModel.getText().observe(getViewLifecycleOwner(), new Observer<String>() {
            @Override
            public void onChanged(@Nullable String s) {
                textView.setText(s);
            }
        });

        final Button startCalibButton = (Button) binding.startCalib;
        assert startCalibButton != null;
        startCalibButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                int iab = Integer.parseInt(et_ab.getText().toString());
                int ibc = Integer.parseInt(et_bc.getText().toString());
                int ids = Integer.parseInt(et_ds.getText().toString());
                String debug = String.format("ab %d, bc %d ds %d",iab,ibc,ids);
                Log.d("MyApplication", "startCalib " + debug);
                startCalib(iab, ibc, ids);
            }
        });

        et_ab = binding.editText1;
        et_bc = binding.editText2;
        et_ds = binding.editText3;

        return root;
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        binding = null;
    }
}