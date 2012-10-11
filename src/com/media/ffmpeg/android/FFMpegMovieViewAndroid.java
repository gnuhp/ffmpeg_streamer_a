package com.media.ffmpeg.android;

import java.io.IOException;

import com.media.ffmpeg.FFMpegPlayer;

import android.content.Context;
import android.util.AttributeSet;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.MediaController;
import android.widget.MediaController.MediaPlayerControl;

public class FFMpegMovieViewAndroid extends SurfaceView {
	private static final String 	TAG = "FFMpegMovieViewAndroid"; 
	
	private FFMpegPlayer			mPlayer;
	private MediaController			mMediaController;
	
	private String filePath ; 
	
	public FFMpegMovieViewAndroid(Context context) {
        super(context);
        SurfaceHolder surfHolder = getHolder();
    	surfHolder.addCallback(mSHCallback);
    }
    
    public FFMpegMovieViewAndroid(Context context, AttributeSet attrs) {
        super(context, attrs);
        SurfaceHolder surfHolder = getHolder();
    	surfHolder.addCallback(mSHCallback);
    }
    
    public FFMpegMovieViewAndroid(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        SurfaceHolder surfHolder = getHolder();
    	surfHolder.addCallback(mSHCallback);
    }
    
    public  void initVideoView() {
    	Log.d(TAG, "creating FFMpegPlayer.. ");
    	mPlayer = new FFMpegPlayer();
    }
    
    private void attachMediaController() {
    	mMediaController = new MediaController(getContext());
        View anchorView = this.getParent() instanceof View ?
                    (View)this.getParent() : this;
        mMediaController.setMediaPlayer(mMediaPlayerControl);
        mMediaController.setAnchorView(anchorView);
        mMediaController.setEnabled(true);
    }
    
    public void setVideoPath(String filePath) 
    {
    	this.filePath = filePath; 
		
	}
    
    /**
     * initzialize player
     */
    private boolean openVideo(SurfaceHolder surfHolder) {
    	try {
        	mPlayer.setDataSource(filePath);
        	//ORDER is important -- Set display before prepare()!!!
    		mPlayer.setDisplay(surfHolder);
			mPlayer.prepare();
			return true;
			
		} catch (IllegalStateException e) {
			Log.e(TAG, "Couldn't prepare player: " + e.getMessage());
		} catch (IOException e) {
			Log.e(TAG, "IO Exception Couldn't prepare player: " + e.getMessage());
		}
    	
    	
    	return false;
    	
    }
    
    private void startVideo() {
    	attachMediaController();
    	mPlayer.start();
    }
    
    private void release() {
    	Log.d(TAG, "releasing player");
    	
    	mPlayer.suspend();
		mPlayer.stop();
		mPlayer.release();
		Log.d(TAG, "released");
    }
    
    public boolean onTouchEvent(android.view.MotionEvent event) {
    	if(mMediaController != null && !mMediaController.isShowing()) {
			mMediaController.show(3000);
		}
		return true;
    }
    
    SurfaceHolder.Callback mSHCallback = new SurfaceHolder.Callback() {
        public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
           
        }

        public void surfaceCreated(SurfaceHolder holder) {
            if (openVideo(holder))
            {
            	startVideo();
            }
            else
            {
            	mPlayer = null; 
            }
        }

        public void surfaceDestroyed(SurfaceHolder holder)
        {
        	if (mPlayer != null)
        	{
        		release();
        	}
        	
			if(mMediaController!= null && mMediaController.isShowing()) {
				mMediaController.hide();
			}
        }
    };
    
    
    
    MediaPlayerControl mMediaPlayerControl = new MediaPlayerControl() {
		
		public void start() {
			mPlayer.resume();
		}
		
		public void seekTo(int pos) {
			//Log.d(TAG, "want seek to");
		}
		
		public void pause() {
			mPlayer.pause();
		}
		
		public boolean isPlaying() {
			return mPlayer.isPlaying();
		}
		
		public int getDuration() {
			return mPlayer.getDuration();
		}
		
		public int getCurrentPosition() {
			return mPlayer.getCurrentPosition();
		}
		
		public int getBufferPercentage() {
			//Log.d(TAG, "want buffer percentage");
			return 0;
		}

		@Override
		public boolean canPause() {
			// TODO Auto-generated method stub
			return false;
		}

		@Override
		public boolean canSeekBackward() {
			// TODO Auto-generated method stub
			return false;
		}

		@Override
		public boolean canSeekForward() {
			// TODO Auto-generated method stub
			return false;
		}
	};
}
