package com.example.myapplication;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.ImageFormat;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraMetadata;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.TotalCaptureResult;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.Image;
import android.media.ImageReader;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;
import android.util.Log;
import android.util.Size;
import android.util.SparseIntArray;
import android.view.Surface;
import android.view.TextureView;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class DebugPreviewActivity extends AppCompatActivity {
    private String [] cameraId;
    private Button takePictureButton;
    private Button nextButton;
    private int saved_times;
    protected CameraDevice [] cameraDevice;
    protected CameraCaptureSession [] cameraCaptureSessions;
    protected CaptureRequest []  captureRequest;
    protected CaptureRequest.Builder [] captureRequestBuilder;
    private Size imageDimension;
    private TextureView [] textureView;
    private static final SparseIntArray ORIENTATIONS = new SparseIntArray();

    static {
        ORIENTATIONS.append(Surface.ROTATION_0, 90);
        ORIENTATIONS.append(Surface.ROTATION_90, 0);
        ORIENTATIONS.append(Surface.ROTATION_180, 270);
        ORIENTATIONS.append(Surface.ROTATION_270, 180);
    }

    private Handler [] mBackgroundHandler;
    private HandlerThread [] mBackgroundThread;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_debug_preview);

        textureView = new TextureView[4];
        cameraId = new String[4];
        cameraDevice = new CameraDevice[4];

        mBackgroundThread = new HandlerThread[4];
        mBackgroundHandler = new Handler[4];
        captureRequestBuilder = new CaptureRequest.Builder[4];
        captureRequest = new CaptureRequest[4];
        cameraCaptureSessions = new CameraCaptureSession[4];
        saved_times = 0;

        mytextureListener textureListener0 = new mytextureListener();
        mytextureListener textureListener1 = new mytextureListener();
        mytextureListener textureListener2 = new mytextureListener();
        mytextureListener textureListener3 = new mytextureListener();
        textureListener0.set_index(0);
        textureListener1.set_index(1);
        textureListener2.set_index(2);
        textureListener3.set_index(3);


        textureView[0] = (TextureView) findViewById(R.id.texture1);
        assert textureView[0] != null;
        textureView[0].setSurfaceTextureListener(textureListener0);

        textureView[1] = (TextureView) findViewById(R.id.texture2);
        assert textureView[1] != null;
        textureView[1].setSurfaceTextureListener(textureListener1);

        textureView[2] = (TextureView) findViewById(R.id.texture3);
        assert textureView[2] != null;
        textureView[2].setSurfaceTextureListener(textureListener2);

        textureView[3] = (TextureView) findViewById(R.id.texture4);
        assert textureView[3] != null;
        textureView[3].setSurfaceTextureListener(textureListener3);

        takePictureButton = (Button) findViewById(R.id.btn_takepicture);
        assert takePictureButton != null;
        takePictureButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                int i;
                for(i=0; i<4; i++){
                    takePicture(i);
                }
                saved_times ++;
            }
        });
        nextButton = (Button) findViewById(R.id.btn_next);
        assert nextButton != null;
        nextButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent();
                intent.setClass(DebugPreviewActivity.this, SplitDebugActivity.class);
                DebugPreviewActivity.this.startActivity(intent);
                DebugPreviewActivity.this.finish();
            }
        });

    }
    private class  mytextureListener implements TextureView.SurfaceTextureListener {
        private int camera_index;
        public void set_index(int i){
            camera_index = i;
        }
        @Override
        public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {

            //open your camera here
            openCamera(camera_index);
        }
        @Override
        public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
        }

        @Override
        public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
            return false;
        }

        @Override
        public void onSurfaceTextureUpdated(SurfaceTexture surface) {
        }

    }

    protected void takePicture(int i) {
        if (null == cameraDevice[i]) {
            Log.e("MyApplication", "cameraDevice is null");
            return;
        }
        CameraManager manager = (CameraManager) getSystemService(Context.CAMERA_SERVICE);
        try {
            CameraCharacteristics characteristics = manager.getCameraCharacteristics(cameraDevice[i].getId());
            Size[] jpegSizes = null;
            if (characteristics != null) {
                jpegSizes = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP).getOutputSizes(ImageFormat.JPEG);
            }
            int width = 640;
            int height = 480;
            if (jpegSizes != null && 0 < jpegSizes.length) {
                width = jpegSizes[0].getWidth();
                height = jpegSizes[0].getHeight();
            }

            ImageReader reader = ImageReader.newInstance(width, height, ImageFormat.JPEG, 4);
            List<Surface> outputSurfaces = new ArrayList<Surface>(2);
            outputSurfaces.add(reader.getSurface());
            outputSurfaces.add(new Surface(textureView[i].getSurfaceTexture()));
            final CaptureRequest.Builder captureBuilder = cameraDevice[i].createCaptureRequest(CameraDevice.TEMPLATE_STILL_CAPTURE);
            captureBuilder.addTarget(reader.getSurface());
            captureBuilder.set(CaptureRequest.CONTROL_MODE, CameraMetadata.CONTROL_MODE_AUTO);
            // Orientation
            int rotation = getWindowManager().getDefaultDisplay().getRotation();
            captureBuilder.set(CaptureRequest.JPEG_ORIENTATION, ORIENTATIONS.get(rotation));


            String filename = String.format("%s/pic_%d_%d.jpg",getApplicationContext().getFilesDir().getPath(),saved_times,i);
            //String filename = String.format("%s/pic_%d_%d.jpg","/data/App",saved_times,i);
            final File file = new File(filename);
            ImageReader.OnImageAvailableListener readerListener = new ImageReader.OnImageAvailableListener() {
                @Override
                public void onImageAvailable(ImageReader reader) {
                    Image image = null;
                    try {
                        image = reader.acquireLatestImage();
                        ByteBuffer buffer = image.getPlanes()[0].getBuffer();
                        byte[] bytes = new byte[buffer.capacity()];
                        buffer.get(bytes);
                        save(bytes);
                    } catch (FileNotFoundException e) {
                        e.printStackTrace();
                    } catch (IOException e) {
                        e.printStackTrace();
                    } finally {
                        if (image != null) {
                            image.close();
                        }
                    }
                }

                private void save(byte[] bytes) throws IOException {
                    OutputStream output = null;
                    try {
                        output = new FileOutputStream(file);
                        output.write(bytes);
                    } finally {
                        if (null != output) {
                            output.close();
                        }
                    }
                }
            };
            reader.setOnImageAvailableListener(readerListener, mBackgroundHandler[i]);
            final CameraCaptureSession.CaptureCallback captureListener = new CameraCaptureSession.CaptureCallback() {
                @Override
                public void onCaptureCompleted(CameraCaptureSession session, CaptureRequest request, TotalCaptureResult result) {
                    super.onCaptureCompleted(session, request, result);

                    Toast.makeText(DebugPreviewActivity.this, "Saved:" + file, Toast.LENGTH_SHORT).show();
                    Log.d("MyApplication", "Saved" + file);
                    createCameraPreview(i);
                }
            };
            cameraDevice[i].createCaptureSession(outputSurfaces, new CameraCaptureSession.StateCallback() {
                @Override
                public void onConfigured(CameraCaptureSession session) {
                    try {
                        session.capture(captureBuilder.build(), captureListener, mBackgroundHandler[i]);
                    } catch (CameraAccessException e) {
                        e.printStackTrace();
                    }
                }

                @Override
                public void onConfigureFailed(CameraCaptureSession session) {
                }
            }, mBackgroundHandler[i]);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }
/*
    TextureView.SurfaceTextureListener textureListener0 = new TextureView.SurfaceTextureListener() {
        @Override
        public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {

            //open your camera here
            openCamera(0);
        }

        @Override
        public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
            // Transform you image captured size according to the surface width and height
        }

        @Override
        public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
            return false;
        }

        @Override
        public void onSurfaceTextureUpdated(SurfaceTexture surface) {
        }
    };
    TextureView.SurfaceTextureListener textureListener1 = new TextureView.SurfaceTextureListener() {
        @Override
        public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {

            //open your camera here
            openCamera(1);
        }

        @Override
        public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
            // Transform you image captured size according to the surface width and height
        }

        @Override
        public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
            return false;
        }

        @Override
        public void onSurfaceTextureUpdated(SurfaceTexture surface) {
        }
    };
    
 */
    private final CameraDevice.StateCallback stateCallback = new CameraDevice.StateCallback() {
        @Override
        public void onOpened(CameraDevice camera) {
            //This is called when the camera is open
            Log.d("MyApplication", "onOpened " + camera.getId());

            int i;
            for(i=0; i<4; i++){
                Log.d("MyApplication","i=" + i +" cameraId[i] = " + cameraId[i]);

                if(camera.getId().equals(cameraId[i]))
                    break;
            }

            cameraDevice[i] = camera;
            createCameraPreview(i);
        }

        @Override
        public void onDisconnected(CameraDevice camera) {

            int i;
            for(i=0; i<4; i++){
                if(camera.getId() == cameraId[i])
                    break;
            }
            cameraDevice[i].close();
        }

        @Override
        public void onError(CameraDevice camera, int error) {
            int i;
            for(i=0; i<4; i++){
                if(camera.getId() == cameraId[i])
                    break;
            }
            cameraDevice[i].close();
            cameraDevice[i] = null;
        }
    };

    protected void startBackgroundThread(int i) {
        mBackgroundThread[i] = new HandlerThread("Camera Background");
        mBackgroundThread[i].start();
        mBackgroundHandler[i] = new Handler(mBackgroundThread[i].getLooper());
    }

    protected void stopBackgroundThread(int i) {
        mBackgroundThread[i].quitSafely();
        try {
            mBackgroundThread[i].join();
            mBackgroundThread[i] = null;
            mBackgroundHandler[i] = null;
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    protected void createCameraPreview(int i) {
        try {
            SurfaceTexture texture = textureView[i].getSurfaceTexture();
            assert texture != null;
            texture.setDefaultBufferSize(imageDimension.getWidth(), imageDimension.getHeight());
            Surface surface = new Surface(texture);
            captureRequestBuilder[i] = cameraDevice[i].createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
            captureRequestBuilder[i].addTarget(surface);
            cameraDevice[i].createCaptureSession(Arrays.asList(surface), new CameraCaptureSession.StateCallback() {
                @Override
                public void onConfigured(@NonNull CameraCaptureSession cameraCaptureSession) {
                    //The camera is already closed
                    if (null == cameraDevice[i]) {
                        return;
                    }
                    // When the session is ready, we start displaying the preview.
                    cameraCaptureSessions[i] = cameraCaptureSession;
                    updatePreview(i);
                }

                @Override
                public void onConfigureFailed(@NonNull CameraCaptureSession cameraCaptureSession) {
                    Toast.makeText(DebugPreviewActivity.this, "Configuration change", Toast.LENGTH_SHORT).show();
                }
            }, null);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    private void openCamera(int i) {
        CameraManager manager = (CameraManager) getSystemService(Context.CAMERA_SERVICE);

        try {
            cameraId[i] = manager.getCameraIdList()[i];

            CameraCharacteristics characteristics = manager.getCameraCharacteristics(cameraId[i]);
            StreamConfigurationMap map = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            assert map != null;
            imageDimension = map.getOutputSizes(SurfaceTexture.class)[0];

            manager.openCamera(cameraId[i], stateCallback, null);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    protected void updatePreview(int i) {
        if (null == cameraDevice[i]) {
            Log.d("MyApplication", "updatePreview error, return");
        }
        captureRequestBuilder[i].set(CaptureRequest.CONTROL_MODE, CameraMetadata.CONTROL_MODE_AUTO);
        try {
            cameraCaptureSessions[i].setRepeatingRequest(captureRequestBuilder[i].build(), null, mBackgroundHandler[i]);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    private void closeCamera(int i) {
        if (null != cameraDevice[i]) {
            cameraDevice[i].close();
            cameraDevice[i] = null;
        }

    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.d("MyApplication", "onResume");

        startBackgroundThread(0);
        startBackgroundThread(1);
        startBackgroundThread(2);
        startBackgroundThread(3);
        /*
        if (textureView[0].isAvailable()) {
            openCamera(0);
        } else {
            textureView[0].setSurfaceTextureListener(textureListener0);
        }

        startBackgroundThread(1);
        if (textureView[1].isAvailable()) {
            openCamera(1);
        } else {
            textureView[1].setSurfaceTextureListener(textureListener1);
        }
        */

    }

    @Override
    protected void onPause() {
        Log.d("MyApplication", "onPause");
        //closeCamera(0);
        stopBackgroundThread(0);
        stopBackgroundThread(1);
        stopBackgroundThread(2);
        stopBackgroundThread(3);
        super.onPause();
    }
}