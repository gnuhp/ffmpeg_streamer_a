package cz.havlena.ffmpeg.ui;

import java.io.IOException;

import com.media.ffmpeg.FFMpeg;
import com.media.ffmpeg.FFMpegException;
import com.media.ffmpeg.android.FFMpegMovieViewAndroid;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

public class FFMpegPlayerActivity extends Activity {
	private static final String 	TAG = "FFMpegPlayerActivity";
	//private static final String 	LICENSE = "This software uses libraries from the FFmpeg project under the LGPLv2.1";
	
	private FFMpegMovieViewAndroid 	mMovieView;
	//private WakeLock				mWakeLock;
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		setContentView(R.layout.ffmpeg_player_activity);
		mMovieView = (FFMpegMovieViewAndroid) findViewById(R.id.imageVideo);
		
		Intent i = getIntent();
		String filePath = i.getStringExtra(getResources().getString(R.string.input_file));
		if(filePath == null) {
			Log.d(TAG, "Not specified video file");
			finish();
		} else {
			//PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
		    //mWakeLock = pm.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK, TAG);

			try {
				FFMpeg ffmpeg = new FFMpeg();
				//mMovieView = ffmpeg.getMovieView(this);
				mMovieView.initVideoView(); 
				mMovieView.setVideoPath(filePath);
				
				
			} catch (FFMpegException e) {
				Log.d(TAG, "Error when initializing ffmpeg: " + e.getMessage());
				FFMpegMessageBox.show(this, e);
				finish();
			}
		}
	}
	
	protected void onStop()
	{
		super.onStop();
		
		
	}
	
	
	
	protected void onDestroy()
	{
		
		//Close the player properly here. 
		super.onDestroy();
	}
	
	
}
